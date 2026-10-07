// SPDX-License-Identifier: MIT
#pragma once

#include <charconv>

namespace cnahouse::util
{

    /// @brief `std::from_chars` for floating point, on every standard library the house builds with.
    ///
    /// Apple's libc++ (LLVM 20, Xcode 26 and later) declares the floating-point `std::from_chars`
    /// overloads with `availability(strict, introduced=26.0)`, so a call is a hard error below
    /// macOS/iOS 26 -- and CNA's Apple floor is macOS 13.3. Where the standard function is usable this
    /// is exactly that function; elsewhere it reproduces its contract: no leading whitespace, no `+`,
    /// no hexadecimal, never a read at or past `last`, `ptr` naming the first character not consumed,
    /// and `value` untouched unless `ec` is success.
    [[nodiscard]] std::from_chars_result
    FromChars(const char* first, const char* last, float& value) noexcept;

    /// @brief The `double` overload of `FromChars(const char*, const char*, float&)`.
    [[nodiscard]] std::from_chars_result
    FromChars(const char* first, const char* last, double& value) noexcept;

} // namespace cnahouse::util
