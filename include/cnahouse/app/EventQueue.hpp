// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace cnahouse::app
{

    /// @brief Typed events, published during a frame and drained once per frame in a fixed order.
    ///
    /// **Why an event queue at all**, when `ISystem` already has a fixed order. Because the fixed
    /// order says *when* a system runs, not *who told it to*. The light switch is in `interaction`;
    /// the light it turns on is in `lighting`, three stages later; the sound it makes is in `audio`,
    /// seven stages later. A direct call would run `lighting` inside `interaction`'s stage, which is
    /// exactly the ordering violation `UpdateStage` exists to prevent. A published event is delivered
    /// at the subscriber's own stage, so the order stays the order.
    ///
    /// **Why draining is per frame and not immediate.** Immediate delivery makes the frame's behaviour
    /// depend on publish order, which is the same class of bug as an unordered system list, and it
    /// makes an event published from a handler re-enter the queue mid-drain. Draining once, at a known
    /// point, keeps a frame's causality one hop deep and legible.
    ///
    /// **Why there is a per-frame cap.** A handler that publishes the event it handles is a livelock
    /// that presents as a hang, and a hang is the least diagnosable failure a game can have. The cap
    /// turns it into a bounded frame plus a diagnostic naming the event type -- which is `HOUSE-00130`'s
    /// acceptance criterion.
    ///
    /// Not thread-safe, deliberately: `cna-house` is single-threaded per §7.5.
    class EventQueue
    {
    public:
        /// @brief The most events one frame may deliver before the queue reports an overflow.
        ///
        /// Sized well above any legitimate frame -- a storm publishing a lightning strike, sixty
        /// interactables settling, the residency system requesting a pack -- and well below a livelock.
        static constexpr std::size_t kMaxEventsPerFrame = 4096;

        using HandlerId = std::uint32_t;

        /// @brief Subscribes to events of type @p TEvent.
        ///
        /// @return an id for `Unsubscribe`. Systems generally subscribe once and never unsubscribe,
        ///         but interactables come and go with a pack, so removal has to be possible.
        template<typename TEvent>
        HandlerId Subscribe(std::function<void(const TEvent&)> handler)
        {
            auto& list = handlers_[std::type_index(typeid(TEvent))];
            const HandlerId id = nextHandlerId_++;
            list.push_back(Handler{id,
                                   [handler = std::move(handler)](const void* payload)
                                   { handler(*static_cast<const TEvent*>(payload)); }});
            return id;
        }

        template<typename TEvent>
        void Unsubscribe(HandlerId id)
        {
            auto it = handlers_.find(std::type_index(typeid(TEvent)));
            if (it == handlers_.end())
            {
                return;
            }
            std::erase_if(it->second, [id](const Handler& handler) { return handler.id == id; });
        }

        /// @brief Publishes an event for delivery at the next `Drain`.
        ///
        /// The payload is copied into the queue. Events are small value types by convention -- an id,
        /// a position, a state -- because an event that owns a resource turns "who deletes it" into a
        /// question the queue would have to answer.
        template<typename TEvent>
        void Publish(TEvent event)
        {
            pending_.push_back(Pending{std::type_index(typeid(TEvent)),
                                       [event = std::move(event)](const HandlerList& list)
                                       {
                                           for (const Handler& handler : list)
                                           {
                                               handler.invoke(&event);
                                           }
                                       }});
        }

        /// @brief Delivers everything published since the last drain, in publish order.
        ///
        /// Events published *during* the drain are delivered in the same drain, up to the cap -- so a
        /// switch that opens a door that changes a portal resolves within one frame rather than
        /// arriving one frame late. The cap is what keeps that from being unbounded.
        ///
        /// @return how many events were delivered.
        std::size_t Drain();

        /// @brief Whether the last `Drain` hit the cap, and on which event type.
        [[nodiscard]] bool Overflowed() const noexcept
        {
            return overflowed_;
        }

        [[nodiscard]] const std::string& OverflowType() const noexcept
        {
            return overflowType_;
        }

        [[nodiscard]] std::size_t LastDrainCount() const noexcept
        {
            return lastDrainCount_;
        }

        [[nodiscard]] std::size_t PendingCount() const noexcept
        {
            return pending_.size();
        }

        void Clear() noexcept;

    private:
        struct Handler
        {
            HandlerId id;
            std::function<void(const void*)> invoke;
        };

        using HandlerList = std::vector<Handler>;

        struct Pending
        {
            std::type_index type;
            std::function<void(const HandlerList&)> deliver;
        };

        std::unordered_map<std::type_index, HandlerList> handlers_;
        std::vector<Pending> pending_;
        HandlerId nextHandlerId_ = 1;
        bool overflowed_ = false;
        std::string overflowType_;
        std::size_t lastDrainCount_ = 0;
    };

} // namespace cnahouse::app
