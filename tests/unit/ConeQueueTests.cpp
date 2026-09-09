// SPDX-License-Identifier: MIT
//
// `HOUSE-00695`. §25.2's work queue, as the fixed-capacity ring its acceptance asks for.
//
// **Tested at capacity 4, because the house cannot reach the interesting case.** The deepest
// frontier over every cell at four headings with every door open is 13 cones and a whole frame
// pushes about twenty, against a production capacity of 128 -- so no pose this building can be
// stood in ever wraps the ring or fills it. Both are exactly the arithmetic a ring gets wrong, so
// they are driven here, on a queue small enough that four pushes reach the end of the array.
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/visibility/ConeQueue.hpp"

namespace
{
    using cnahouse::visibility::ConeQueue;

    using Small = ConeQueue<int, 4>;
} // namespace

TEST(ConeQueueTests, EmptyUntilSomethingIsPushed)
{
    Small queue;
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0U);
    EXPECT_EQ(queue.Peak(), 0U);
    EXPECT_EQ(queue.Dropped(), 0U);
    EXPECT_EQ(Small::CapacityValue(), 4U);

    EXPECT_TRUE(queue.Push(7));
    EXPECT_FALSE(queue.Empty());
    EXPECT_EQ(queue.Size(), 1U);
    EXPECT_EQ(queue.Pop(), 7);
    EXPECT_TRUE(queue.Empty());
}

TEST(ConeQueueTests, FirstInFirstOutBecauseTheWalkIsBreadthFirst)
{
    // §25.2 is breadth-first so that every room is reached at the shallowest depth it can be:
    // a queue that handed back the LAST cone pushed would make it depth-first and change which
    // rooms the depth caps let through, not merely the order they arrive in.
    Small queue;
    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    ASSERT_TRUE(queue.Push(3));
    EXPECT_EQ(queue.Pop(), 1);
    EXPECT_EQ(queue.Pop(), 2);
    EXPECT_EQ(queue.Pop(), 3);
}

TEST(ConeQueueTests, ThePushAndThePopBothWrapRoundTheEndOfTheArray)
{
    // The case the house never reaches. One in, one out, forty times over a four-slot ring: every
    // push and every pop crosses the end of the array ten times, and an off-by-one in either
    // index shows up as the wrong value within the first lap.
    Small queue;
    ASSERT_TRUE(queue.Push(-2));
    ASSERT_TRUE(queue.Push(-1));
    for (int i = 0; i < 40; ++i)
    {
        ASSERT_TRUE(queue.Push(i)) << "the ring should have room: two in, two out, every lap";
        EXPECT_EQ(queue.Size(), 3U);
        const int popped = queue.Pop();
        EXPECT_EQ(popped, i - 2) << "at push " << i;
    }
    EXPECT_EQ(queue.Size(), 2U);
    EXPECT_EQ(queue.Pop(), 38);
    EXPECT_EQ(queue.Pop(), 39);
    EXPECT_TRUE(queue.Empty());
}

TEST(ConeQueueTests, FullIsARefusalAndNotAGrowth)
{
    // The other case the house never reaches, and the one §25 has to be loud about: a cone that
    // is refused is a room that may never be reached. It is counted, the queue is unchanged, and
    // -- the point of the whole exercise -- nothing is allocated to make room.
    Small queue;
    for (int i = 0; i < 4; ++i)
    {
        ASSERT_TRUE(queue.Push(i));
    }
    EXPECT_EQ(queue.Size(), 4U);
    EXPECT_FALSE(queue.Push(99));
    EXPECT_FALSE(queue.Push(100));
    EXPECT_EQ(queue.Dropped(), 2U);
    EXPECT_EQ(queue.Size(), 4U) << "a refused push must change nothing";

    // And what is in it is what was put in it, in order: a full ring that had quietly overwritten
    // its oldest entry would still be the right SIZE.
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(queue.Pop(), i);
    }
    EXPECT_TRUE(queue.Empty());

    // Room again once it has drained, which is what makes it a queue and not a buffer.
    EXPECT_TRUE(queue.Push(5));
    EXPECT_EQ(queue.Pop(), 5);
}

TEST(ConeQueueTests, ThePeakIsTheDeepestItGotAndNotWhereItEnded)
{
    // `TraversalStats::queuePeak` is what says whether the capacity is right, so it has to be a
    // high-water mark: a peak that read the CURRENT size would be zero at the end of every frame,
    // which is the one moment it is guaranteed to be.
    Small queue;
    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    ASSERT_TRUE(queue.Push(3));
    EXPECT_EQ(queue.Peak(), 3U);
    static_cast<void>(queue.Pop());
    static_cast<void>(queue.Pop());
    static_cast<void>(queue.Pop());
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Peak(), 3U) << "the peak fell back with the size";

    queue.Clear();
    EXPECT_EQ(queue.Peak(), 0U);
    EXPECT_EQ(queue.Dropped(), 0U);
    EXPECT_TRUE(queue.Empty());
}

TEST(ConeQueueTests, ClearLeavesItUsableAndNotMerelyEmpty)
{
    // `PortalTraversal::Run` clears at the top of every frame, so what `Clear` owes the next frame
    // is an empty queue, counters at zero and FIFO order from the first push -- all of which are
    // asserted here. It rewinds the head as well, and that part is deliberately NOT asserted: an
    // injection proved it unobservable, because a push starts from the same head a pop does, so a
    // ring left mid-array behaves identically. Rewinding it is for whoever reads the array in a
    // debugger, not for correctness.
    Small queue;
    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    static_cast<void>(queue.Pop());
    queue.Clear();
    ASSERT_TRUE(queue.Push(42));
    EXPECT_EQ(queue.Pop(), 42);
}
