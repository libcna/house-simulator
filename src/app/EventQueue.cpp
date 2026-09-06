// SPDX-License-Identifier: MIT
#include "cnahouse/app/EventQueue.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::app
{

    std::size_t EventQueue::Drain()
    {
        overflowed_ = false;
        overflowType_.clear();
        lastDrainCount_ = 0;

        static const HandlerList kNoHandlers;

        // Indexed rather than iterated: a handler may publish, which appends to `pending_` and would
        // invalidate an iterator. Delivering what is appended in the SAME drain is deliberate -- a
        // switch that opens a door that changes a portal resolves this frame instead of next -- and is
        // exactly why the cap has to exist.
        std::size_t index = 0;
        while (index < pending_.size())
        {
            if (lastDrainCount_ >= kMaxEventsPerFrame)
            {
                overflowed_ = true;
                overflowType_ = pending_[index].type.name();
                util::Log::Error(util::LogCat::App,
                                 "event queue overflowed at {} events in one frame; the next undelivered "
                                 "event is '{}'. This is a publish loop, not a busy frame: the cap is "
                                 "well above any legitimate frame.",
                                 kMaxEventsPerFrame,
                                 overflowType_);
                break;
            }
            const Pending& event = pending_[index];
            const auto handlers = handlers_.find(event.type);
            event.deliver(handlers == handlers_.end() ? kNoHandlers : handlers->second);
            ++lastDrainCount_;
            ++index;
        }

        // Everything is dropped, delivered or not. Carrying undelivered events into the next frame
        // would let a publish loop survive the cap and simply overflow again forever, which turns a
        // loud failure into a quiet one.
        pending_.clear();
        return lastDrainCount_;
    }

    void EventQueue::Clear() noexcept
    {
        pending_.clear();
        overflowed_ = false;
        overflowType_.clear();
        lastDrainCount_ = 0;
    }

} // namespace cnahouse::app
