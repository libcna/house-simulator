// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

#include "cnahouse/util/Result.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
    class RenderTarget2D;
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::debug
{

    /// @brief Captures the back buffer to a PNG.
    ///
    /// **The capture reads the RENDER TARGET, not the window.** There is no XNA way to read the
    /// presented back buffer directly, so the frame is drawn once more into a `RenderTarget2D` of the
    /// same size and that is what is saved. Doing it that way also makes a screenshot independent of
    /// what the compositor did with the window -- no title bar, no cursor, no other window on top --
    /// which is the only way a screenshot is usable as a regression fixture (`HOUSE-00164`).
    ///
    /// **`Texture2D::SaveAsPng(filename)` is `CNAEXT` and therefore forbidden.** The stream overload
    /// `SaveAsPng(Stream*, width, height)` is plain XNA 4.0, so the file is opened with
    /// `System::IO::FileStream` and handed to that. `std::filesystem` is not used, per
    /// `cna-house.md` §8.3, which permits it only inside `SaveStore`'s desktop implementation.
    ///
    /// **The pixels are read back and re-uploaded before saving, and that is not a workaround.**
    /// Calling `SaveAsPng` on a render target directly fails with *"no CPU-side pixel data
    /// available"*: a render target's pixels live on the GPU and the const save path has no shadow
    /// copy to write out. So the capture does what the phase-1 probes proved works --
    /// `RenderTarget2D::GetData` into a `Color` array, `Texture2D::SetData` into a staging texture --
    /// and saves that. Two extra copies of one frame, once, at the moment the user asked for a file.
    class Screenshot
    {
    public:
        /// @brief Saves the contents of @p source to @p path as a PNG.
        ///
        /// @param device the device the staging texture is created on.
        /// @param source the render target holding the captured frame.
        [[nodiscard]] static util::Result<void>
        Save(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
             Microsoft::Xna::Framework::Graphics::RenderTarget2D& source,
             std::string_view path);

        /// @brief A timestamped name under @p directory, so repeated captures do not overwrite.
        [[nodiscard]] static std::string TimestampedName(std::string_view directory,
                                                         std::string_view prefix = "cna-house");
    };

} // namespace cnahouse::debug
