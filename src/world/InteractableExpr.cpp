// SPDX-License-Identifier: MIT
#include "cnahouse/world/InteractableExpr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace cnahouse::world
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;
        using util::Result;

        [[nodiscard]] bool IsIdentStart(char c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }

        [[nodiscard]] bool IsIdentPart(char c) noexcept
        {
            return IsIdentStart(c) || (c >= '0' && c <= '9');
        }

        [[nodiscard]] std::string_view TypeName(const StateValue& value) noexcept
        {
            if (std::holds_alternative<bool>(value))
            {
                return "boolean";
            }
            if (std::holds_alternative<double>(value))
            {
                return "number";
            }
            return "text";
        }

        [[nodiscard]] std::string Spell(const StateValue& value)
        {
            if (const bool* flag = std::get_if<bool>(&value))
            {
                return *flag ? "true" : "false";
            }
            if (const double* number = std::get_if<double>(&value))
            {
                std::string text = std::to_string(*number);
                // `std::to_string` gives six decimals for everything; a predicate printed back as
                // `state.flow == 0.500000` is harder to compare with the file than it needs to be.
                while (text.size() > 1 && text.back() == '0')
                {
                    text.pop_back();
                }
                if (!text.empty() && text.back() == '.')
                {
                    text.pop_back();
                }
                return text;
            }
            return "'" + std::get<std::string>(value) + "'";
        }
    } // namespace

    // ======================================================================== StateTable ========

    void StateTable::Declare(std::string name, StateValue value)
    {
        if (Field* existing = Find(name); existing != nullptr)
        {
            existing->value = std::move(value);
            return;
        }
        m_fields.push_back(Field{std::move(name), std::move(value)});
    }

    const StateTable::Field* StateTable::Find(std::string_view name) const noexcept
    {
        for (const Field& field : m_fields)
        {
            if (field.name == name)
            {
                return &field;
            }
        }
        return nullptr;
    }

    StateTable::Field* StateTable::Find(std::string_view name) noexcept
    {
        for (Field& field : m_fields)
        {
            if (field.name == name)
            {
                return &field;
            }
        }
        return nullptr;
    }

    std::size_t StateTable::IndexOf(std::string_view name) const noexcept
    {
        for (std::size_t index = 0; index < m_fields.size(); ++index)
        {
            if (m_fields[index].name == name)
            {
                return index;
            }
        }
        return m_fields.size();
    }

    std::span<const StateTable::Field> StateTable::Fields() const noexcept
    {
        return m_fields;
    }

    bool StateTable::Empty() const noexcept
    {
        return m_fields.empty();
    }

    std::string StateTable::Names() const
    {
        std::string text;
        for (const Field& field : m_fields)
        {
            if (!text.empty())
            {
                text += ", ";
            }
            text += field.name;
        }
        return text.empty() ? "(none)" : text;
    }

    // ==================================================================== ExpressionParser ======

    /// @brief One recursive-descent pass over one expression.
    ///
    /// Hand-written rather than generated: the grammar is fifteen lines and the thing that matters
    /// about it is the *messages*, which a generator would give up.
    class ExpressionParser
    {
    public:
        ExpressionParser(std::string_view text, const StateTable& state)
            : m_text(text)
            , m_state(state)
        {
        }

        [[nodiscard]] Result<Predicate> ParsePredicate()
        {
            Predicate predicate;
            const Result<std::int32_t> root = ParseOr(predicate);
            if (!root)
            {
                return root.Error();
            }
            if (const Result<void> end = ExpectEnd(); !end)
            {
                return end.Error();
            }
            predicate.m_root = root.Value();
            return predicate;
        }

        [[nodiscard]] Result<Effect> ParseEffect()
        {
            Effect effect;
            while (true)
            {
                SkipSpace();
                if (AtEnd())
                {
                    break;
                }
                const Result<Effect::Statement> statement = ParseStatement();
                if (!statement)
                {
                    return statement.Error();
                }
                effect.m_statements.push_back(statement.Value());
                SkipSpace();
                if (Peek() == ';')
                {
                    ++m_at;
                    continue;
                }
                break;
            }
            if (const Result<void> end = ExpectEnd(); !end)
            {
                return end.Error();
            }
            if (effect.m_statements.empty())
            {
                return Err(ErrorCode::InvalidData, "an effect does something; this one is empty", Where());
            }
            return effect;
        }

    private:
        [[nodiscard]] bool AtEnd() const noexcept
        {
            return m_at >= m_text.size();
        }

        [[nodiscard]] char Peek() const noexcept
        {
            return AtEnd() ? '\0' : m_text[m_at];
        }

        void SkipSpace() noexcept
        {
            while (!AtEnd() && (m_text[m_at] == ' ' || m_text[m_at] == '\t' || m_text[m_at] == '\n'))
            {
                ++m_at;
            }
        }

        /// @brief Is @p token next, without consuming it?
        [[nodiscard]] bool Looking(std::string_view token) const noexcept
        {
            return m_text.compare(m_at, token.size(), token) == 0;
        }

        [[nodiscard]] bool Take(std::string_view token) noexcept
        {
            SkipSpace();
            if (m_text.compare(m_at, token.size(), token) == 0)
            {
                m_at += token.size();
                return true;
            }
            return false;
        }

        /// @brief `at offset N of "<expression>"`, so a message points at a character.
        [[nodiscard]] std::string Where() const
        {
            return "offset " + std::to_string(m_at) + " of \"" + std::string(m_text) + "\"";
        }

        [[nodiscard]] Result<void> ExpectEnd()
        {
            SkipSpace();
            if (!AtEnd())
            {
                return Err(ErrorCode::InvalidData,
                           std::string("unexpected \"") + std::string(m_text.substr(m_at)) +
                               "\" after the end of the expression",
                           Where());
            }
            return util::Ok();
        }

        /// @brief Appends @p node and returns its index.
        ///
        /// **By const reference and not by value.** Every caller passes a named local it does not
        /// give up, so a by-value parameter was a copy AND a move where a copy does; and at `-O3`
        /// GCC 14 could not prove the copied-into parameter's `StateValue` variant was initialised
        /// through the two inlinings, which made `-Wmaybe-uninitialized` fire on
        /// `basic_string::_M_string_length` and, under `-Werror`, broke every optimised build of
        /// the project (`HOUSE-00698`). The reference removes the temporary the warning was about.
        [[nodiscard]] std::int32_t Add(Predicate& predicate, const Predicate::Node& node)
        {
            predicate.m_nodes.push_back(node);
            return static_cast<std::int32_t>(predicate.m_nodes.size()) - 1;
        }

        [[nodiscard]] Result<std::int32_t> ParseOr(Predicate& predicate)
        {
            Result<std::int32_t> left = ParseAnd(predicate);
            if (!left)
            {
                return left;
            }
            while (Take("||"))
            {
                const Result<std::int32_t> right = ParseAnd(predicate);
                if (!right)
                {
                    return right;
                }
                Predicate::Node node;
                node.op = Predicate::Op::Or;
                node.left = left.Value();
                node.right = right.Value();
                left = Add(predicate, node);
            }
            return left;
        }

        [[nodiscard]] Result<std::int32_t> ParseAnd(Predicate& predicate)
        {
            Result<std::int32_t> left = ParseComparison(predicate);
            if (!left)
            {
                return left;
            }
            while (Take("&&"))
            {
                const Result<std::int32_t> right = ParseComparison(predicate);
                if (!right)
                {
                    return right;
                }
                Predicate::Node node;
                node.op = Predicate::Op::And;
                node.left = left.Value();
                node.right = right.Value();
                left = Add(predicate, node);
            }
            return left;
        }

        [[nodiscard]] Result<std::int32_t> ParseComparison(Predicate& predicate)
        {
            SkipSpace();
            if (Take("!"))
            {
                const Result<std::int32_t> inner = ParseComparison(predicate);
                if (!inner)
                {
                    return inner;
                }
                Predicate::Node node;
                node.op = Predicate::Op::Not;
                node.left = inner.Value();
                return Add(predicate, node);
            }
            if (Take("("))
            {
                const Result<std::int32_t> inner = ParseOr(predicate);
                if (!inner)
                {
                    return inner;
                }
                if (!Take(")"))
                {
                    return Err(ErrorCode::InvalidData, "a \"(\" here is never closed", Where());
                }
                return inner;
            }

            const Result<Predicate::Node> left = ParseTerm();
            if (!left)
            {
                return left.Error();
            }

            static constexpr std::pair<std::string_view, Predicate::Op> kOperators[] = {
                {"==", Predicate::Op::Equal},
                {"!=", Predicate::Op::NotEqual},
                {"<=", Predicate::Op::LessEqual},
                {">=", Predicate::Op::GreaterEqual},
                {"<", Predicate::Op::Less},
                {">", Predicate::Op::Greater},
            };
            for (const auto& [token, op] : kOperators)
            {
                if (!Take(token))
                {
                    continue;
                }
                const Result<Predicate::Node> right = ParseTerm();
                if (!right)
                {
                    return right.Error();
                }
                if (const Result<void> typed = CheckComparable(left.Value(), right.Value(), op, token);
                    !typed)
                {
                    return typed.Error();
                }
                Predicate::Node node;
                node.op = op;
                node.left = Add(predicate, left.Value());
                node.right = Add(predicate, right.Value());
                return Add(predicate, node);
            }

            // Nothing matched an operator. Before falling back to "a bare boolean is a condition",
            // say so when what follows is neither the end nor a connective: `=<`, `+`, `&` and a
            // stray second value all land here, and "a number on its own" would be true but would
            // point at the wrong half of the line.
            SkipSpace();
            if (!AtEnd() && Peek() != ')' && !Looking("&&") && !Looking("||"))
            {
                return Err(ErrorCode::InvalidData,
                           std::string("expected a comparison operator before \"") +
                               std::string(m_text.substr(m_at)) +
                               "\"; the operators are ==, !=, <, <=, > and >=",
                           Where());
            }

            // A bare term used as a condition. Only a boolean can be one: `state.flow` alone is
            // almost certainly a forgotten comparison rather than "is the flow non-zero".
            const StateValue value = ValueOf(left.Value());
            if (!std::holds_alternative<bool>(value))
            {
                return Err(ErrorCode::InvalidData,
                           std::string("a condition is a boolean or a comparison; this is a ") +
                               std::string(TypeName(value)) +
                               " on its own, which is a comparison somebody did not finish",
                           Where());
            }
            Predicate::Node node;
            node.op = Predicate::Op::Truth;
            node.left = Add(predicate, left.Value());
            return Add(predicate, node);
        }

        /// @brief The static type of a leaf: the declared field's type, or the literal's.
        [[nodiscard]] StateValue ValueOf(const Predicate::Node& leaf) const
        {
            if (leaf.field >= 0)
            {
                return m_state.Fields()[static_cast<std::size_t>(leaf.field)].value;
            }
            return leaf.literal;
        }

        [[nodiscard]] Result<void> CheckComparable(const Predicate::Node& left,
                                                   const Predicate::Node& right,
                                                   Predicate::Op op,
                                                   std::string_view token) const
        {
            const StateValue a = ValueOf(left);
            const StateValue b = ValueOf(right);
            if (a.index() != b.index())
            {
                return Err(ErrorCode::InvalidData,
                           std::string("cannot compare a ") + std::string(TypeName(a)) + " with a " +
                               std::string(TypeName(b)),
                           Where());
            }
            const bool ordered = op == Predicate::Op::Less || op == Predicate::Op::LessEqual ||
                                 op == Predicate::Op::Greater || op == Predicate::Op::GreaterEqual;
            if (ordered && !std::holds_alternative<double>(a))
            {
                return Err(ErrorCode::InvalidData,
                           std::string("\"") + std::string(token) + "\" orders numbers; these are " +
                               std::string(TypeName(a)) + "s",
                           Where());
            }
            return util::Ok();
        }

        [[nodiscard]] Result<Predicate::Node> ParseTerm()
        {
            SkipSpace();
            if (AtEnd())
            {
                return Err(ErrorCode::InvalidData, "the expression ends where a value is expected", Where());
            }

            Predicate::Node node;
            if (Take("state."))
            {
                const std::size_t start = m_at;
                while (!AtEnd() && IsIdentPart(m_text[m_at]))
                {
                    ++m_at;
                }
                const std::string_view name = m_text.substr(start, m_at - start);
                if (name.empty())
                {
                    return Err(ErrorCode::InvalidData, "\"state.\" names a field", Where());
                }
                const std::size_t index = m_state.IndexOf(name);
                if (index == m_state.Fields().size())
                {
                    return Err(ErrorCode::InvalidData,
                               std::string("this interactable has no state field \"") + std::string(name) +
                                   "\"; it declares " + m_state.Names(),
                               Where());
                }
                node.field = static_cast<std::int32_t>(index);
                return node;
            }
            if (Take("true"))
            {
                node.literal = true;
                return node;
            }
            if (Take("false"))
            {
                node.literal = false;
                return node;
            }
            if (Peek() == '\'')
            {
                ++m_at;
                const std::size_t start = m_at;
                while (!AtEnd() && m_text[m_at] != '\'')
                {
                    ++m_at;
                }
                if (AtEnd())
                {
                    return Err(ErrorCode::InvalidData, "a \"'\" here is never closed", Where());
                }
                node.literal = std::string(m_text.substr(start, m_at - start));
                ++m_at;
                return node;
            }

            const char first = Peek();
            if ((first >= '0' && first <= '9') || first == '-' || first == '+' || first == '.')
            {
                const std::size_t start = m_at;
                if (first == '-' || first == '+')
                {
                    ++m_at;
                }
                bool digits = false;
                bool dot = false;
                while (!AtEnd())
                {
                    const char c = m_text[m_at];
                    if (c >= '0' && c <= '9')
                    {
                        digits = true;
                        ++m_at;
                    }
                    else if (c == '.' && !dot)
                    {
                        dot = true;
                        ++m_at;
                    }
                    else
                    {
                        break;
                    }
                }
                if (!digits)
                {
                    return Err(ErrorCode::InvalidData, "this is not a number", Where());
                }
                node.literal = std::strtod(std::string(m_text.substr(start, m_at - start)).c_str(), nullptr);
                return node;
            }

            // Everything else. The message quotes the token as written, because "unexpected input"
            // sends nobody anywhere, and it is the acceptance criterion for this task.
            const std::size_t start = m_at;
            while (!AtEnd() && IsIdentPart(m_text[m_at]))
            {
                ++m_at;
            }
            const std::string_view token =
                m_at > start ? m_text.substr(start, m_at - start) : m_text.substr(start, 1);
            if (m_at == start)
            {
                ++m_at;
            }
            return Err(ErrorCode::InvalidData,
                       std::string("\"") + std::string(token) +
                           "\" is not in the expression vocabulary; a value is state.<field>, a "
                           "number, true, false, or 'text'",
                       Where());
        }

        [[nodiscard]] Result<Effect::Statement> ParseStatement()
        {
            Effect::Statement statement;
            SkipSpace();

            if (Take("toggle"))
            {
                if (!Take("("))
                {
                    return Err(ErrorCode::InvalidData, "toggle takes a field: toggle(state.x)", Where());
                }
                const Result<Predicate::Node> target = ParseTerm();
                if (!target)
                {
                    return target.Error();
                }
                if (!Take(")"))
                {
                    return Err(ErrorCode::InvalidData, "a \"(\" here is never closed", Where());
                }
                if (target.Value().field < 0)
                {
                    return Err(ErrorCode::InvalidData, "toggle takes a state field, not a literal", Where());
                }
                const StateValue& value =
                    m_state.Fields()[static_cast<std::size_t>(target.Value().field)].value;
                if (!std::holds_alternative<bool>(value))
                {
                    return Err(ErrorCode::InvalidData,
                               std::string("toggle flips a boolean; this field is a ") +
                                   std::string(TypeName(value)),
                               Where());
                }
                statement.kind = Effect::Kind::Toggle;
                statement.field = target.Value().field;
                return statement;
            }

            const Result<Predicate::Node> target = ParseTerm();
            if (!target)
            {
                return target.Error();
            }
            if (target.Value().field < 0)
            {
                return Err(ErrorCode::InvalidData,
                           "an effect assigns to a state field; the left side here is a literal",
                           Where());
            }
            statement.field = target.Value().field;

            SkipSpace();
            if (Looking("=="))
            {
                // The commonest slip, and the one whose default message is least helpful: taking
                // the first `=` leaves `= true`, and the parser then complains about `=`.
                return Err(ErrorCode::InvalidData,
                           "an effect assigns and \"==\" compares; this statement assigns, so it "
                           "wants a single \"=\"",
                           Where());
            }
            if (Take("+="))
            {
                statement.kind = Effect::Kind::Add;
            }
            else if (Take("-="))
            {
                statement.kind = Effect::Kind::Subtract;
            }
            else if (Take("="))
            {
                statement.kind = Effect::Kind::Assign;
            }
            else
            {
                return Err(
                    ErrorCode::InvalidData, "an effect statement assigns: \"=\", \"+=\" or \"-=\"", Where());
            }

            const Result<Predicate::Node> source = ParseTerm();
            if (!source)
            {
                return source.Error();
            }
            const StateValue& declared = m_state.Fields()[static_cast<std::size_t>(statement.field)].value;
            const StateValue assigned =
                source.Value().field >= 0
                    ? m_state.Fields()[static_cast<std::size_t>(source.Value().field)].value
                    : source.Value().literal;
            if (declared.index() != assigned.index())
            {
                return Err(ErrorCode::InvalidData,
                           std::string("cannot assign a ") + std::string(TypeName(assigned)) + " to a " +
                               std::string(TypeName(declared)) + " field",
                           Where());
            }
            if (statement.kind != Effect::Kind::Assign && !std::holds_alternative<double>(declared))
            {
                return Err(ErrorCode::InvalidData,
                           std::string("\"+=\" and \"-=\" add numbers; this field is a ") +
                               std::string(TypeName(declared)),
                           Where());
            }

            if (source.Value().field >= 0)
            {
                statement.source = source.Value().field;
            }
            else
            {
                statement.literal = source.Value().literal;
            }
            return statement;
        }

        std::string_view m_text;
        const StateTable& m_state;
        std::size_t m_at = 0;
    };

    // ========================================================================== Predicate =======

    Predicate Predicate::AlwaysTrue()
    {
        Predicate predicate;
        predicate.m_nodes.push_back(Node{});
        predicate.m_root = 0;
        return predicate;
    }

    bool Predicate::IsAlwaysTrue() const noexcept
    {
        return m_root >= 0 && m_nodes[static_cast<std::size_t>(m_root)].op == Op::True;
    }

    util::Result<Predicate> Predicate::Parse(std::string_view text, const StateTable& state)
    {
        // An absent or blank `when` is the predicate that is always true. Most of the 640 rows have
        // no condition, and the file already writes `null` for them.
        const std::size_t first = text.find_first_not_of(" \t\n");
        if (first == std::string_view::npos)
        {
            return AlwaysTrue();
        }
        ExpressionParser parser(text, state);
        return parser.ParsePredicate();
    }

    util::Result<bool> Predicate::Evaluate(const StateTable& state) const
    {
        if (m_root < 0)
        {
            return true;
        }

        // An explicit stack rather than recursion: an authored predicate is shallow, but the tree
        // comes from data and a deep one must not be able to overflow the game's stack.
        struct Frame
        {
            std::int32_t node;
            std::int32_t stage;
        };

        std::vector<bool> values(m_nodes.size(), false);
        std::vector<Frame> stack{{m_root, 0}};

        const auto leaf = [&state, this](std::int32_t index) -> util::Result<StateValue>
        {
            const Node& node = m_nodes[static_cast<std::size_t>(index)];
            if (node.field < 0)
            {
                return node.literal;
            }
            const std::size_t field = static_cast<std::size_t>(node.field);
            if (field >= state.Fields().size())
            {
                return util::Err(util::ErrorCode::InvalidData,
                                 "the state table this predicate was parsed against has fewer "
                                 "fields than the one it is evaluated against");
            }
            return state.Fields()[field].value;
        };

        while (!stack.empty())
        {
            Frame& frame = stack.back();
            const Node& node = m_nodes[static_cast<std::size_t>(frame.node)];
            const std::size_t slot = static_cast<std::size_t>(frame.node);

            switch (node.op)
            {
                case Op::True:
                    values[slot] = true;
                    stack.pop_back();
                    break;
                case Op::Not:
                    if (frame.stage == 0)
                    {
                        frame.stage = 1;
                        stack.push_back({node.left, 0});
                    }
                    else
                    {
                        values[slot] = !values[static_cast<std::size_t>(node.left)];
                        stack.pop_back();
                    }
                    break;
                case Op::And:
                case Op::Or:
                    if (frame.stage == 0)
                    {
                        frame.stage = 1;
                        stack.push_back({node.left, 0});
                    }
                    else if (frame.stage == 1)
                    {
                        frame.stage = 2;
                        stack.push_back({node.right, 0});
                    }
                    else
                    {
                        const bool a = values[static_cast<std::size_t>(node.left)];
                        const bool b = values[static_cast<std::size_t>(node.right)];
                        values[slot] = node.op == Op::And ? (a && b) : (a || b);
                        stack.pop_back();
                    }
                    break;
                case Op::Truth:
                {
                    const util::Result<StateValue> value = leaf(node.left);
                    if (!value)
                    {
                        return value.Error();
                    }
                    values[slot] = std::get<bool>(value.Value());
                    stack.pop_back();
                    break;
                }
                default:
                {
                    const util::Result<StateValue> a = leaf(node.left);
                    if (!a)
                    {
                        return a.Error();
                    }
                    const util::Result<StateValue> b = leaf(node.right);
                    if (!b)
                    {
                        return b.Error();
                    }
                    bool result = false;
                    if (const double* number = std::get_if<double>(&a.Value()))
                    {
                        const double other = std::get<double>(b.Value());
                        switch (node.op)
                        {
                            case Op::Equal:
                                result = *number == other;
                                break;
                            case Op::NotEqual:
                                result = *number != other;
                                break;
                            case Op::Less:
                                result = *number < other;
                                break;
                            case Op::LessEqual:
                                result = *number <= other;
                                break;
                            case Op::Greater:
                                result = *number > other;
                                break;
                            default:
                                result = *number >= other;
                                break;
                        }
                    }
                    else if (const bool* flag = std::get_if<bool>(&a.Value()))
                    {
                        const bool other = std::get<bool>(b.Value());
                        result = node.op == Op::Equal ? (*flag == other) : (*flag != other);
                    }
                    else
                    {
                        const std::string& left = std::get<std::string>(a.Value());
                        const std::string& right = std::get<std::string>(b.Value());
                        result = node.op == Op::Equal ? (left == right) : (left != right);
                    }
                    values[slot] = result;
                    stack.pop_back();
                    break;
                }
            }
        }
        return static_cast<bool>(values[static_cast<std::size_t>(m_root)]);
    }

    std::string Predicate::ToString() const
    {
        if (m_root < 0 || IsAlwaysTrue())
        {
            return "true";
        }
        std::string text;
        Print(m_root, text);
        return text;
    }

    void Predicate::PrintLeaf(const Node& node, std::string& text)
    {
        if (node.field >= 0)
        {
            text += "state.#" + std::to_string(node.field);
        }
        else
        {
            text += Spell(node.literal);
        }
    }

    void Predicate::Print(std::int32_t index, std::string& text) const
    {
        const Node& node = m_nodes[static_cast<std::size_t>(index)];
        switch (node.op)
        {
            case Op::True:
                text += "true";
                return;
            case Op::Not:
                text += "!(";
                Print(node.left, text);
                text += ")";
                return;
            case Op::And:
            case Op::Or:
                text += "(";
                Print(node.left, text);
                text += node.op == Op::And ? " && " : " || ";
                Print(node.right, text);
                text += ")";
                return;
            case Op::Truth:
                PrintLeaf(m_nodes[static_cast<std::size_t>(node.left)], text);
                return;
            default:
                break;
        }
        PrintLeaf(m_nodes[static_cast<std::size_t>(node.left)], text);
        switch (node.op)
        {
            case Op::Equal:
                text += " == ";
                break;
            case Op::NotEqual:
                text += " != ";
                break;
            case Op::Less:
                text += " < ";
                break;
            case Op::LessEqual:
                text += " <= ";
                break;
            case Op::Greater:
                text += " > ";
                break;
            default:
                text += " >= ";
                break;
        }
        PrintLeaf(m_nodes[static_cast<std::size_t>(node.right)], text);
    }

    // ============================================================================= Effect =======

    Effect Effect::Nothing()
    {
        return Effect{};
    }

    bool Effect::IsEmpty() const noexcept
    {
        return m_statements.empty();
    }

    util::Result<Effect> Effect::Parse(std::string_view text, const StateTable& state)
    {
        const std::size_t first = text.find_first_not_of(" \t\n");
        if (first == std::string_view::npos)
        {
            return Nothing();
        }
        ExpressionParser parser(text, state);
        return parser.ParseEffect();
    }

    util::Result<void> Effect::Apply(StateTable& state) const
    {
        for (const Statement& statement : m_statements)
        {
            const std::size_t index = static_cast<std::size_t>(statement.field);
            if (index >= state.Fields().size())
            {
                return util::Err(util::ErrorCode::InvalidData,
                                 "the state table this effect was parsed against has fewer fields "
                                 "than the one it is applied to");
            }
            // `Fields()` is the const view; the writable one is the same storage.
            StateTable::Field* field = state.Find(state.Fields()[index].name);
            if (field == nullptr)
            {
                return util::Err(util::ErrorCode::InvalidData, "state field disappeared");
            }

            StateValue value = statement.literal;
            if (statement.source >= 0)
            {
                value = state.Fields()[static_cast<std::size_t>(statement.source)].value;
            }

            switch (statement.kind)
            {
                case Kind::Assign:
                    field->value = value;
                    break;
                case Kind::Add:
                    field->value = std::get<double>(field->value) + std::get<double>(value);
                    break;
                case Kind::Subtract:
                    field->value = std::get<double>(field->value) - std::get<double>(value);
                    break;
                case Kind::Toggle:
                    field->value = !std::get<bool>(field->value);
                    break;
            }
        }
        return util::Ok();
    }

    std::string Effect::ToString() const
    {
        std::string text;
        for (const Statement& statement : m_statements)
        {
            if (!text.empty())
            {
                text += "; ";
            }
            if (statement.kind == Kind::Toggle)
            {
                text += "toggle(state.#" + std::to_string(statement.field) + ")";
                continue;
            }
            text += "state.#" + std::to_string(statement.field);
            switch (statement.kind)
            {
                case Kind::Assign:
                    text += " = ";
                    break;
                case Kind::Add:
                    text += " += ";
                    break;
                default:
                    text += " -= ";
                    break;
            }
            text += statement.source >= 0 ? "state.#" + std::to_string(statement.source)
                                          : Spell(statement.literal);
        }
        return text;
    }

} // namespace cnahouse::world
