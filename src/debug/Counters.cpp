// SPDX-License-Identifier: MIT
#include "cnahouse/debug/Counters.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

namespace cnahouse::debug
{

    std::int64_t Counter::Min() const noexcept
    {
        if (count_ == 0)
        {
            return 0;
        }
        return *std::min_element(window_.begin(), window_.begin() + static_cast<std::ptrdiff_t>(count_));
    }

    std::int64_t Counter::Max() const noexcept
    {
        if (count_ == 0)
        {
            return 0;
        }
        return *std::max_element(window_.begin(), window_.begin() + static_cast<std::ptrdiff_t>(count_));
    }

    double Counter::Average() const noexcept
    {
        if (count_ == 0)
        {
            return 0.0;
        }
        const std::int64_t total = std::accumulate(
            window_.begin(), window_.begin() + static_cast<std::ptrdiff_t>(count_), std::int64_t{0});
        return static_cast<double>(total) / static_cast<double>(count_);
    }

    void Counter::Commit() noexcept
    {
        window_[next_] = current;
        next_ = (next_ + 1) % kWindow;
        count_ = std::min(count_ + 1, kWindow);
        current = 0;
    }

    void Counter::Reset() noexcept
    {
        window_.fill(0);
        next_ = 0;
        count_ = 0;
        current = 0;
    }

    Counters::Handle Counters::Resolve(std::string_view name)
    {
        for (std::size_t i = 0; i < counters_.size(); ++i)
        {
            if (counters_[i].name == name)
            {
                return i;
            }
        }
        Counter counter;
        counter.name = std::string(name);
        counters_.push_back(std::move(counter));
        return counters_.size() - 1;
    }

    void Counters::Add(Handle handle, std::int64_t amount) noexcept
    {
        if (handle < counters_.size())
        {
            counters_[handle].current += amount;
        }
    }

    void Counters::Set(Handle handle, std::int64_t value) noexcept
    {
        if (handle < counters_.size())
        {
            counters_[handle].current = value;
        }
    }

    void Counters::BeginFrame() noexcept
    {
        for (Counter& counter : counters_)
        {
            counter.Commit();
        }
    }

    const Counter* Counters::Find(std::string_view name) const
    {
        for (const Counter& counter : counters_)
        {
            if (counter.name == name)
            {
                return &counter;
            }
        }
        return nullptr;
    }

    void Counters::Reset() noexcept
    {
        for (Counter& counter : counters_)
        {
            counter.Reset();
        }
    }

} // namespace cnahouse::debug
