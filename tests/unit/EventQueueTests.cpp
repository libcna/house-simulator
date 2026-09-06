// SPDX-License-Identifier: MIT
//
// `HOUSE-00130`'s verification: typed events, drained once per frame in a fixed order, with a
// per-frame cap and an overflow diagnostic.
#include <gtest/gtest.h>

#include "cnahouse/app/EventQueue.hpp"
#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::app::EventQueue;

    struct LightToggled
    {
        int roomId = 0;
        bool on = false;
    };

    struct DoorOpened
    {
        int doorId = 0;
    };

    class EventQueueTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cnahouse::util::Log::ResetForTesting();
        }

        void TearDown() override
        {
            cnahouse::util::Log::ResetForTesting();
        }
    };

    TEST_F(EventQueueTest, EventsAreDeliveredToTheirOwnTypeOnly)
    {
        EventQueue queue;
        std::vector<int> lights;
        std::vector<int> doors;
        (void)queue.Subscribe<LightToggled>([&](const LightToggled& e) { lights.push_back(e.roomId); });
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened& e) { doors.push_back(e.doorId); });

        queue.Publish(LightToggled{3, true});
        queue.Publish(DoorOpened{9});
        queue.Publish(LightToggled{4, false});

        EXPECT_EQ(queue.Drain(), 3u);
        EXPECT_EQ(lights, (std::vector<int>{3, 4}));
        EXPECT_EQ(doors, (std::vector<int>{9}));
    }

    TEST_F(EventQueueTest, NothingIsDeliveredBeforeTheDrain)
    {
        // Immediate delivery would make a frame's behaviour depend on publish order and would let a
        // handler re-enter the queue mid-publish. Draining at a known point keeps causality legible.
        EventQueue queue;
        int seen = 0;
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened&) { ++seen; });
        queue.Publish(DoorOpened{1});
        EXPECT_EQ(seen, 0);
        EXPECT_EQ(queue.PendingCount(), 1u);
        (void)queue.Drain();
        EXPECT_EQ(seen, 1);
    }

    TEST_F(EventQueueTest, DeliveryIsInPublishOrder)
    {
        EventQueue queue;
        std::vector<int> order;
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened& e) { order.push_back(e.doorId); });
        for (int i = 0; i < 10; ++i)
        {
            queue.Publish(DoorOpened{i});
        }
        (void)queue.Drain();
        EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
    }

    TEST_F(EventQueueTest, EveryHandlerForATypeIsCalled)
    {
        EventQueue queue;
        int a = 0;
        int b = 0;
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened&) { ++a; });
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened&) { ++b; });
        queue.Publish(DoorOpened{1});
        (void)queue.Drain();
        EXPECT_EQ(a, 1);
        EXPECT_EQ(b, 1);
    }

    TEST_F(EventQueueTest, AnEventWithNoSubscriberIsNotAnError)
    {
        // Common and correct: a pack that is not loaded has no subscriber for its interactables'
        // events, and publishing into the void must be free rather than a warning storm.
        EventQueue queue;
        queue.Publish(DoorOpened{1});
        EXPECT_EQ(queue.Drain(), 1u);
    }

    TEST_F(EventQueueTest, UnsubscribeStopsDelivery)
    {
        EventQueue queue;
        int seen = 0;
        const auto id = queue.Subscribe<DoorOpened>([&](const DoorOpened&) { ++seen; });
        queue.Publish(DoorOpened{1});
        (void)queue.Drain();
        ASSERT_EQ(seen, 1);

        queue.Unsubscribe<DoorOpened>(id);
        queue.Publish(DoorOpened{2});
        (void)queue.Drain();
        EXPECT_EQ(seen, 1) << "an interactable removed with its pack must stop receiving";
    }

    TEST_F(EventQueueTest, AnEventPublishedByAHandlerIsDeliveredInTheSameDrain)
    {
        // A switch that opens a door that changes a portal resolves this frame rather than arriving one
        // frame late. This is why the drain is a loop over a growing list.
        EventQueue queue;
        int doors = 0;
        (void)queue.Subscribe<LightToggled>([&](const LightToggled&) { queue.Publish(DoorOpened{1}); });
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened&) { ++doors; });

        queue.Publish(LightToggled{1, true});
        EXPECT_EQ(queue.Drain(), 2u);
        EXPECT_EQ(doors, 1);
    }

    TEST_F(EventQueueTest, APublishLoopIsCappedAndDiagnosed)
    {
        // Without the cap this is a hang, and a hang is the least diagnosable failure a game can have.
        // With it, it is a bounded frame plus a message naming the event type.
        EventQueue queue;
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened& e)
                                          { queue.Publish(DoorOpened{e.doorId + 1}); });
        queue.Publish(DoorOpened{0});

        const std::size_t delivered = queue.Drain();
        EXPECT_EQ(delivered, EventQueue::kMaxEventsPerFrame);
        EXPECT_TRUE(queue.Overflowed());
        EXPECT_FALSE(queue.OverflowType().empty()) << "the diagnostic must name the event type";
    }

    TEST_F(EventQueueTest, AnOverflowDropsTheRemainderRatherThanCarryingIt)
    {
        // Carrying undelivered events would let a publish loop survive the cap and overflow again
        // forever, which turns a loud failure into a quiet one.
        EventQueue queue;
        (void)queue.Subscribe<DoorOpened>([&](const DoorOpened& e)
                                          { queue.Publish(DoorOpened{e.doorId + 1}); });
        queue.Publish(DoorOpened{0});
        (void)queue.Drain();
        ASSERT_TRUE(queue.Overflowed());
        EXPECT_EQ(queue.PendingCount(), 0u);
    }

    TEST_F(EventQueueTest, AnOrdinaryFrameIsNowhereNearTheCap)
    {
        // The cap has to be well above any legitimate frame or it becomes a source of dropped events.
        // A storm publishing a strike, sixty interactables settling and a residency request together
        // are two orders of magnitude below it.
        EventQueue queue;
        (void)queue.Subscribe<DoorOpened>([](const DoorOpened&) {});
        for (int i = 0; i < 100; ++i)
        {
            queue.Publish(DoorOpened{i});
        }
        EXPECT_EQ(queue.Drain(), 100u);
        EXPECT_FALSE(queue.Overflowed());
    }

    TEST_F(EventQueueTest, DrainCountsAreReportedForTheDebugOverlay)
    {
        EventQueue queue;
        (void)queue.Subscribe<DoorOpened>([](const DoorOpened&) {});
        queue.Publish(DoorOpened{1});
        queue.Publish(DoorOpened{2});
        (void)queue.Drain();
        EXPECT_EQ(queue.LastDrainCount(), 2u);
        (void)queue.Drain();
        EXPECT_EQ(queue.LastDrainCount(), 0u) << "an empty frame reports zero, not the previous count";
    }

} // namespace
