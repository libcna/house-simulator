// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/util/Result.hpp"

#include "render/ImageCompare.hpp"

namespace cnahouse::testsupport
{

    /// @brief Renders one frame of the real game to a PNG and reads it back.
    ///
    /// **It goes through `--screenshot`, the production capture path, on purpose.** A harness that
    /// rendered its own frame would be testing a second renderer that the player never sees, and the
    /// first thing to drift would be the harness. This drives `CnaHouseGame` exactly as a script or
    /// a bug reporter would, and what it compares is what a screenshot contains.
    ///
    /// The frame is deterministic by construction: a fixed back-buffer size, vsync off, the game
    /// stopped after exactly one drawn frame, and no input — so the only thing that can vary between
    /// two runs is the rasteriser.
    class RenderHarness
    {
    public:
        /// @brief Runs the game once and writes @p pngPath.
        [[nodiscard]] static util::Result<void>
        CaptureFrame(app::Options options, int width, int height, const std::string& pngPath);

        /// @brief Decodes a PNG through `Texture2D::FromStream` -- plain XNA 4.0.
        ///
        /// Needs a live device, so it runs inside its own short-lived `Game`. That is also why
        /// decoding is a separate step from capturing: the capture's device is gone by then.
        [[nodiscard]] static util::Result<Image> LoadPng(const std::string& pngPath);

        /// @brief Captures a frame and decodes both it and @p referencePath in one device session.
        [[nodiscard]] static util::Result<ImageDiff> CompareWithReference(app::Options options,
                                                                          int width,
                                                                          int height,
                                                                          const std::string& pngPath,
                                                                          const std::string& referencePath,
                                                                          int channelTolerance,
                                                                          const std::vector<Region>& ignore);

        /// @brief Writes a magenta diagnostic PNG for an already-captured comparison failure.
        [[nodiscard]] static util::Result<void> WriteDifferenceImage(const std::string& actualPath,
                                                                     const std::string& referencePath,
                                                                     const std::string& differencePath,
                                                                     int channelTolerance,
                                                                     const std::vector<Region>& ignore);

        /// @brief The corner frame-time readout, which genuinely differs between two runs.
        ///
        /// Scaled from the 1600x900 design canvas of §67.2, generously, because the text's width
        /// depends on the numbers in it and a mask that clipped one digit would be worse than none.
        [[nodiscard]] static std::vector<Region> NonDeterministicRegions(int width, int height);

        /// @brief Where the committed reference frames live, relative to the repository root.
        [[nodiscard]] static std::string ReferenceDirectory();

        /// @brief Whether this process is rendering through a software rasteriser.
        ///
        /// The reference frames are generated under one (`LIBGL_ALWAYS_SOFTWARE=1`), which is what
        /// CI uses (`HOUSE-00138`), because that is the only rasteriser whose output is reproducible
        /// on a machine nobody owns. A hardware run compares coverage rather than pixels and says so.
        [[nodiscard]] static bool RenderingInSoftware();
    };

} // namespace cnahouse::testsupport
