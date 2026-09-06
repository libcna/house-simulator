// SPDX-License-Identifier: MIT
#include "cnahouse/util/Json.hpp"

#include <format>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/IO/File.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonValueKind.hpp"

namespace cnahouse::util
{
    namespace
    {
        using SysJson = System::Text::Json::JsonElement;

        JsonValue::Kind KindOf(const SysJson& element)
        {
            switch (element.getValueKindProperty())
            {
                case System::Text::Json::JsonValueKind::Object:
                    return JsonValue::Kind::Object;
                case System::Text::Json::JsonValueKind::Array:
                    return JsonValue::Kind::Array;
                case System::Text::Json::JsonValueKind::String:
                    return JsonValue::Kind::String;
                case System::Text::Json::JsonValueKind::Number:
                    return JsonValue::Kind::Number;
                case System::Text::Json::JsonValueKind::True:
                case System::Text::Json::JsonValueKind::False:
                    return JsonValue::Kind::Boolean;
                case System::Text::Json::JsonValueKind::Null:
                    return JsonValue::Kind::Null;
                default:
                    return JsonValue::Kind::Undefined;
            }
        }

        std::string_view KindName(JsonValue::Kind kind)
        {
            switch (kind)
            {
                case JsonValue::Kind::Object:
                    return "an object";
                case JsonValue::Kind::Array:
                    return "an array";
                case JsonValue::Kind::String:
                    return "a string";
                case JsonValue::Kind::Number:
                    return "a number";
                case JsonValue::Kind::Boolean:
                    return "a boolean";
                case JsonValue::Kind::Null:
                    return "null";
                case JsonValue::Kind::Undefined:
                    return "nothing";
            }
            return "nothing";
        }

    } // namespace

    /// The `System::Text::Json` handles, hidden so the header does not force every world-data file to
    /// include sharp-runtime. The document is shared because a `JsonElement` borrows its storage: a
    /// `JsonValue` that outlived its document would be a dangling read, and copying elements is how
    /// this class is meant to be used.
    class JsonValue::Impl
    {
    public:
        Impl() = default;

        Impl(std::shared_ptr<System::Text::Json::JsonDocument> owner, const SysJson& value)
            : document(std::move(owner))
            , element(value)
        {
        }

        std::shared_ptr<System::Text::Json::JsonDocument> document;
        SysJson element;
        bool valid = false;
    };

    JsonValue::JsonValue()
        : impl_(std::make_unique<Impl>())
    {
    }

    JsonValue::JsonValue(std::shared_ptr<System::Text::Json::JsonDocument> document,
                         const SysJson& element,
                         std::string path)
        : impl_(std::make_unique<Impl>(std::move(document), element))
        , path_(std::move(path))
    {
        impl_->valid = true;
    }

    JsonValue::JsonValue(const JsonValue& other)
        : impl_(std::make_unique<Impl>(*other.impl_))
        , path_(other.path_)
    {
    }

    JsonValue::JsonValue(JsonValue&& other) noexcept = default;

    JsonValue& JsonValue::operator=(const JsonValue& other)
    {
        if (this != &other)
        {
            impl_ = std::make_unique<Impl>(*other.impl_);
            path_ = other.path_;
        }
        return *this;
    }

    JsonValue& JsonValue::operator=(JsonValue&& other) noexcept = default;
    JsonValue::~JsonValue() = default;

    bool JsonValue::IsValid() const
    {
        return impl_ && impl_->valid;
    }

    JsonValue::Kind JsonValue::GetKind() const
    {
        return IsValid() ? KindOf(impl_->element) : Kind::Undefined;
    }

    std::string JsonValue::Child(std::string_view field) const
    {
        if (path_.empty())
        {
            return std::string(field);
        }
        return std::format("{}.{}", path_, field);
    }

    Error JsonValue::Expected(std::string_view what, std::string_view atPath) const
    {
        return util::Error(ErrorCode::SchemaMismatch, std::format("expected {}", what), std::string(atPath));
    }

    bool JsonValue::Has(std::string_view field) const
    {
        if (!IsValid() || GetKind() != Kind::Object)
        {
            return false;
        }
        SysJson found;
        return impl_->element.TryGetProperty(std::string(field), found);
    }

    namespace
    {

        /// Fetches one named property, or produces the "which path, what was expected" error that is the
        /// whole reason this wrapper exists.
        Result<SysJson> Fetch(const JsonValue& parent,
                              const SysJson& element,
                              std::string_view field,
                              const std::string& childPath)
        {
            if (KindOf(element) != JsonValue::Kind::Object)
            {
                return util::Error(ErrorCode::SchemaMismatch,
                                   std::format("expected an object with a '{}' field, found {}",
                                               field,
                                               KindName(KindOf(element))),
                                   parent.Path().empty() ? std::string("<root>") : parent.Path());
            }
            SysJson found;
            if (!element.TryGetProperty(std::string(field), found))
            {
                return util::Error(ErrorCode::SchemaMismatch,
                                   std::format("required field '{}' is missing", field),
                                   childPath);
            }
            return found;
        }

    } // namespace

    Result<JsonValue> JsonValue::RequireObject(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto found = Fetch(*this, impl_->element, field, childPath);
        if (!found)
        {
            return found.Error();
        }
        if (KindOf(*found) != Kind::Object)
        {
            return Expected(std::format("an object, found {}", KindName(KindOf(*found))), childPath);
        }
        return JsonValue(impl_->document, *found, childPath);
    }

    Result<JsonValue> JsonValue::RequireArray(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto found = Fetch(*this, impl_->element, field, childPath);
        if (!found)
        {
            return found.Error();
        }
        if (KindOf(*found) != Kind::Array)
        {
            return Expected(std::format("an array, found {}", KindName(KindOf(*found))), childPath);
        }
        return JsonValue(impl_->document, *found, childPath);
    }

    Result<std::string> JsonValue::RequireString(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto found = Fetch(*this, impl_->element, field, childPath);
        if (!found)
        {
            return found.Error();
        }
        if (KindOf(*found) != Kind::String)
        {
            return Expected(std::format("a string, found {}", KindName(KindOf(*found))), childPath);
        }
        return found->GetString();
    }

    Result<double> JsonValue::RequireNumber(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto found = Fetch(*this, impl_->element, field, childPath);
        if (!found)
        {
            return found.Error();
        }
        if (KindOf(*found) != Kind::Number)
        {
            return Expected(std::format("a number, found {}", KindName(KindOf(*found))), childPath);
        }
        return found->GetDouble();
    }

    Result<float> JsonValue::RequireFloat(std::string_view field) const
    {
        auto number = RequireNumber(field);
        if (!number)
        {
            return number.Error();
        }
        return static_cast<float>(*number);
    }

    Result<std::int64_t> JsonValue::RequireInt(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto number = RequireNumber(field);
        if (!number)
        {
            return number.Error();
        }
        // A fractional value where an integer is required is an authoring mistake, not a value to
        // truncate: `"count": 3.5` means the author believed something this schema does not support.
        const double value = *number;
        if (value != static_cast<double>(static_cast<std::int64_t>(value)))
        {
            return util::Error(ErrorCode::SchemaMismatch,
                               std::format("expected a whole number, found {}", value),
                               childPath);
        }
        return static_cast<std::int64_t>(value);
    }

    Result<bool> JsonValue::RequireBool(std::string_view field) const
    {
        const std::string childPath = Child(field);
        auto found = Fetch(*this, impl_->element, field, childPath);
        if (!found)
        {
            return found.Error();
        }
        if (KindOf(*found) != Kind::Boolean)
        {
            return Expected(std::format("a boolean, found {}", KindName(KindOf(*found))), childPath);
        }
        return found->getValueKindProperty() == System::Text::Json::JsonValueKind::True;
    }

    namespace
    {

        /// Reads a fixed-length numeric array, naming the index that was wrong rather than the whole array.
        Result<std::vector<float>>
        Floats(const JsonValue& parent, std::string_view field, std::size_t expected)
        {
            auto array = parent.RequireArray(field);
            if (!array)
            {
                return array.Error();
            }
            auto elements = array->Elements();
            if (!elements)
            {
                return elements.Error();
            }
            if (elements->size() != expected)
            {
                return util::Error(ErrorCode::SchemaMismatch,
                                   std::format("expected {} numbers, found {}", expected, elements->size()),
                                   array->Path());
            }
            std::vector<float> out;
            out.reserve(expected);
            for (const JsonValue& element : *elements)
            {
                auto value = element.AsFloat();
                if (!value)
                {
                    return value.Error();
                }
                out.push_back(*value);
            }
            return out;
        }

    } // namespace

    Result<Microsoft::Xna::Framework::Vector3> JsonValue::RequireVector3(std::string_view field) const
    {
        auto values = Floats(*this, field, 3);
        if (!values)
        {
            return values.Error();
        }
        return Microsoft::Xna::Framework::Vector3((*values)[0], (*values)[1], (*values)[2]);
    }

    Result<Microsoft::Xna::Framework::Vector2> JsonValue::RequireVector2(std::string_view field) const
    {
        auto values = Floats(*this, field, 2);
        if (!values)
        {
            return values.Error();
        }
        return Microsoft::Xna::Framework::Vector2((*values)[0], (*values)[1]);
    }

    Result<Microsoft::Xna::Framework::BoundingBox> JsonValue::RequireBox(std::string_view field) const
    {
        auto object = RequireObject(field);
        if (!object)
        {
            return object.Error();
        }
        auto min = object->RequireVector3("min");
        if (!min)
        {
            return min.Error();
        }
        auto max = object->RequireVector3("max");
        if (!max)
        {
            return max.Error();
        }
        // Validated here rather than at every use: an inverted box passes every later type check and
        // then silently contains nothing, which shows up as a room that is invisible for no reason.
        if (min->X > max->X || min->Y > max->Y || min->Z > max->Z)
        {
            return util::Error(ErrorCode::OutOfRange,
                               std::format("expected min <= max on every axis, found min ({}, {}, {}) and "
                                           "max ({}, {}, {})",
                                           min->X,
                                           min->Y,
                                           min->Z,
                                           max->X,
                                           max->Y,
                                           max->Z),
                               object->Path());
        }
        return Microsoft::Xna::Framework::BoundingBox(*min, *max);
    }

    Result<std::string> JsonValue::OptionalString(std::string_view field, std::string fallback) const
    {
        if (!Has(field))
        {
            return fallback;
        }
        return RequireString(field);
    }

    Result<std::int64_t> JsonValue::OptionalInt(std::string_view field, std::int64_t fallback) const
    {
        if (!Has(field))
        {
            return fallback;
        }
        return RequireInt(field);
    }

    Result<float> JsonValue::OptionalFloat(std::string_view field, float fallback) const
    {
        if (!Has(field))
        {
            return fallback;
        }
        return RequireFloat(field);
    }

    Result<bool> JsonValue::OptionalBool(std::string_view field, bool fallback) const
    {
        if (!Has(field))
        {
            return fallback;
        }
        return RequireBool(field);
    }

    Result<std::vector<JsonValue>> JsonValue::Elements() const
    {
        if (!IsValid() || GetKind() != Kind::Array)
        {
            return Expected(std::format("an array, found {}", KindName(GetKind())), path_);
        }
        std::vector<JsonValue> out;
        const auto raw = impl_->element.EnumerateArray();
        out.reserve(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i)
        {
            out.emplace_back(JsonValue(impl_->document, raw[i], std::format("{}[{}]", path_, i)));
        }
        return out;
    }

    Result<std::size_t> JsonValue::Count() const
    {
        if (!IsValid() || GetKind() != Kind::Array)
        {
            return Expected(std::format("an array, found {}", KindName(GetKind())), path_);
        }
        return static_cast<std::size_t>(impl_->element.GetArrayLength());
    }

    Result<std::string> JsonValue::AsString() const
    {
        if (GetKind() != Kind::String)
        {
            return Expected(std::format("a string, found {}", KindName(GetKind())), path_);
        }
        return impl_->element.GetString();
    }

    Result<double> JsonValue::AsNumber() const
    {
        if (GetKind() != Kind::Number)
        {
            return Expected(std::format("a number, found {}", KindName(GetKind())), path_);
        }
        return impl_->element.GetDouble();
    }

    Result<float> JsonValue::AsFloat() const
    {
        auto number = AsNumber();
        if (!number)
        {
            return number.Error();
        }
        return static_cast<float>(*number);
    }

    Result<std::int64_t> JsonValue::AsInt() const
    {
        auto number = AsNumber();
        if (!number)
        {
            return number.Error();
        }
        if (*number != static_cast<double>(static_cast<std::int64_t>(*number)))
        {
            return Error(
                ErrorCode::SchemaMismatch, std::format("expected a whole number, found {}", *number), path_);
        }
        return static_cast<std::int64_t>(*number);
    }

    // ------------------------------------------------------------------------------------------------

    JsonDocument::JsonDocument(std::shared_ptr<System::Text::Json::JsonDocument> document,
                               JsonValue root,
                               std::string name)
        : document_(std::move(document))
        , root_(std::move(root))
        , name_(std::move(name))
    {
    }

    Result<JsonDocument> JsonDocument::Parse(std::string_view text, std::string name)
    {
        try
        {
            auto document = System::Text::Json::JsonDocument::Parse(std::string(text));
            if (!document)
            {
                return util::Error(ErrorCode::InvalidData, "the document did not parse", name);
            }
            JsonValue root(document, document->getRootElementProperty(), std::string{});
            return JsonDocument(std::move(document), std::move(root), std::move(name));
        }
        catch (const std::exception& e)
        {
            // `System::Text::Json` throws on malformed input; every other failure in this file is a
            // `Result`, so the exception is converted here at the boundary rather than escaping into
            // the loaders (`docs/conventions.md` §5.2).
            return util::Error(ErrorCode::InvalidData, e.what(), name);
        }
    }

    Result<JsonDocument> JsonDocument::Load(std::string_view path)
    {
        std::string text;
        try
        {
            text = System::IO::File::ReadAllText(std::string(path));
        }
        catch (const std::exception& e)
        {
            return util::Error(ErrorCode::IoFailure, e.what(), std::string(path));
        }
        return Parse(text, std::string(path));
    }

} // namespace cnahouse::util
