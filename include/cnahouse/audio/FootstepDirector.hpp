// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/util/Rng.hpp"

namespace cnahouse::audio
{

    /// @brief One fixed-step sample of grounded player travel.
    ///
    /// The controller already knows all of these facts. Keeping collision and stair discovery out
    /// of the audio layer avoids a second ground probe and a second interpretation of the house.
    struct FootstepStep
    {
        std::string_view surface;
        float distanceMeters = 0.0F;
        float verticalDistanceMeters = 0.0F;
        float riserHeightMeters = 0.0F;
        bool onGround = false;
        bool fastWalk = false;

        [[nodiscard]] bool OnStairs() const noexcept
        {
            return riserHeightMeters > 0.0F;
        }
    };

    /// @brief A sound selection ready for the existing sound cache to play.
    struct Footstep
    {
        std::string_view sample;
        util::Id bank;
        float pitch = 0.0F;
        /// @brief Bank gain with the per-step ±10 % variation, before the category/master mix.
        float volume = 1.0F;
    };

    /// @brief M8's distance cadence, surface lookup and deterministic sample variation.
    ///
    /// `Advance` consumes one 120 Hz player step. At the retained walk speeds that step cannot
    /// cross two ordinary strides or two risers, so one optional event is the complete answer and
    /// no per-step allocation is needed.
    class FootstepDirector
    {
    public:
        inline static constexpr float kWalkStrideMeters = 0.75F;
        inline static constexpr float kFastStrideMeters = 0.95F;

        explicit FootstepDirector(const AudioSystem& audio,
                                  std::uint64_t seed = 0xF00757E05EED1234ULL) noexcept;

        /// @brief Maps one collision surface spelling to a resolved audio bank.
        void BindSurface(std::string surface, util::Id bank);

        /// @brief Replaces the surface map from the authored bank rows.
        void BindBanks(std::span<const world::AudioBank> banks);

        /// @brief Advances cadence and returns the sound for a foot plant, if one occurred.
        [[nodiscard]] std::optional<Footstep> Advance(const FootstepStep& step);

        /// @brief Drops cadence phase, for a teleport or a newly loaded walk.
        void Reset() noexcept;

    private:
        [[nodiscard]] std::optional<Footstep> Select(util::Id bank);

        struct SurfaceBinding
        {
            std::string surface;
            util::Id bank;
        };

        const AudioSystem& audio_;
        util::Rng rng_;
        std::vector<SurfaceBinding> surfaceBanks_;
        std::unordered_map<util::Id, std::size_t> nextSamples_;
        float strideDistance_ = 0.0F;
        float stairHeight_ = 0.0F;
        bool wasOnStairs_ = false;
    };

} // namespace cnahouse::audio
