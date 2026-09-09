// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/world/WorldData.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cnahouse::world
{
    /// @brief One thing wrong with a world, named the way `validate_world.py` names it.
    struct ValidationProblem
    {
        std::int32_t rule = 0;
        std::string file;
        std::string path;
        std::string message;

        /// @brief `layout.cells.json:cells/12: the message`, which is what the Python gate prints.
        [[nodiscard]] std::string ToString() const;
    };

    /// @brief How much of §15.7 to run.
    enum class ValidationDepth
    {
        /// Everything whose cost is linear in the rows. What a debug build runs at load.
        Fast,
        /// ...plus rule 11's reachability proof, which samples a floor per interactable and is the
        /// only rule expensive enough to be worth not paying for on every run.
        Full,
    };

    /// @brief §15.7's rules over a loaded world, in C++ (`HOUSE-00357`).
    ///
    /// `tools/world/validate_world.py` is the gate that runs on every commit. This is the same
    /// statement made by the code that actually reads the house, and the reason for having both is
    /// that they keep finding each other wrong: `HOUSE-00378` found the loader refusing a portal
    /// the Python accepted, and `HOUSE-00388` found it silently dropping seven of §64.3's ten
    /// transmission classes. A rule stated once is a rule nobody checks.
    ///
    /// **Four of the twelve are already enforced before a `WorldData` exists**, and repeating them
    /// here would be checking the same thing twice in every debug run:
    ///
    /// * rule 1, ids unique across every kind — `WorldData::Create` refuses to build otherwise;
    /// * rule 2, boxes non-degenerate and inside their level — `WorldLoader` refuses the row;
    /// * rule 4, portal rectangles in both cells' planes — likewise, including the container and
    ///   in-the-wall cases;
    /// * rule 8, a flight's risers reaching the floor it claims — likewise.
    ///
    /// So a world that exists has passed those four, and this runs the seven that are properties
    /// of the **whole** world: 3, 5, 6, 7, 9, 10 and 11.
    ///
    /// What it does not yet see, because the C++ model does not carry it: the exterior file's
    /// gates, kerbs, paths and structures, the sky and weather tables, and a pet's start perch and
    /// bed. Those are checked by the Python gate alone until the loader reads them -- and rule 12,
    /// added by `HOUSE-00769`, is entirely about those rectangles, so it lives there too.
    class WorldValidator
    {
    public:
        [[nodiscard]] static std::vector<ValidationProblem>
        Validate(const WorldData& world, ValidationDepth depth = ValidationDepth::Fast);
    };
} // namespace cnahouse::world
