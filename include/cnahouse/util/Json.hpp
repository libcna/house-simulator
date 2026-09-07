// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace Microsoft::Xna::Framework
{
    struct Vector2;
    struct Vector3;
    struct BoundingBox;
} // namespace Microsoft::Xna::Framework

namespace System::Text::Json
{
    class JsonDocument;
    class JsonElement;
} // namespace System::Text::Json

namespace cnahouse::util
{

    /// @brief A typed, path-carrying view over one JSON value.
    ///
    /// **Why this exists rather than calling `System::Text::Json` directly.** The house is data
    /// (`CLAUDE.md` §3): rooms, portals, lights, props and interactables are all authored JSON, and
    /// most of the failures this project will ever see are authoring mistakes in those files. The
    /// difference between a usable project and an infuriating one is whether the error says
    ///
    ///     world/l0.json: rooms[3].portals[1].width: expected a number, found a string
    ///
    /// or `std::bad_variant_access`. `System::Text::Json` gives the second. This wrapper carries the
    /// **JSON path** alongside every value so the first is what a caller gets for free, which is
    /// exactly `HOUSE-00028`'s acceptance criterion.
    ///
    /// The `Require*` family returns `Result`, never throws, and never silently substitutes a default
    /// (`docs/conventions.md` §5.4). `Optional*` is the explicit form for a field that may be absent,
    /// and it takes the default at the call site where a reader can see it.
    class JsonValue
    {
    public:
        /// @brief What kind of value this is, without needing the underlying enum.
        enum class Kind
        {
            Undefined,
            Object,
            Array,
            String,
            Number,
            Boolean,
            Null,
        };

        JsonValue();
        JsonValue(const JsonValue& other);
        JsonValue(JsonValue&& other) noexcept;
        JsonValue& operator=(const JsonValue& other);
        JsonValue& operator=(JsonValue&& other) noexcept;
        ~JsonValue();

        [[nodiscard]] Kind GetKind() const;
        [[nodiscard]] bool IsValid() const;

        /// @brief The JSON path of this value, e.g. `rooms[3].portals[1].width`.
        [[nodiscard]] const std::string& Path() const noexcept
        {
            return path_;
        }

        // --- required fields: absent or wrongly typed is an error -------------------------------
        [[nodiscard]] Result<JsonValue> RequireObject(std::string_view field) const;
        [[nodiscard]] Result<JsonValue> RequireArray(std::string_view field) const;
        [[nodiscard]] Result<std::string> RequireString(std::string_view field) const;
        [[nodiscard]] Result<std::int64_t> RequireInt(std::string_view field) const;
        [[nodiscard]] Result<double> RequireNumber(std::string_view field) const;
        [[nodiscard]] Result<float> RequireFloat(std::string_view field) const;
        [[nodiscard]] Result<bool> RequireBool(std::string_view field) const;

        /// @brief A `Vector3` from a three-element array `[x, y, z]`.
        ///
        /// The array form is used rather than `{"x":…}` because it is what every DCC tool and every
        /// glTF file already writes, and because three numbers on one line stay readable in a diff.
        [[nodiscard]] Result<Microsoft::Xna::Framework::Vector3> RequireVector3(std::string_view field) const;
        [[nodiscard]] Result<Microsoft::Xna::Framework::Vector2> RequireVector2(std::string_view field) const;
        /// @brief A `BoundingBox` from `{"min":[…],"max":[…]}`, validated as min <= max per axis.
        [[nodiscard]] Result<Microsoft::Xna::Framework::BoundingBox> RequireBox(std::string_view field) const;

        // --- optional fields: absent is fine, present-but-wrong is still an error ------------------
        [[nodiscard]] Result<std::string> OptionalString(std::string_view field, std::string fallback) const;
        [[nodiscard]] Result<std::int64_t> OptionalInt(std::string_view field, std::int64_t fallback) const;
        [[nodiscard]] Result<float> OptionalFloat(std::string_view field, float fallback) const;
        [[nodiscard]] Result<bool> OptionalBool(std::string_view field, bool fallback) const;

        [[nodiscard]] bool Has(std::string_view field) const;

        /// @brief Is @p field present **and** explicitly `null`?
        ///
        /// `Has` cannot answer this and the three cases are genuinely different in the world files.
        /// `docs/world-format.md` is explicit that `null` means "not specified, use the documented
        /// default" and is **never** a synonym for zero, so a reader has to tell "absent" from
        /// "null" from "present and the wrong type" -- and without this, `null` and a string that
        /// failed to parse look identical to a caller (`HOUSE-00343`).
        [[nodiscard]] bool IsNull(std::string_view field) const;

        // --- arrays --------------------------------------------------------------------------------
        [[nodiscard]] Result<std::vector<JsonValue>> Elements() const;
        [[nodiscard]] Result<std::size_t> Count() const;

        /// @brief This value read as a string/number/etc., for elements of an array.
        [[nodiscard]] Result<std::string> AsString() const;
        [[nodiscard]] Result<double> AsNumber() const;
        [[nodiscard]] Result<float> AsFloat() const;
        [[nodiscard]] Result<std::int64_t> AsInt() const;

    private:
        friend class JsonDocument;
        class Impl;

        JsonValue(std::shared_ptr<System::Text::Json::JsonDocument> document,
                  const System::Text::Json::JsonElement& element,
                  std::string path);

        [[nodiscard]] Error Expected(std::string_view what, std::string_view atPath) const;
        [[nodiscard]] std::string Child(std::string_view field) const;

        std::unique_ptr<Impl> impl_;
        std::string path_;
    };

    /// @brief A parsed JSON document, owning the storage every `JsonValue` from it borrows.
    class JsonDocument
    {
    public:
        /// @brief Parses @p text. @p name is what an error message will call the source.
        [[nodiscard]] static Result<JsonDocument> Parse(std::string_view text, std::string name);

        /// @brief Reads and parses a file through `System::IO`.
        ///
        /// Not `std::filesystem`: `docs/conventions.md` §5 and `cna-house.md` §8.3 permit it only
        /// inside `SaveStore`'s desktop implementation, and the Web port has no such thing.
        [[nodiscard]] static Result<JsonDocument> Load(std::string_view path);

        JsonDocument(const JsonDocument&) = default;
        JsonDocument(JsonDocument&&) noexcept = default;
        JsonDocument& operator=(const JsonDocument&) = default;
        JsonDocument& operator=(JsonDocument&&) noexcept = default;
        ~JsonDocument() = default;

        [[nodiscard]] const JsonValue& Root() const noexcept
        {
            return root_;
        }

        [[nodiscard]] const std::string& Name() const noexcept
        {
            return name_;
        }

    private:
        JsonDocument(std::shared_ptr<System::Text::Json::JsonDocument> document,
                     JsonValue root,
                     std::string name);

        std::shared_ptr<System::Text::Json::JsonDocument> document_;
        JsonValue root_;
        std::string name_;
    };

} // namespace cnahouse::util
