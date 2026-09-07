// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "cnahouse/util/Result.hpp"

/// @file
/// The closed expression vocabulary of `interactables.json` (`HOUSE-00354`, `cna-house.md` §50.3).
///
/// An action carries a `when` predicate and a `do` effect, both text in the data and both parsed
/// **at load** into a fixed tree over the interactable's own typed state. `docs/world-format.md`
/// gives the requirement in one sentence -- "parsed at load time into a fixed expression tree over
/// this interactable's own typed state fields, with an unknown token a load-time error" -- and this
/// is the grammar that satisfies it:
///
/// ```
/// when := or
/// or   := and ( "||" and )*
/// and  := cmp ( "&&" cmp )*
/// cmp  := "!" cmp | "(" or ")" | term [ ("=="|"!="|"<"|"<="|">"|">=") term ]
/// term := "state." IDENT | NUMBER | "true" | "false" | "'" TEXT "'"
///
/// do   := stmt ( ";" stmt )*
/// stmt := "state." IDENT ("="|"+="|"-=") term | "toggle" "(" "state." IDENT ")"
/// ```
///
/// **Why the operations are closed and the field names are not a list.** §50.4 has twelve
/// behaviour classes with about forty distinct state fields between them. A closed list of setter
/// verbs -- `setDoor`, `setFlow`, `setChannel` -- would be a forty-entry table that has to be kept
/// in step with every behaviour class, and adding the 641st interactable would stop being "a JSON
/// row". Checking each field against the row's **own** `state` object instead is closed by
/// construction: a typo is caught in exactly the same way, the message can name the fields that do
/// exist, and nothing has to be maintained twice.
///
/// **Types are checked at parse, not at run.** `state.doorOpen == 0.5` is refused when `doorOpen`
/// is declared `false` in the row's state, because a comparison between a boolean and a number is
/// a mistake that would otherwise be a silent `false` for the life of the build.

namespace cnahouse::world
{

    /// @brief One typed state field's value. The three types `interactables.json` uses.
    using StateValue = std::variant<bool, double, std::string>;

    /// @brief An interactable's declared state: the fields an expression may name.
    ///
    /// A vector, not a map: a row has a handful of fields, it is searched a few times at load and
    /// then indexed by slot at run time, and a stable order makes the "did you mean" list in an
    /// error message the same on every machine.
    class StateTable
    {
    public:
        struct Field
        {
            std::string name;
            StateValue value;
        };

        void Declare(std::string name, StateValue value);

        [[nodiscard]] const Field* Find(std::string_view name) const noexcept;
        [[nodiscard]] Field* Find(std::string_view name) noexcept;
        [[nodiscard]] std::size_t IndexOf(std::string_view name) const noexcept;
        [[nodiscard]] std::span<const Field> Fields() const noexcept;
        [[nodiscard]] bool Empty() const noexcept;

        /// @brief The declared names, joined by `, `, for a diagnostic.
        [[nodiscard]] std::string Names() const;

    private:
        std::vector<Field> m_fields;
    };

    /// @brief A parsed `when`.
    ///
    /// Holds no reference to the table it was parsed against; `Evaluate` takes the live one, which
    /// is the same shape by construction because the parse checked it.
    class Predicate
    {
    public:
        /// @brief An empty or absent `when` is the predicate that is always true.
        ///
        /// Most of the 640 rows have no condition -- a light switch is always operable -- and
        /// writing `"when": null` for them is what the file already does.
        [[nodiscard]] static Predicate AlwaysTrue();

        [[nodiscard]] static util::Result<Predicate> Parse(std::string_view text, const StateTable& state);

        [[nodiscard]] util::Result<bool> Evaluate(const StateTable& state) const;

        /// @brief The expression as text again, normalised. For diagnostics and round-trip tests.
        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] bool IsAlwaysTrue() const noexcept;

    private:
        friend class ExpressionParser;

        enum class Op : std::uint8_t
        {
            True,
            Not,
            And,
            Or,
            Equal,
            NotEqual,
            Less,
            LessEqual,
            Greater,
            GreaterEqual,
            /// A bare `state.flag` used as a condition.
            Truth,
        };

        struct Node
        {
            Op op = Op::True;
            /// Children, as indices into the flat node list. Indices rather than pointers so the
            /// tree is one allocation and copying a `Predicate` copies a vector.
            std::int32_t left = -1;
            std::int32_t right = -1;
            /// For a leaf: the state field's index, or -1 for a literal.
            std::int32_t field = -1;
            StateValue literal{false};
        };

        void Print(std::int32_t index, std::string& text) const;
        static void PrintLeaf(const Node& node, std::string& text);

        std::vector<Node> m_nodes;
        std::int32_t m_root = -1;
    };

    /// @brief A parsed `do`: a sequence of assignments over the interactable's own state.
    class Effect
    {
    public:
        [[nodiscard]] static Effect Nothing();

        [[nodiscard]] static util::Result<Effect> Parse(std::string_view text, const StateTable& state);

        /// @brief Applies every statement, in order, to @p state.
        [[nodiscard]] util::Result<void> Apply(StateTable& state) const;

        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] bool IsEmpty() const noexcept;

    private:
        friend class ExpressionParser;

        enum class Kind : std::uint8_t
        {
            Assign,
            Add,
            Subtract,
            Toggle,
        };

        struct Statement
        {
            Kind kind = Kind::Assign;
            std::int32_t field = -1;
            /// For `Assign`/`Add`/`Subtract`: the literal, or the index of the field to read.
            StateValue literal{false};
            std::int32_t source = -1;
        };

        std::vector<Statement> m_statements;
    };

} // namespace cnahouse::world
