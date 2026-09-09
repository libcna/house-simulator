// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>

namespace cnahouse::visibility
{

    /// @brief §25.2's work queue: a fixed-capacity ring, breadth-first, that never allocates
    ///        (`HOUSE-00695`).
    ///
    /// **Why a type of its own rather than three members of `PortalTraversal`.** The interesting
    /// half of a ring is the wrap, and no pose in this house reaches it: the deepest frontier
    /// measured over every cell at four headings with every door open is 13 cones and the walk
    /// pushes about twenty in a whole frame, against a capacity of 128. Inlined into the traversal
    /// that arithmetic would be code no test could run. Here it is a template, and
    /// `ConeQueueTests` drives a `ConeQueue<int, 4>` round the wrap dozens of times.
    ///
    /// **Full is a refusal, never a growth.** A `std::vector` that keeps its capacity allocates on
    /// the frame that first needs more -- the frame with the most to do, and so the frame least
    /// able to pay for it. This cannot: `Push` returns false and the caller counts it, which is
    /// how §25's one loud failure mode -- a cone that was never expanded -- stays visible.
    template<typename T, std::size_t Capacity>
    class ConeQueue
    {
    public:
        static_assert(Capacity > 0, "a queue with no room is not a queue");

        [[nodiscard]] static constexpr std::size_t CapacityValue() noexcept
        {
            return Capacity;
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return count_ == 0;
        }

        [[nodiscard]] std::size_t Size() const noexcept
        {
            return count_;
        }

        /// @brief The most entries this queue has held since the last `Clear`.
        [[nodiscard]] std::size_t Peak() const noexcept
        {
            return peak_;
        }

        /// @brief Entries `Push` refused for want of room since the last `Clear`.
        [[nodiscard]] std::size_t Dropped() const noexcept
        {
            return dropped_;
        }

        /// @brief Empties the queue and resets its counters. Keeps the storage, which is the
        ///        object's own array and was never anywhere else.
        void Clear() noexcept
        {
            head_ = 0;
            count_ = 0;
            peak_ = 0;
            dropped_ = 0;
        }

        /// @brief Appends @p value. False when full, and then nothing changed but `Dropped`.
        bool Push(const T& value)
        {
            if (count_ == Capacity)
            {
                ++dropped_;
                return false;
            }
            std::size_t slot = head_ + count_;
            if (slot >= Capacity)
            {
                slot -= Capacity;
            }
            storage_[slot] = value;
            ++count_;
            peak_ = count_ > peak_ ? count_ : peak_;
            return true;
        }

        /// @brief Removes and returns the oldest entry.
        ///
        /// **By value**, because the slot is free the moment it is popped and the very next `Push`
        /// -- which the caller makes while expanding this cone -- may write over it.
        /// @pre The queue is not empty.
        [[nodiscard]] T Pop()
        {
            T value = storage_[head_];
            ++head_;
            if (head_ == Capacity)
            {
                head_ = 0;
            }
            --count_;
            return value;
        }

    private:
        std::array<T, Capacity> storage_{};
        std::size_t head_ = 0;
        std::size_t count_ = 0;
        std::size_t peak_ = 0;
        std::size_t dropped_ = 0;
    };

} // namespace cnahouse::visibility
