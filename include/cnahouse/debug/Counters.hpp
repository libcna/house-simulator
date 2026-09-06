// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cnahouse::debug
{

    /// @brief One named per-frame quantity, with min, average and max over a rolling window.
    ///
    /// **Why a window rather than an instantaneous value.** A counter read once per frame answers "what
    /// is it now", which is the least useful question: draw calls spike when a room comes into view,
    /// visible cells spike at a doorway, and the number a person happens to be looking at is whichever
    /// frame their eye landed on. The window answers "what does this cost, and how badly does it
    /// spike", which is what a budget is written against.
    ///
    /// **240 frames**, which is four seconds at 60 Hz -- long enough that walking through a doorway is
    /// entirely inside the window, short enough that the numbers still respond while someone is moving
    /// around looking for the spike.
    struct Counter
    {
        static constexpr std::size_t kWindow = 240;

        std::string name;
        /// @brief This frame's value. Reset to zero by `BeginFrame`.
        std::int64_t current = 0;

        [[nodiscard]] std::int64_t Min() const noexcept;
        [[nodiscard]] std::int64_t Max() const noexcept;
        [[nodiscard]] double Average() const noexcept;

        [[nodiscard]] std::size_t SampleCount() const noexcept
        {
            return count_;
        }

        /// @brief Closes the frame: pushes `current` into the window and zeroes it.
        void Commit() noexcept;
        void Reset() noexcept;

    private:
        std::array<std::int64_t, kWindow> window_{};
        std::size_t next_ = 0;
        std::size_t count_ = 0;
    };

    /// @brief The named counters, looked up once and then incremented by handle.
    ///
    /// Lookup by string every frame would put a hash of a string literal on a per-frame path for no
    /// reason. A system resolves its handle once, at construction, and the per-frame cost is an array
    /// index.
    class Counters
    {
    public:
        using Handle = std::size_t;
        static constexpr Handle kInvalid = static_cast<Handle>(-1);

        /// @brief Finds or creates the counter named @p name.
        [[nodiscard]] Handle Resolve(std::string_view name);

        void Add(Handle handle, std::int64_t amount) noexcept;
        void Set(Handle handle, std::int64_t value) noexcept;

        void Increment(Handle handle) noexcept
        {
            Add(handle, 1);
        }

        /// @brief Commits every counter and zeroes it for the next frame.
        void BeginFrame() noexcept;

        [[nodiscard]] const std::vector<Counter>& All() const noexcept
        {
            return counters_;
        }

        [[nodiscard]] const Counter* Find(std::string_view name) const;

        void Reset() noexcept;

    private:
        std::vector<Counter> counters_;
    };

} // namespace cnahouse::debug
