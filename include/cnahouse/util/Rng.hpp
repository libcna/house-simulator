// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cnahouse::util
{

    /// @brief xoshiro256++, with explicit state so a stream can be saved and restored.
    ///
    /// **Why not `std::mt19937`.** The save format has to round-trip the generator's state
    /// (`HOUSE-00027`'s acceptance), and a house that reloads must produce the same weather, the same
    /// creaks and the same footstep variation it would have produced without the reload. `mt19937`'s
    /// state is 624 words and its textual round trip is locale-sensitive; xoshiro256++ is four
    /// `uint64_t`, is trivially serialisable as hex, and is faster and better-distributed than anything
    /// this project needs.
    ///
    /// **Determinism is a requirement, not a nicety.** `--seed` must reproduce a session exactly for a
    /// bug report to be actionable, so every system that needs randomness owns its own `Rng` seeded
    /// from the session seed and a per-system salt. Sharing one global stream would make one system's
    /// draw count perturb another's sequence.
    class Rng
    {
    public:
        using State = std::array<std::uint64_t, 4>;

        /// @brief Seeds from a 64-bit value through SplitMix64, as xoshiro's author specifies.
        ///
        /// Seeding the four words directly from one integer would leave the state close to all-zero
        /// for a small seed, and xoshiro recovers from that only slowly -- so `--seed 1` would produce
        /// visibly poor randomness for the first few hundred draws.
        explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept;

        explicit Rng(const State& state) noexcept
            : state_(state)
        {
        }

        [[nodiscard]] std::uint64_t NextUInt64() noexcept;

        /// @brief A uniform value in [0, 1).
        [[nodiscard]] float NextFloat() noexcept;

        /// @brief A uniform value in [@p min, @p max).
        [[nodiscard]] float NextFloat(float min, float max) noexcept;

        /// @brief A uniform integer in [@p min, @p max], inclusive at both ends.
        ///
        /// Uses Lemire's rejection method rather than a modulo, because a modulo over a range that
        /// does not divide 2^64 biases the low values -- invisibly, and exactly where a designer would
        /// later wonder why the first entry of a table comes up slightly too often.
        [[nodiscard]] std::int32_t NextInt(std::int32_t min, std::int32_t max) noexcept;

        [[nodiscard]] bool NextBool() noexcept
        {
            return (NextUInt64() >> 63) != 0u;
        }

        [[nodiscard]] const State& GetState() const noexcept
        {
            return state_;
        }

        void SetState(const State& state) noexcept
        {
            state_ = state;
        }

        /// @brief The state as 64 hex characters, for the save file.
        [[nodiscard]] std::string ToHex() const;
        /// @brief Restores from `ToHex`. Returns false and leaves the state untouched on bad input.
        [[nodiscard]] bool FromHex(std::string_view hex) noexcept;

    private:
        State state_{};
    };

    /// @brief A shuffled draw without repeats until the bag empties, then reshuffled.
    ///
    /// This is what stops the dog barking with the same sample four times running. Uniform random
    /// selection genuinely produces such runs, and a player hears them as a bug rather than as
    /// randomness, so the ambience and interaction sounds draw from a bag instead
    /// (`cna-house.md` §64). The bag also guarantees every variation is heard, which uniform selection
    /// does not.
    template<typename T>
    class Bag
    {
    public:
        Bag() = default;

        explicit Bag(std::vector<T> items)
            : items_(std::move(items))
        {
        }

        void SetItems(std::vector<T> items)
        {
            items_ = std::move(items);
            remaining_ = 0;
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return items_.empty();
        }

        [[nodiscard]] std::size_t Size() const noexcept
        {
            return items_.size();
        }

        /// @brief Draws the next item, reshuffling when the bag empties.
        ///
        /// The reshuffle keeps the item just drawn out of the first position when there is more than
        /// one item, so a bag boundary cannot produce the immediate repeat the whole type exists to
        /// prevent. `lastIndex_` is updated HERE, on the draw, not in `Refill` -- an earlier version
        /// set it inside `Refill` to the item about to be drawn first, which meant the anti-repeat
        /// check compared the new cycle against itself and let a boundary repeat through. The unit
        /// test caught it on draw 16.
        [[nodiscard]] const T& Draw(Rng& rng)
        {
            if (items_.size() == 1)
            {
                return items_.front();
            }
            if (remaining_ == 0)
            {
                Refill(rng);
            }
            --remaining_;
            lastIndex_ = order_[remaining_];
            hasLast_ = true;
            return items_[lastIndex_];
        }

    private:
        void Refill(Rng& rng)
        {
            const std::size_t count = items_.size();
            order_.resize(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                order_[i] = i;
            }
            // Fisher-Yates, drawing from the front so the LAST element of `order_` -- the one `Draw`
            // takes first -- is uniformly chosen.
            for (std::size_t i = count; i > 1; --i)
            {
                const auto j = static_cast<std::size_t>(rng.NextInt(0, static_cast<std::int32_t>(i) - 1));
                std::swap(order_[i - 1], order_[j]);
            }
            // If the new cycle's FIRST draw would repeat the item drawn last, swap it one place down.
            // One swap, and the only sequence it forbids is the one that sounds broken.
            if (hasLast_ && count > 1 && order_[count - 1] == lastIndex_)
            {
                std::swap(order_[count - 1], order_[count - 2]);
            }
            remaining_ = count;
        }

        std::vector<T> items_;
        std::vector<std::size_t> order_;
        std::size_t remaining_ = 0;
        std::size_t lastIndex_ = 0;
        bool hasLast_ = false;
    };

} // namespace cnahouse::util
