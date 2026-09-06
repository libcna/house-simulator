// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "cnahouse/animation/Animation.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::anim
{

    /// @brief Reads a `.chanim` sidecar. The format is `docs/anim-format.md`.
    ///
    /// **Nothing here is graphics.** `TitleContainer::OpenStream` and `System::IO::BinaryReader` are
    /// plain XNA 4.0 / .NET API, which is what lets this project own its animation data without
    /// naming CNA's `CNAEXT` `Graphics::SkinningData`, `AnimationClip` or `Keyframe` (ADR-0001,
    /// `cna-house.md` §47.0).
    ///
    /// **Every failure is a `util::Result` that names the file and what was wrong with it**, per
    /// `docs/conventions.md` §5.4. There is no fallback skeleton and there must not be: a missing
    /// texture can be substituted with grey, but a character whose skeleton did not load cannot be
    /// drawn at all, and a substitute would be a silently wrong deformation.
    class ChanimReader
    {
    public:
        /// @brief The 4-byte magic, `CHAN`.
        static constexpr std::uint32_t kMagic = 0x4E414843u; // 'C','H','A','N' little-endian
        /// @brief The only version this reader accepts.
        static constexpr std::uint32_t kVersion = 1u;
        /// @brief `Byte4` blend indices cannot address more (`HOUSE-00074`).
        ///
        /// NOT 72. `SkinnedEffect::MaxBones` limits what one DRAW may use, not what a skeleton may
        /// contain; that check belongs at the draw and `rendering::MaterialBinder` makes it there.
        static constexpr std::uint32_t kMaxBones = 256u;
        static constexpr std::uint32_t kMaxClips = 4096u;
        static constexpr std::uint32_t kMaxKeysPerClip = 1048576u;
        static constexpr std::uint32_t kMaxFootPlants = 64u;
        /// @brief A joint name longer than this is a corrupt length field, not a name.
        static constexpr std::uint32_t kMaxNameBytes = 1024u;

        /// @brief Reads from @p stream. @p name is used only in error messages.
        [[nodiscard]] static util::Result<ClipLibrary> Read(System::IO::Stream& stream,
                                                            std::string_view name);

        /// @brief Opens @p contentPath through `TitleContainer` and reads it.
        ///
        /// @param contentPath a path relative to the working directory, e.g.
        ///        `content/Anim/body_f.chanim`.
        [[nodiscard]] static util::Result<ClipLibrary> ReadFromTitle(std::string_view contentPath);
    };

} // namespace cnahouse::anim
