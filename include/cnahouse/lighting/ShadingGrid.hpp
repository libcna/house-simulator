// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::lighting
{

    /// @brief §22's grid for one window: how much of it the sun reaches, from every direction.
    ///
    /// **Occlusion only.** Whether the sun is up at all, and whether it is within §28.4's ±75° of
    /// the window's normal, are the analytic `SkyExposure` and are deliberately not baked here —
    /// so the grid stays a pure property of the geometry and survives a change to that function.
    struct WindowShading
    {
        util::Id window;
        /// @brief The interior cell the window lights.
        util::Id cell;
        /// @brief The outward normal, which §28.4's ±75° test needs.
        Microsoft::Xna::Framework::Vector3 normal;
        /// @brief `altitudeSteps × azimuthSteps` bytes, altitude-major, `round(fraction × 255)`.
        std::vector<std::uint8_t> grid;
    };

    /// @brief `shading.bin`, whole. The format is `docs/shading-format.md`.
    ///
    /// Read with `System::IO::BinaryReader` and nothing else, so it parses with no graphics device
    /// and its tests need none (ADR-0001).
    class ShadingGrid
    {
    public:
        /// @brief The 4-byte magic, `CSHF`.
        static constexpr std::uint32_t kMagic = 0x46485343u; // 'C','S','H','F' little-endian
        static constexpr std::uint32_t kVersion = 1u;
        /// @brief §22's grid, and the only shape this reader accepts. A file that disagrees is
        ///        refused rather than interpolated over: the node spacing is baked into the lookup.
        static constexpr std::uint32_t kAltitudeSteps = 12u;
        static constexpr std::uint32_t kAzimuthSteps = 24u;
        /// @brief A name longer than this is a corrupt length field, not a name.
        static constexpr std::uint32_t kMaxNameBytes = 1024u;
        /// @brief §12.6 schedules 64 windows. A file claiming more than this is corrupt.
        static constexpr std::uint32_t kMaxWindows = 65536u;

        [[nodiscard]] static util::Result<ShadingGrid> Read(System::IO::Stream& stream,
                                                            std::string_view name);

        /// @brief Opens @p contentPath through `TitleContainer` and reads it.
        [[nodiscard]] static util::Result<ShadingGrid> ReadFromTitle(std::string_view contentPath);

        /// @brief An empty grid: every window unshaded.
        ///
        /// **What a build with no `shading.bin` gets, and it is a decision.** The alternative is to
        /// refuse to light the house at all, and a missing bake is a content-pipeline state rather
        /// than a corrupt one — `HOUSE-01279`'s stage skips on a checkout with no Blender. An
        /// unshaded window is *wrong in a direction a person can see* (the foyer gets sun the porch
        /// should have stopped), which is what makes it safe: it fails visibly rather than dark.
        [[nodiscard]] static ShadingGrid Unshaded() noexcept
        {
            return ShadingGrid{};
        }

        /// @brief How much of @p window the sun reaches at (@p altitudeDeg, @p azimuthDeg), `[0, 1]`.
        ///
        /// Bilinear between the four surrounding nodes. **The azimuth axis wraps** — node 23 is
        /// 345° and its neighbour is node 0 at 360° = 0°, with no seam — and the altitude axis
        /// clamps, because 0° and 90° are measured rather than extrapolated (`HOUSE-00207` sampled
        /// nodes and not cell centres exactly so this is exact at the ends).
        ///
        /// A window this file does not carry returns **1.0**, unshaded, for the reason `Unshaded`
        /// gives. `Contains` is how a caller that wants to know asks.
        [[nodiscard]] float Factor(util::Id window, double altitudeDeg, double azimuthDeg) const noexcept;

        /// @brief The fraction of the sky the window can actually see, `[0, 1]`.
        ///
        /// **`Factor` is a SUN-DIRECTION mask and is the wrong multiplier for a diffuse term.**
        /// `shading_factor.py` stores 0 for every direction behind the window's own wall — half
        /// the grid, short-circuited without casting a ray — so a north window's factor is 0
        /// whenever the sun is in the south, which at 40° N is all day. §28.4's formula multiplies
        /// the whole of `skyExposure` by it, and taken literally that makes every north-facing
        /// room pitch dark from dawn to dusk. `HOUSE-01263` measured it: `L0_FAMILY` read exactly
        /// 0.000 at a 45° sun due south.
        ///
        /// A diffuse sky term is not directional in that way: an eave takes a share of the sky
        /// DOME, not all of it. This is that share, computed once from the same baked grid — the
        /// irradiance-weighted mean of the grid over the hemisphere the window faces, with each
        /// node weighted by `cos(altitude)` for the grid's own over-sampling of the zenith and by
        /// the cosine of incidence on the pane. No new data and no new file: the occlusion the
        /// bake already measured, integrated instead of sampled.
        ///
        /// A window this file does not carry returns **1.0**, for the reason `Unshaded` gives.
        [[nodiscard]] float SkyViewFactor(util::Id window) const noexcept;

        [[nodiscard]] bool Contains(util::Id window) const noexcept;

        [[nodiscard]] const WindowShading* Find(util::Id window) const noexcept;

        [[nodiscard]] std::size_t WindowCount() const noexcept
        {
            return windows_.size();
        }

        [[nodiscard]] const std::vector<WindowShading>& Windows() const noexcept
        {
            return windows_;
        }

        /// @brief Rays per axis across the window the grid was measured with. Provenance, not maths.
        [[nodiscard]] std::uint32_t Samples() const noexcept
        {
            return samples_;
        }

    private:
        std::vector<WindowShading> windows_;
        /// @brief `SkyViewFactor`, one per window, computed once when the file is read.
        std::vector<float> skyView_;
        std::unordered_map<std::uint32_t, std::size_t> index_;
        std::uint32_t samples_ = 0;
    };

} // namespace cnahouse::lighting
