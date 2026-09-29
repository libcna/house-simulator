// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace cnahouse::util
{
    /// @brief Whether @p path names content through XNA's `TitleContainer` rather than a file.
    ///
    /// `HOUSE-03033`. A relative path is a title path: beside the executable on the desktop, in the
    /// preload on the Web, in the APK's assets on Android, where no file system reaches it. An
    /// absolute path is a file -- a test's fixture directory, or the desktop content root `Main`
    /// puts beside the binary.
    [[nodiscard]] bool IsTitlePath(std::string_view path) noexcept;

    /// @brief The whole of @p path's content, read as `IsTitlePath` says.
    [[nodiscard]] Result<std::vector<std::uint8_t>> ReadContentBytes(std::string_view path);

    /// @brief @p path's content as text, read as `IsTitlePath` says.
    [[nodiscard]] Result<std::string> ReadContentText(std::string_view path);

    /// @brief Whether @p path's content exists, asked as `IsTitlePath` says.
    [[nodiscard]] bool ContentExists(std::string_view path);
} // namespace cnahouse::util
