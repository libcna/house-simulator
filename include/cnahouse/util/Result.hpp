// SPDX-License-Identifier: MS-PL
#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

/// @file
/// The recoverable-failure half of the error-handling policy (`docs/conventions.md` section 5).
///
/// `Result<T>` is the default for anything a caller can reasonably handle: a world file that does
/// not parse, a save whose checksum is wrong, an expression with an unknown token. Exceptions are
/// reserved for the `Game` boundary and for genuinely exceptional content failures; `assert` is
/// for invariants an earlier stage already guaranteed.
///
/// The type is deliberately small. It carries a value or an error, it cannot be ignored, and its
/// error names what was expected and where. It is not a monad library.

namespace cnahouse::util
{
    /// A stable, serialisable classification of a recoverable failure.
    ///
    /// Codes are for the program: they are what a caller switches on, what a test asserts, and
    /// what a log line records alongside the human message. They are never shown to a player on
    /// their own.
    enum class ErrorCode : std::uint16_t
    {
        None = 0,
        Unknown,
        /// A named thing does not exist: a file, a cell id, a content name, an asset row.
        NotFound,
        /// A caller passed something the function cannot accept. Usually a programming error.
        InvalidArgument,
        /// Data exists and is readable, but it is wrong: a malformed JSON value, a portal
        /// rectangle that is not in its plane, a negative range.
        InvalidData,
        /// The `"schema"` header is absent or names a kind this reader does not handle.
        SchemaMismatch,
        /// The schema kind matches but the version does not, and no migration covers it.
        VersionMismatch,
        /// A checksum or hash did not match the payload it covers.
        ChecksumMismatch,
        /// A numeric value is outside the range the format or the design permits.
        OutOfRange,
        /// An id, key or name appeared twice where uniqueness is required.
        Duplicate,
        /// The operation is meaningful but this build or platform profile cannot perform it.
        Unsupported,
        /// The underlying storage failed: a read, a write, a rename, a flush.
        IoFailure,
        /// `ContentManager` could not produce the asset.
        ContentLoadFailure,
        /// The operation was abandoned deliberately, not because anything failed.
        Cancelled,
    };

    /// The stable spelling of a code, for logs and test assertions.
    [[nodiscard]] constexpr std::string_view ToStringView(ErrorCode code) noexcept
    {
        switch (code)
        {
            case ErrorCode::None:
                return "None";
            case ErrorCode::Unknown:
                return "Unknown";
            case ErrorCode::NotFound:
                return "NotFound";
            case ErrorCode::InvalidArgument:
                return "InvalidArgument";
            case ErrorCode::InvalidData:
                return "InvalidData";
            case ErrorCode::SchemaMismatch:
                return "SchemaMismatch";
            case ErrorCode::VersionMismatch:
                return "VersionMismatch";
            case ErrorCode::ChecksumMismatch:
                return "ChecksumMismatch";
            case ErrorCode::OutOfRange:
                return "OutOfRange";
            case ErrorCode::Duplicate:
                return "Duplicate";
            case ErrorCode::Unsupported:
                return "Unsupported";
            case ErrorCode::IoFailure:
                return "IoFailure";
            case ErrorCode::ContentLoadFailure:
                return "ContentLoadFailure";
            case ErrorCode::Cancelled:
                return "Cancelled";
        }
        return "Unknown";
    }

    /// A recoverable failure: what went wrong, in words, and where.
    ///
    /// `Context()` is the "where" — a file name, a JSON path, an id — and it accumulates from the
    /// inside out as an error travels up: the JSON reader adds `cells[41].boxes[0].x`, the world
    /// loader adds `layout.cells.json`. The result reads like a path, which is what makes an
    /// authoring mistake findable without a debugger.
    class Error
    {
    public:
        Error() = default;

        Error(ErrorCode code, std::string message)
            : m_code(code)
            , m_message(std::move(message))
        {
        }

        Error(ErrorCode code, std::string message, std::string context)
            : m_code(code)
            , m_message(std::move(message))
            , m_context(std::move(context))
        {
        }

        [[nodiscard]] ErrorCode Code() const noexcept
        {
            return m_code;
        }

        [[nodiscard]] const std::string& Message() const noexcept
        {
            return m_message;
        }

        /// Where the failure happened, outermost first, joined by `/`. Empty when unknown.
        [[nodiscard]] const std::string& Context() const noexcept
        {
            return m_context;
        }

        /// Returns a copy with @p outer prepended to the context.
        [[nodiscard]] Error WithContext(std::string_view outer) const
        {
            std::string combined(outer);
            if (!m_context.empty())
            {
                combined += '/';
                combined += m_context;
            }
            return Error(m_code, m_message, std::move(combined));
        }

        /// `ErrorCode [context]: message`, the form used by every log sink and test failure.
        [[nodiscard]] std::string ToString() const
        {
            std::string text(ToStringView(m_code));
            if (!m_context.empty())
            {
                text += " [";
                text += m_context;
                text += ']';
            }
            text += ": ";
            text += m_message;
            return text;
        }

        [[nodiscard]] friend bool operator==(const Error& left, const Error& right) noexcept
        {
            return left.m_code == right.m_code && left.m_message == right.m_message &&
                   left.m_context == right.m_context;
        }

    private:
        ErrorCode m_code = ErrorCode::Unknown;
        std::string m_message;
        std::string m_context;
    };

    /// A value or an `Error`, never both, never neither.
    ///
    /// `[[nodiscard]]` is the point of the type: a caller who ignores a `Result` gets a warning,
    /// and warnings are errors in this project, so a failure cannot be dropped silently.
    template<typename T>
    class [[nodiscard]] Result
    {
        static_assert(!std::is_reference_v<T>, "Result<T&> is not supported; return a pointer.");
        static_assert(!std::is_same_v<std::remove_cv_t<T>, ::cnahouse::util::Error>,
                      "Result<Error> is ambiguous by construction.");

    public:
        using ValueType = T;

        Result(T value)
            : m_state(std::in_place_index<0>, std::move(value))
        {
        }

        Result(::cnahouse::util::Error error)
            : m_state(std::in_place_index<1>, std::move(error))
        {
        }

        [[nodiscard]] bool HasValue() const noexcept
        {
            return m_state.index() == 0;
        }

        explicit operator bool() const noexcept
        {
            return HasValue();
        }

        /// Precondition: `HasValue()`. Checked by `assert`, because reaching here without
        /// testing is a programming error, not a data error.
        [[nodiscard]] T& Value() & noexcept
        {
            assert(HasValue() && "Result::Value() on an error result");
            return std::get<0>(m_state);
        }

        [[nodiscard]] const T& Value() const& noexcept
        {
            assert(HasValue() && "Result::Value() on an error result");
            return std::get<0>(m_state);
        }

        [[nodiscard]] T&& Value() && noexcept
        {
            assert(HasValue() && "Result::Value() on an error result");
            return std::get<0>(std::move(m_state));
        }

        [[nodiscard]] T ValueOr(T fallback) const&
        {
            return HasValue() ? std::get<0>(m_state) : std::move(fallback);
        }

        /// Precondition: `!HasValue()`.
        [[nodiscard]] const ::cnahouse::util::Error& Error() const noexcept
        {
            assert(!HasValue() && "Result::Error() on a value result");
            return std::get<1>(m_state);
        }

        [[nodiscard]] T& operator*() & noexcept
        {
            return Value();
        }

        [[nodiscard]] const T& operator*() const& noexcept
        {
            return Value();
        }

        [[nodiscard]] T* operator->() noexcept
        {
            assert(HasValue() && "Result::operator-> on an error result");
            return &std::get<0>(m_state);
        }

        [[nodiscard]] const T* operator->() const noexcept
        {
            assert(HasValue() && "Result::operator-> on an error result");
            return &std::get<0>(m_state);
        }

        /// Returns a copy of the error with @p outer prepended to its context; the value case is
        /// returned unchanged. This is how a caller adds "which file" to an inner failure.
        [[nodiscard]] Result WithContext(std::string_view outer) const&
        {
            if (HasValue())
            {
                return *this;
            }
            return Result(std::get<1>(m_state).WithContext(outer));
        }

    private:
        std::variant<T, ::cnahouse::util::Error> m_state;
    };

    /// The no-value case: an operation that either succeeded or failed for a stated reason.
    template<>
    class [[nodiscard]] Result<void>
    {
    public:
        using ValueType = void;

        Result() = default;

        Result(::cnahouse::util::Error error)
            : m_error(std::move(error))
            , m_hasError(true)
        {
        }

        [[nodiscard]] bool HasValue() const noexcept
        {
            return !m_hasError;
        }

        explicit operator bool() const noexcept
        {
            return HasValue();
        }

        /// Precondition: `!HasValue()`.
        [[nodiscard]] const ::cnahouse::util::Error& Error() const noexcept
        {
            assert(m_hasError && "Result<void>::Error() on a success result");
            return m_error;
        }

        [[nodiscard]] Result WithContext(std::string_view outer) const
        {
            if (!m_hasError)
            {
                return Result();
            }
            return Result(m_error.WithContext(outer));
        }

    private:
        ::cnahouse::util::Error m_error;
        bool m_hasError = false;
    };

    /// `return Ok();` for `Result<void>`.
    [[nodiscard]] inline Result<void> Ok() noexcept
    {
        return Result<void>();
    }

    /// `return Err(ErrorCode::InvalidData, "...");` — the spelling used everywhere a failure is
    /// produced, so that a grep for `Err(` finds every one of them.
    [[nodiscard]] inline ::cnahouse::util::Error Err(ErrorCode code, std::string message)
    {
        return ::cnahouse::util::Error(code, std::move(message));
    }

    [[nodiscard]] inline ::cnahouse::util::Error Err(ErrorCode code, std::string message, std::string context)
    {
        return ::cnahouse::util::Error(code, std::move(message), std::move(context));
    }
} // namespace cnahouse::util
