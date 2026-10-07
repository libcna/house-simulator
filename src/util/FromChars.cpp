// SPDX-License-Identifier: MIT
#include "cnahouse/util/FromChars.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <new>
#include <string>
#include <type_traits>

// <charconv> (included above) is what defines this macro on libc++; it is 0 exactly when the
// floating-point overloads are declared but unavailable at the deployment target.
#if defined(_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT) &&                                           \
    !_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT
#define CNAHOUSE_FROM_CHARS_FLOAT_FALLBACK 1
#else
#define CNAHOUSE_FROM_CHARS_FLOAT_FALLBACK 0
#endif

namespace cnahouse::util
{
    namespace
    {
#if CNAHOUSE_FROM_CHARS_FLOAT_FALLBACK
        template<class T>
        std::from_chars_result ParseWithStrtod(const char* first, const char* last, T& value) noexcept
        {
            // `std::from_chars`' grammar is narrower than strtod's: it skips no whitespace and takes
            // no '+'. Refuse both before strtod can accept them.
            if (first == last || *first == '+' || std::isspace(static_cast<unsigned char>(*first)) != 0)
            {
                return {first, std::errc::invalid_argument};
            }

            // strtod reads C99 hexadecimal; `std::from_chars` (chars_format::general) reads the leading
            // zero and stops at the 'x'. Cutting the copy after that zero gives the same answer.
            auto length = static_cast<std::size_t>(last - first);
            const std::size_t digit = *first == '-' ? 1 : 0;
            if (length > digit + 1 && first[digit] == '0' &&
                (first[digit + 1] == 'x' || first[digit + 1] == 'X'))
            {
                length = digit + 1;
            }

            // strtod has no end pointer and reads to a NUL, so it parses a terminated copy of the
            // caller's range rather than the range itself.
            std::string copy;
            try
            {
                copy.assign(first, length);
            }
            catch (const std::bad_alloc&)
            {
                return {first, std::errc::not_enough_memory};
            }

            errno = 0;
            char* end = nullptr;
            T parsed{};
            if constexpr (std::is_same_v<T, float>)
            {
                parsed = std::strtof(copy.c_str(), &end);
            }
            else
            {
                parsed = std::strtod(copy.c_str(), &end);
            }
            const auto used = static_cast<std::size_t>(end - copy.c_str());
            if (used == 0)
            {
                return {first, std::errc::invalid_argument};
            }
            // ERANGE also reports a subnormal result, which `std::from_chars` returns as a value.
            if (errno == ERANGE && (std::isinf(parsed) || parsed == T{}))
            {
                return {first + used, std::errc::result_out_of_range};
            }
            value = parsed;
            return {first + used, std::errc{}};
        }
#endif
    } // namespace

    std::from_chars_result FromChars(const char* first, const char* last, float& value) noexcept
    {
#if CNAHOUSE_FROM_CHARS_FLOAT_FALLBACK
        return ParseWithStrtod(first, last, value);
#else
        return std::from_chars(first, last, value);
#endif
    }

    std::from_chars_result FromChars(const char* first, const char* last, double& value) noexcept
    {
#if CNAHOUSE_FROM_CHARS_FLOAT_FALLBACK
        return ParseWithStrtod(first, last, value);
#else
        return std::from_chars(first, last, value);
#endif
    }
} // namespace cnahouse::util
