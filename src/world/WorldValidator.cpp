// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldValidator.hpp"

#include "cnahouse/world/InteractableExpr.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cnahouse::world
{
    namespace
    {
        using util::Id;

        constexpr float kOverlapAreaTolerance = 1e-4F; // §15.7 rule 3: "more than 1 cm²"
        constexpr float kPlaneTolerance = 0.01F;
        constexpr float kDoorHeightLow = 1.98F; // §70.5, interior door leaf
        constexpr float kDoorHeightHigh = 2.10F;
        constexpr float kDoorWidthLow = 0.76F;
        constexpr float kDoorWidthHigh = 0.95F;
        constexpr float kStairSumLow = 0.600F; // §70.5, 2·rise + going
        constexpr float kStairSumHigh = 0.650F;
        constexpr float kCapsuleWidth = 0.62F;
        constexpr float kCapsuleHeight = 1.95F;
        constexpr float kSillLow = 0.50F; // §70.5, window sill over the room's own floor
        constexpr float kSillHigh = 1.10F;
        constexpr float kSwitchLow = 1.10F; // §70.5, light switch centre
        constexpr float kSwitchHigh = 1.30F;
        constexpr float kHandleLow = 0.95F; // §70.5, door handle centre
        constexpr float kHandleHigh = 1.10F;
        constexpr float kCorridorWidth = 0.90F; // §70.5, "a corridor ≥ 0.9 m wide"

        /// @brief Window types §70.5's habitable sill band does not govern (`HOUSE-00360`).
        ///
        /// The test is the declared `type`, never the measured sill: an exemption written as
        /// "high sills are fine" would exempt every window authored at the wrong height along
        /// with the ones that belong there. Anything not listed is checked, so a type added to
        /// §12.6 arrives inside the band and has to argue its way out.
        bool SillExempt(std::string_view type) noexcept
        {
            return type == "W_SIDELIGHT"   // glazing beside the front door; runs to the floor
                   || type == "W_PANEL"    // the sunroom's fixed full-height flanks
                   || type == "W_SLIDER"   // a door with glass in it
                   || type == "W_TRANSOM"  // sits above a door head
                   || type == "W_BATH"     // obscured privacy glazing, above eye level by design
                   || type == "W_BASEMENT" // a hopper in a window well
                   || type == "W_GABLE";   // a non-opening louvre in a gable end
        }

        /// @brief §70.5's minimum floor area for a room whose name says what it is for, or 0.
        ///
        /// The layout has no `function` field -- a WC and a study are both `CellKind::Room` -- and
        /// the `name` is the only place the data says which. Whole words, so "Bedroom 2" and
        /// "Master Bedroom" match and "Bedroomish" does not.
        float MinimumAreaFor(const std::string& name) noexcept
        {
            float minimum = 0.0F;
            std::string lowered;
            lowered.reserve(name.size());
            for (const char letter : name)
            {
                lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
            }
            std::size_t start = 0;
            while (start <= lowered.size())
            {
                const std::size_t end = std::min(lowered.find(' ', start), lowered.size());
                const std::string_view word(lowered.data() + start, end - start);
                if (word == "bedroom")
                {
                    minimum = std::max(minimum, 9.0F);
                }
                else if (word == "bathroom")
                {
                    minimum = std::max(minimum, 3.5F);
                }
                else if (word == "wc")
                {
                    minimum = std::max(minimum, 1.8F);
                }
                start = end + 1;
            }
            return minimum;
        }

        constexpr float kReach = 2.50F; // §15.7 rule 11
        constexpr float kEyeHeight = 1.60F;
        constexpr float kSampleStep = 0.25F;

        /// @brief `L0_FOYER`. §15.7 rule 5 names it, and a graph walk has to start somewhere.
        const char* const kRootCell = "L0_FOYER";

        [[nodiscard]] std::string Name(Id id)
        {
            const std::string_view text = util::IdRegistry::NameOf(id);
            return text.empty() ? std::string("<unnamed>") : std::string(text);
        }

        [[nodiscard]] std::string Number(float value, int places = 2)
        {
            std::string text = std::to_string(value);
            const std::size_t dot = text.find('.');
            if (dot != std::string::npos && text.size() > dot + static_cast<std::size_t>(places) + 1)
            {
                text.erase(dot + static_cast<std::size_t>(places) + 1);
            }
            return text;
        }

        struct Sink
        {
            std::vector<ValidationProblem> problems;

            void Add(std::int32_t rule, std::string file, std::string path, std::string message)
            {
                problems.push_back(
                    ValidationProblem{rule, std::move(file), std::move(path), std::move(message)});
            }
        };

        [[nodiscard]] float Overlap(float aLow, float aHigh, float bLow, float bHigh) noexcept
        {
            return std::max(0.0F, std::min(aHigh, bHigh) - std::max(aLow, bLow));
        }

        [[nodiscard]] float OverlapArea(const Footprint& a, const Footprint& b) noexcept
        {
            return Overlap(a.minX, a.maxX, b.minX, b.maxX) * Overlap(a.minZ, a.maxZ, b.minZ, b.maxZ);
        }

        /// @brief Rule 3. Two cells on one level do not overlap, unless one nests in the other.
        void CheckOverlaps(const WorldData& world, Sink& sink)
        {
            const std::span<const Cell> cells = world.Cells();
            for (std::size_t first = 0; first < cells.size(); ++first)
            {
                for (std::size_t second = first + 1; second < cells.size(); ++second)
                {
                    const Cell& a = cells[first];
                    const Cell& b = cells[second];
                    if (a.level != b.level)
                    {
                        continue;
                    }
                    // A declared sub-cell IS inside its parent, and says so (`HOUSE-00373`).
                    if (a.parent == b.id || b.parent == a.id)
                    {
                        continue;
                    }
                    float area = 0.0F;
                    for (const Footprint& one : a.boxes)
                    {
                        for (const Footprint& other : b.boxes)
                        {
                            area += OverlapArea(one, other);
                        }
                    }
                    if (area > kOverlapAreaTolerance)
                    {
                        sink.Add(3,
                                 "layout.cells.json",
                                 "cells/" + Name(b.id),
                                 "cells " + Name(a.id) + " and " + Name(b.id) + " overlap by " +
                                     Number(area) + " m² on level " + Name(a.level));
                    }
                }
            }
        }

        /// @brief Rule 5, both graphs: the portal graph from `L0_FOYER`, and the pet graph.
        void CheckConnectivity(const WorldData& world, Sink& sink)
        {
            const Id root = util::Intern(kRootCell);
            if (world.FindCell(root) == nullptr)
            {
                if (!world.Portals().empty())
                {
                    sink.Add(5,
                             "layout.cells.json",
                             "cells",
                             std::string("there is no ") + kRootCell +
                                 " and portals exist; §15.7 rule 5 walks the graph from it");
                }
                return;
            }

            std::set<Id> reached{root};
            std::vector<Id> pending{root};
            while (!pending.empty())
            {
                const Id current = pending.back();
                pending.pop_back();
                for (const std::uint32_t index : world.PortalsOf(current))
                {
                    const Portal& portal = world.Portals()[index];
                    if (!IsPassable(portal.kind))
                    {
                        continue;
                    }
                    const Id other = world.OtherSide(portal, current);
                    if (other.IsValid() && reached.insert(other).second)
                    {
                        pending.push_back(other);
                    }
                }
            }
            for (const Cell& cell : world.Cells())
            {
                if (cell.kind == CellKind::Void || reached.count(cell.id) == 1)
                {
                    continue;
                }
                sink.Add(5,
                         "layout.cells.json",
                         "cells/" + Name(cell.id),
                         "cell " + Name(cell.id) + " is not reachable from " + kRootCell +
                             " through open or door portals");
            }

            // ...and the pet graph, which has its own edges (`HOUSE-00389`).
            if (world.NavNodes().empty())
            {
                return;
            }
            std::map<Id, std::vector<Id>> neighbours;
            for (const NavEdge& edge : world.NavEdges())
            {
                neighbours[edge.a].push_back(edge.b);
                neighbours[edge.b].push_back(edge.a);
            }
            const Id start = world.NavNodes().front().id;
            std::set<Id> walked{start};
            std::vector<Id> queue{start};
            while (!queue.empty())
            {
                const Id current = queue.back();
                queue.pop_back();
                const auto found = neighbours.find(current);
                if (found == neighbours.end())
                {
                    continue;
                }
                for (const Id next : found->second)
                {
                    if (walked.insert(next).second)
                    {
                        queue.push_back(next);
                    }
                }
            }
            std::set<Id> stranded;
            for (const NavNode& node : world.NavNodes())
            {
                if (walked.count(node.id) == 0)
                {
                    stranded.insert(node.cell);
                }
            }
            if (!stranded.empty())
            {
                std::string rooms;
                for (const Id cell : stranded)
                {
                    rooms += (rooms.empty() ? "" : ", ") + Name(cell);
                }
                sink.Add(5,
                         "layout.nav.json",
                         "nodes",
                         "the pet graph is in more than one piece: nothing reaches " + rooms + " from " +
                             Name(start));
            }
        }

        /// @brief §64.3's row for a portal: by the leaf's type, and by kind for the two with none.
        ///
        /// The same table `validate_world.py` keys on, and it has to be: a solid-core door and a
        /// hollow one are the same portal kind and 8 dB apart (`HOUSE-00388`).
        [[nodiscard]] std::string TransmissionClassOf(const Portal& portal, const Opening* leaf)
        {
            if (IsAlwaysOpen(portal.kind))
            {
                return "opening";
            }
            const std::string type = leaf == nullptr ? std::string{} : Name(leaf->type);
            static const std::map<std::string, std::string> kByType{
                {"D_INT_PASSAGE", "door_hollow"},
                {"D_INT_PRIVACY", "door_hollow"},
                {"D_INT_LOW", "door_hollow"},
                {"D_STAIRHEAD", "door_hollow"},
                {"D_INT_SOLID", "door_solid"},
                {"D_DOUBLE", "door_solid"},
                {"D_ENTRY", "door_exterior"},
                {"D_EXT_SIDE", "door_exterior"},
                {"D_SLIDER", "slider_glass"},
                {"D_GARAGE", "door_garage"},
                {"D_APPLIANCE", "appliance"},
                {"H_LID", "appliance"},
                {"H_LOFT", "hatch_loft"},
                {"W_BASEMENT", "window_hopper"},
            };
            const auto found = kByType.find(type);
            if (found != kByType.end())
            {
                return found->second;
            }
            return type.rfind("W_", 0) == 0 ? "window_single" : "door_hollow";
        }

        /// @brief Rule 6, the references the C++ model carries.
        void CheckReferences(const WorldData& world, Sink& sink)
        {
            const bool haveMaterials = !world.Materials().empty();
            const auto material = [&](const Cell& cell, util::Id value, const char* field)
            {
                if (!haveMaterials || !value.IsValid() || world.FindMaterial(value) != nullptr)
                {
                    return;
                }
                sink.Add(6,
                         "layout.cells.json",
                         "cells/" + Name(cell.id) + "/" + field,
                         std::string(field) + " " + Name(value) + " is not a known material");
            };
            for (const Cell& cell : world.Cells())
            {
                material(cell, cell.floorMaterial, "floorMaterial");
                material(cell, cell.wallMaterial, "wallMaterial");
                material(cell, cell.ceilingMaterial, "ceilingMaterial");
                material(cell, cell.trimMaterial, "trimMaterial");

                // A cell's `lightGroups` is §28.1's per-frame index and rule 6 checks it both
                // ways: a group listed with no light of that group in the cell, and a light in
                // the cell whose group the list omits (`HOUSE-00381`).
                if (world.Lights().empty())
                {
                    continue;
                }
                std::set<Id> listed(cell.lightGroups.begin(), cell.lightGroups.end());
                std::set<Id> actual;
                for (const std::uint32_t index : world.LightsOf(cell.id))
                {
                    actual.insert(world.Lights()[index].group);
                }
                for (const Id group : listed)
                {
                    if (actual.count(group) == 0)
                    {
                        sink.Add(6,
                                 "layout.cells.json",
                                 "cells/" + Name(cell.id) + "/lightGroups",
                                 "cell " + Name(cell.id) + " lists group " + Name(group) +
                                     " and no light in that cell belongs to it");
                    }
                }
                for (const Id group : actual)
                {
                    if (listed.count(group) == 0)
                    {
                        sink.Add(6,
                                 "layout.cells.json",
                                 "cells/" + Name(cell.id) + "/lightGroups",
                                 "cell " + Name(cell.id) + " has lights in group " + Name(group) +
                                     " and does not list it; §28.1 walks this list once a frame");
                    }
                }
            }

            // A light is inside the cell it names. Automatic control is a property of the whole
            // group: one combined lightmap cannot be half wall-switched and half dusk-driven.
            std::map<Id, bool> duskByGroup;
            for (const Light& light : world.Lights())
            {
                const Cell* cell = world.FindCell(light.cell);
                if (cell == nullptr)
                {
                    sink.Add(6,
                             "layout.lights.json",
                             "lights/" + Name(light.id) + "/cell",
                             "light " + Name(light.id) + " is in cell " + Name(light.cell) +
                                 ", which does not exist");
                }
                const auto [found, inserted] = duskByGroup.emplace(light.group, light.duskSensor);
                if (!inserted && found->second != light.duskSensor)
                {
                    sink.Add(6,
                             "layout.lights.json",
                             "lights/" + Name(light.id) + "/duskSensor",
                             "group " + Name(light.group) +
                                 " mixes dusk-sensor and non-dusk fixtures; one group has one control");
                }
                if (light.duskSensor && light.defaultOn)
                {
                    sink.Add(6,
                             "layout.lights.json",
                             "lights/" + Name(light.id) + "/defaultOn",
                             "dusk-controlled light " + Name(light.id) +
                                 " must start off; the live sun decides its state");
                }
            }

            // A duct branch and its cells agree.
            std::set<Id> branches;
            for (const HvacBranch& branch : world.HvacBranches())
            {
                branches.insert(branch.id);
                for (const HvacRegister& grille : branch.registers)
                {
                    if (std::find(branch.cells.begin(), branch.cells.end(), grille.cell) ==
                        branch.cells.end())
                    {
                        sink.Add(6,
                                 "layout.levels.json",
                                 "hvac/" + Name(branch.id) + "/registers",
                                 "branch " + Name(branch.id) + " has a register in " + Name(grille.cell) +
                                     ", which is not one of its cells");
                    }
                }
            }
            if (!branches.empty())
            {
                for (const Cell& cell : world.Cells())
                {
                    if (!cell.thermal.ductBranch.IsValid())
                    {
                        continue;
                    }
                    if (branches.count(cell.thermal.ductBranch) == 0)
                    {
                        sink.Add(6,
                                 "layout.cells.json",
                                 "cells/" + Name(cell.id) + "/thermal/ductBranch",
                                 "cell " + Name(cell.id) + " is on duct branch " +
                                     Name(cell.thermal.ductBranch) + ", which is not declared");
                        continue;
                    }
                    const auto found = std::find_if(world.HvacBranches().begin(),
                                                    world.HvacBranches().end(),
                                                    [&cell](const HvacBranch& branch)
                                                    { return branch.id == cell.thermal.ductBranch; });
                    if (std::find(found->cells.begin(), found->cells.end(), cell.id) == found->cells.end())
                    {
                        sink.Add(6,
                                 "layout.cells.json",
                                 "cells/" + Name(cell.id) + "/thermal/ductBranch",
                                 "cell " + Name(cell.id) + " says it is on " + Name(cell.thermal.ductBranch) +
                                     " and that branch does not list it");
                    }
                }
            }

            // A switch's state fields ARE the groups it controls (`HOUSE-00384`).
            if (!world.Lights().empty())
            {
                std::set<Id> groups;
                for (const Light& light : world.Lights())
                {
                    groups.insert(light.group);
                }
                for (const Interactable& item : world.Interactables())
                {
                    if (item.kind != "light_switch")
                    {
                        continue;
                    }
                    for (const StateTable::Field& gang : item.state.Fields())
                    {
                        if (groups.count(util::Intern(gang.name)) == 0)
                        {
                            sink.Add(6,
                                     "interactables.json",
                                     "interactables/" + Name(item.id) + "/state/" + gang.name,
                                     "gang " + gang.name + " on " + Name(item.id) +
                                         " is not a known light group");
                        }
                    }
                }
            }

            // A leaf swings into a cell, and an emitter is heard from one.
            for (const Opening& opening : world.Openings())
            {
                if (opening.swing.IsValid() && world.FindCell(opening.swing) == nullptr)
                {
                    sink.Add(6,
                             "layout.openings.json",
                             "openings/" + Name(opening.id) + "/swing",
                             "leaf " + Name(opening.id) + " swings into " + Name(opening.swing) +
                                 ", which is not a cell");
                }
            }
            for (const AudioEmitter& emitter : world.AudioEmitters())
            {
                if (world.FindCell(emitter.cell) == nullptr)
                {
                    sink.Add(6,
                             "layout.audio.json",
                             "emitters/" + Name(emitter.id) + "/cell",
                             "emitter " + Name(emitter.id) + " is in cell " + Name(emitter.cell) +
                                 ", which does not exist");
                }
            }

            // A nav edge joins two nodes, and one that names a portal joins THAT portal's cells.
            std::map<Id, Id> nodeCell;
            for (const NavNode& node : world.NavNodes())
            {
                nodeCell.emplace(node.id, node.cell);
            }
            for (const NavEdge& edge : world.NavEdges())
            {
                const auto first = nodeCell.find(edge.a);
                const auto second = nodeCell.find(edge.b);
                if (first == nodeCell.end() || second == nodeCell.end())
                {
                    sink.Add(6,
                             "layout.nav.json",
                             "edges/" + Name(edge.a) + "-" + Name(edge.b),
                             "an edge joins " + Name(edge.a) + " and " + Name(edge.b) +
                                 ", and one of them is not a node");
                    continue;
                }
                if (!edge.portal.IsValid())
                {
                    continue;
                }
                const Portal* portal = world.FindPortal(edge.portal);
                if (portal == nullptr)
                {
                    sink.Add(6,
                             "layout.nav.json",
                             "edges/" + Name(edge.a) + "-" + Name(edge.b),
                             "the edge names portal " + Name(edge.portal) + ", which does not exist");
                    continue;
                }
                const std::set<Id> ends{portal->cellA, portal->cellB};
                const std::set<Id> joined{first->second, second->second};
                if (ends != joined)
                {
                    sink.Add(6,
                             "layout.nav.json",
                             "edges/" + Name(edge.a) + "-" + Name(edge.b),
                             "the edge joins " + Name(first->second) + " and " + Name(second->second) +
                                 " and names portal " + Name(edge.portal) + ", which joins " +
                                 Name(portal->cellA) + " and " + Name(portal->cellB) +
                                 "; a route through a door goes through that door");
                }
            }

            // A portal's `soundLoss` is a cache of §64.3's class (`HOUSE-00388`).
            if (!world.AudioTransmissions().empty() && !world.Openings().empty())
            {
                std::map<Id, const Opening*> leafOf;
                for (const Opening& opening : world.Openings())
                {
                    leafOf.emplace(opening.portal, &opening);
                }
                for (const Portal& portal : world.Portals())
                {
                    const auto found = leafOf.find(portal.id);
                    const std::string kind =
                        TransmissionClassOf(portal, found == leafOf.end() ? nullptr : found->second);
                    const AudioTransmission* row = world.FindTransmission(kind);
                    if (row == nullptr)
                    {
                        sink.Add(6,
                                 "layout.portals.json",
                                 "portals/" + Name(portal.id) + "/soundLoss",
                                 "portal " + Name(portal.id) + " is a '" + kind +
                                     "' and layout.audio.json declares no such class");
                        continue;
                    }
                    if (std::abs(row->closed - portal.soundLossClosed) > 5e-4F ||
                        std::abs(row->open - portal.soundLossOpen) > 5e-4F)
                    {
                        sink.Add(6,
                                 "layout.portals.json",
                                 "portals/" + Name(portal.id) + "/soundLoss",
                                 "portal " + Name(portal.id) + " carries " +
                                     Number(portal.soundLossClosed, 3) + " closed and its class '" + kind +
                                     "' says " + Number(row->closed, 3) + "; §64.3 is the one that decides");
                    }
                }
            }

            // The initial state starts things in cells that exist and heads for a real archetype.
            const InitialState& initial = world.GetInitialState();
            if (initial.player.cell.IsValid() && world.FindCell(initial.player.cell) == nullptr)
            {
                sink.Add(6,
                         "initialstate.json",
                         "player/cell",
                         "the player starts in cell " + Name(initial.player.cell) + ", which does not exist");
            }
            for (const PetStart& pet : initial.pets)
            {
                if (pet.cell.IsValid() && world.FindCell(pet.cell) == nullptr)
                {
                    sink.Add(6,
                             "initialstate.json",
                             "pets/" + Name(pet.id) + "/cell",
                             Name(pet.id) + " starts in cell " + Name(pet.cell) + ", which does not exist");
                }
            }
        }

        /// @brief Rule 7. Every leaf-bearing portal has exactly one leaf, and names it back.
        void CheckOpenings(const WorldData& world, Sink& sink)
        {
            if (world.Openings().empty())
            {
                return; // The bijection is a statement about two files and there is only one.
            }
            std::map<Id, Id> leafOf;
            for (const Opening& opening : world.Openings())
            {
                const auto inserted = leafOf.emplace(opening.portal, opening.id);
                if (!inserted.second)
                {
                    sink.Add(7,
                             "layout.openings.json",
                             "openings/" + Name(opening.id) + "/portal",
                             "portal " + Name(opening.portal) + " is already claimed by " +
                                 Name(inserted.first->second) + "; a portal carries one leaf");
                }
            }
            for (const Portal& portal : world.Portals())
            {
                if (IsAlwaysOpen(portal.kind))
                {
                    continue;
                }
                const auto found = leafOf.find(portal.id);
                if (found == leafOf.end())
                {
                    sink.Add(7,
                             "layout.portals.json",
                             "portals/" + Name(portal.id),
                             "portal " + Name(portal.id) + " can be shut and no opening declares its leaf");
                    continue;
                }
                if (!portal.aperture.IsValid())
                {
                    sink.Add(7,
                             "layout.portals.json",
                             "portals/" + Name(portal.id) + "/aperture",
                             "portal " + Name(portal.id) + " has a leaf (" + Name(found->second) +
                                 ") and no aperture, which this format reads as always open");
                }
                else if (portal.aperture != found->second)
                {
                    sink.Add(7,
                             "layout.portals.json",
                             "portals/" + Name(portal.id) + "/aperture",
                             "portal " + Name(portal.id) + " names aperture " + Name(portal.aperture) +
                                 " and is claimed by " + Name(found->second));
                }
            }
        }

        /// @brief Rule 9. A stack's cells exist, are listed once, and sit over its chase.
        void CheckPlumbing(const WorldData& world, Sink& sink)
        {
            for (const PlumbingStack& stack : world.PlumbingStacks())
            {
                if (stack.dropTo.IsValid() && world.FindCell(stack.dropTo) == nullptr)
                {
                    sink.Add(9,
                             "layout.levels.json",
                             "plumbing/" + Name(stack.id) + "/dropTo",
                             "stack " + Name(stack.id) + " drops to " + Name(stack.dropTo) +
                                 ", which is not a cell");
                }
                std::set<Id> listed;
                for (const Id cellId : stack.cells)
                {
                    const Cell* cell = world.FindCell(cellId);
                    if (cell == nullptr)
                    {
                        sink.Add(9,
                                 "layout.levels.json",
                                 "plumbing/" + Name(stack.id) + "/cells",
                                 "stack " + Name(stack.id) + " names cell " + Name(cellId) +
                                     ", which does not exist");
                        continue;
                    }
                    if (!listed.insert(cellId).second)
                    {
                        sink.Add(9,
                                 "layout.levels.json",
                                 "plumbing/" + Name(stack.id) + "/cells",
                                 "stack " + Name(stack.id) + " lists " + Name(cellId) +
                                     " twice; a cell drains to a stack once");
                    }
                    float area = 0.0F;
                    for (const Footprint& box : cell->boxes)
                    {
                        area += OverlapArea(box, stack.chase);
                    }
                    if (area <= kOverlapAreaTolerance)
                    {
                        sink.Add(9,
                                 "layout.levels.json",
                                 "plumbing/" + Name(stack.id) + "/cells",
                                 "stack " + Name(stack.id) + ": cell " + Name(cellId) +
                                     " does not overlap the chase, so it is not above the drop");
                    }
                }
            }
        }

        /// @brief Rule 10. §70.5's dimensional checks the layout alone decides.
        ///
        /// The same set `validate_world.py` runs, and deliberately so: the two exist to find each
        /// other wrong. What neither checks, and why, is in that tool's docstring -- rise
        /// consistency is a tautology, headroom needs a flight position the layout does not carry,
        /// and railings and sockets have no rows yet.
        void CheckRealism(const WorldData& world, Sink& sink)
        {
            for (const Opening& opening : world.Openings())
            {
                if (opening.kind != OpeningKind::Door)
                {
                    continue;
                }
                const Portal* portal = world.FindPortal(opening.portal);
                if (portal != nullptr &&
                    (portal->kind == PortalKind::ExteriorDoor || portal->kind == PortalKind::GarageDoor ||
                     portal->kind == PortalKind::Hatch || portal->kind == PortalKind::Slider ||
                     portal->kind == PortalKind::DoubleDoor))
                {
                    continue; // §70.5's band is the interior door's.
                }
                // ...and so are these two, which are `kind: door` and are not interior doors: a
                // refrigerator door is hung on an appliance, and the under-stair leaf is 1.55 m
                // because you duck into it. The test is the declared type, never the measurement.
                const std::string type = Name(opening.type);
                if (type == "D_APPLIANCE" || type == "D_INT_LOW")
                {
                    continue;
                }
                if (opening.leaf.height < kDoorHeightLow || opening.leaf.height > kDoorHeightHigh)
                {
                    sink.Add(10,
                             "layout.openings.json",
                             "openings/" + Name(opening.id) + "/leaf/height",
                             "door " + Name(opening.id) + ": leaf height " + Number(opening.leaf.height, 3) +
                                 " m is outside §70.5's 1.98-2.10 m");
                }
                if (opening.leaf.width < kDoorWidthLow || opening.leaf.width > kDoorWidthHigh)
                {
                    sink.Add(10,
                             "layout.openings.json",
                             "openings/" + Name(opening.id) + "/leaf/width",
                             "door " + Name(opening.id) + ": leaf width " + Number(opening.leaf.width, 3) +
                                 " m is outside §70.5's 0.76-0.95 m");
                }
            }

            for (const StairFlight& flight : world.Stairs())
            {
                const float blondel = 2.0F * flight.rise + flight.going;
                const Cell* from = world.FindCell(flight.fromCell);
                const Cell* to = world.FindCell(flight.toCell);
                const bool outdoors = (from != nullptr && from->kind == CellKind::Exterior) ||
                                      (to != nullptr && to->kind == CellKind::Exterior);
                // Exterior steps may exceed the upper bound -- a shallower, deeper step is the
                // right thing outdoors -- and may not go under it (`HOUSE-00379`).
                if (outdoors && blondel > kStairSumHigh)
                {
                    continue;
                }
                if (blondel < kStairSumLow - 1e-6F || blondel > kStairSumHigh + 1e-6F)
                {
                    sink.Add(10,
                             "layout.stairs.json",
                             "flights/" + Name(flight.id),
                             "flight " + Name(flight.id) + ": 2·" + Number(flight.rise, 4) + " + " +
                                 Number(flight.going, 4) + " = " + Number(blondel, 4) +
                                 " m, outside §70.5's 0.600-0.650 m");
                }
            }

            // Capsule clearance through every portal a person walks through.
            for (const Portal& portal : world.Portals())
            {
                if (!IsPassable(portal.kind) || portal.crouch || portal.kind == PortalKind::Hatch)
                {
                    continue;
                }
                if (portal.axis == PlaneAxis::Y)
                {
                    const float narrow = std::min(portal.Width(), portal.Height());
                    if (narrow < kCapsuleWidth - 1e-6F)
                    {
                        sink.Add(10,
                                 "layout.portals.json",
                                 "portals/" + Name(portal.id) + "/rect",
                                 "portal " + Name(portal.id) + ": " + Number(narrow, 3) +
                                     " m across its narrowest side, under the capsule's 0.62 m");
                    }
                    continue;
                }
                if (portal.Width() < kCapsuleWidth - 1e-6F)
                {
                    sink.Add(10,
                             "layout.portals.json",
                             "portals/" + Name(portal.id) + "/rect/u",
                             "portal " + Name(portal.id) + ": " + Number(portal.Width(), 3) +
                                 " m wide, under the player capsule's 0.62 m, and not crouch");
                }
                if (portal.Height() < kCapsuleHeight - 1e-6F)
                {
                    sink.Add(10,
                             "layout.portals.json",
                             "portals/" + Name(portal.id) + "/rect/v",
                             "portal " + Name(portal.id) + ": " + Number(portal.Height(), 3) +
                                 " m high, under the player capsule's 1.95 m, and not crouch");
                }
            }

            // §70.5's window sill, over the room's OWN floor (`HOUSE-00360`). Both sides, because
            // a borrowed-light window has a sill in each room and the floors need not be level.
            for (const Opening& opening : world.Openings())
            {
                if (opening.kind != OpeningKind::Window || SillExempt(Name(opening.type)))
                {
                    continue;
                }
                const Portal* portal = world.FindPortal(opening.portal);
                if (portal == nullptr || portal->axis == PlaneAxis::Y)
                {
                    continue; // a horizontal portal has no sill
                }
                for (const Id side : {portal->cellA, portal->cellB})
                {
                    const Cell* cell = world.FindCell(side);
                    if (cell == nullptr || cell->kind != CellKind::Room)
                    {
                        continue;
                    }
                    const util::Result<Extent> extent = world.ExtentOf(*cell);
                    if (!extent)
                    {
                        continue;
                    }
                    const float sill = portal->minV - extent.Value().floorY;
                    if (sill < kSillLow - 1e-6F || sill > kSillHigh + 1e-6F)
                    {
                        sink.Add(10,
                                 "layout.openings.json",
                                 "openings/" + Name(opening.id),
                                 "window " + Name(opening.id) + " (" + Name(opening.type) +
                                     "): its sill is " + Number(sill) + " m above " + Name(cell->id) +
                                     "'s floor, outside §70.5's 0.50-1.10 m");
                    }
                }
            }

            // §70.5's switch and handle centres. An interactable's focus point IS the thing you
            // reach for -- `interactables.json` says so of the door, "the focus is the handle".
            // The socket row is absent because there are no socket interactables to measure, and
            // a band over an empty set passes for the wrong reason.
            for (const Interactable& thing : world.Interactables())
            {
                float low = 0.0F;
                float high = 0.0F;
                const char* what = nullptr;
                if (thing.kind == "light_switch")
                {
                    low = kSwitchLow;
                    high = kSwitchHigh;
                    what = "switch centre";
                }
                else if (thing.kind == "door")
                {
                    low = kHandleLow;
                    high = kHandleHigh;
                    what = "handle centre";
                }
                if (what == nullptr)
                {
                    continue;
                }
                const Cell* cell = world.FindCell(thing.cell);
                if (cell == nullptr)
                {
                    continue; // rule 6
                }
                const util::Result<Extent> extent = world.ExtentOf(*cell);
                if (!extent)
                {
                    continue;
                }
                const float height = thing.focusPoint.Y - extent.Value().floorY;
                if (height < low - 1e-6F || height > high + 1e-6F)
                {
                    sink.Add(10,
                             "interactables.json",
                             "interactables/" + Name(thing.id) + "/focus/point",
                             Name(thing.id) + ": its " + what + " is " + Number(height) + " m above " +
                                 Name(cell->id) + "'s floor, outside §70.5's " + Number(low) + "-" +
                                 Number(high) + " m");
                }
            }

            // §70.5's floor area against what the room is for, and a corridor's width. One-sided:
            // a bedroom cannot be too large and §70.5 does not pretend otherwise.
            for (const Cell& cell : world.Cells())
            {
                if (cell.kind == CellKind::Corridor)
                {
                    // Width, not area, and on BOTH plan axes: an L-shaped corridor is a wide box
                    // and a narrow one, and the narrow one is the one you walk down sideways.
                    for (const Footprint& box : cell.boxes)
                    {
                        const float narrowest = std::min(box.maxX - box.minX, box.maxZ - box.minZ);
                        if (narrowest < kCorridorWidth - 1e-6F)
                        {
                            sink.Add(10,
                                     "layout.cells.json",
                                     "cells/" + Name(cell.id) + "/boxes",
                                     "corridor " + Name(cell.id) + ": a box is " + Number(narrowest) +
                                         " m across, under §70.5's 0.90 m");
                        }
                    }
                    continue;
                }
                if (cell.kind != CellKind::Room)
                {
                    continue;
                }
                const float minimum = MinimumAreaFor(cell.name);
                if (minimum <= 0.0F)
                {
                    continue;
                }
                const float area = WorldData::FootprintArea(cell);
                if (area < minimum - 1e-6F)
                {
                    sink.Add(10,
                             "layout.cells.json",
                             "cells/" + Name(cell.id),
                             "cell " + Name(cell.id) + " (" + cell.name + ") is " + Number(area) +
                                 " m², under §70.5's " + Number(minimum) +
                                 " m² for what its name says it is");
                }
            }

            // A light is inside the cell it names (`HOUSE-00383`).
            for (const Light& light : world.Lights())
            {
                const Cell* cell = world.FindCell(light.cell);
                if (cell == nullptr)
                {
                    continue; // rule 6
                }
                const bool inside = std::any_of(
                    cell->boxes.begin(),
                    cell->boxes.end(),
                    [&light](const Footprint& box)
                    { return box.Contains(light.position.X, light.position.Z, kPlaneTolerance); });
                if (!inside)
                {
                    sink.Add(10,
                             "layout.lights.json",
                             "lights/" + Name(light.id) + "/position",
                             "light " + Name(light.id) + " is not inside cell " + Name(cell->id) +
                                 "'s footprint");
                    continue;
                }
                const util::Result<Extent> extent = world.ExtentOf(*cell);
                if (extent && !extent.Value().Contains(light.position.Y, kPlaneTolerance))
                {
                    sink.Add(10,
                             "layout.lights.json",
                             "lights/" + Name(light.id) + "/position",
                             "light " + Name(light.id) + " is at y " + Number(light.position.Y) +
                                 ", outside cell " + Name(cell->id) + "'s vertical extent");
                }
            }

            // The player and the pets start somewhere real (`HOUSE-00395`).
            const InitialState& initial = world.GetInitialState();
            const auto startsInside =
                [&](const char* where, Id cellId, const Microsoft::Xna::Framework::Vector3& position)
            {
                const Cell* cell = world.FindCell(cellId);
                if (cell == nullptr)
                {
                    return;
                }
                const bool inside =
                    std::any_of(cell->boxes.begin(),
                                cell->boxes.end(),
                                [&position](const Footprint& box)
                                { return box.Contains(position.X, position.Z, kPlaneTolerance); });
                if (!inside)
                {
                    sink.Add(10,
                             "initialstate.json",
                             std::string(where) + "/position",
                             std::string(where) + " starts outside cell " + Name(cellId));
                    return;
                }
                const util::Result<Extent> extent = world.ExtentOf(*cell);
                if (extent && !extent.Value().Contains(position.Y, kPlaneTolerance))
                {
                    sink.Add(10,
                             "initialstate.json",
                             std::string(where) + "/position",
                             std::string(where) + " starts at y " + Number(position.Y) + ", outside cell " +
                                 Name(cellId) +
                                 "'s extent; a start above the floor is a frame spent falling");
                }
            };
            startsInside("player", initial.player.cell, initial.player.position);
        }

        /// @brief Rule 11. Every interactable's focus is reachable from a standing eye.
        void CheckReachability(const WorldData& world, Sink& sink)
        {
            for (const Interactable& item : world.Interactables())
            {
                const Cell* cell = world.FindCell(item.cell);
                if (cell == nullptr)
                {
                    continue; // rule 6
                }
                const util::Result<Extent> extent = world.ExtentOf(*cell);
                if (!extent)
                {
                    continue;
                }
                if (!world.CellContains(*cell, item.focusPoint, kPlaneTolerance))
                {
                    sink.Add(11,
                             "interactables.json",
                             "interactables/" + Name(item.id) + "/focus",
                             "interactable " + Name(item.id) + "'s focus is not inside cell " +
                                 Name(cell->id));
                    continue;
                }

                // Somewhere a capsule can STAND, in this cell or one next door: a shallow closet,
                // a meter cupboard and a serving hatch are all reached from the room next door.
                std::vector<const Cell*> rooms{cell};
                for (const std::uint32_t index : world.PortalsOf(cell->id))
                {
                    const Portal& portal = world.Portals()[index];
                    if (!IsPassable(portal.kind))
                    {
                        continue;
                    }
                    const Cell* neighbour = world.FindCell(world.OtherSide(portal, cell->id));
                    if (neighbour != nullptr)
                    {
                        rooms.push_back(neighbour);
                    }
                }

                bool reachable = false;
                for (const Cell* room : rooms)
                {
                    const util::Result<Extent> roomExtent = world.ExtentOf(*room);
                    if (!roomExtent ||
                        roomExtent.Value().ceilingY - roomExtent.Value().floorY < kCapsuleHeight - 1e-6F)
                    {
                        continue; // nobody can stand up in here
                    }
                    const float eyeY = roomExtent.Value().floorY + kEyeHeight;
                    for (const Footprint& box : room->boxes)
                    {
                        for (float x = box.minX + kSampleStep; x < box.maxX && !reachable; x += kSampleStep)
                        {
                            for (float z = box.minZ + kSampleStep; z < box.maxZ && !reachable;
                                 z += kSampleStep)
                            {
                                const float dx = item.focusPoint.X - x;
                                const float dy = item.focusPoint.Y - eyeY;
                                const float dz = item.focusPoint.Z - z;
                                if (dx * dx + dy * dy + dz * dz <= kReach * kReach)
                                {
                                    reachable = true;
                                }
                            }
                        }
                        if (reachable)
                        {
                            break;
                        }
                    }
                    if (reachable)
                    {
                        break;
                    }
                }
                if (!reachable)
                {
                    sink.Add(11,
                             "interactables.json",
                             "interactables/" + Name(item.id) + "/focus",
                             "interactable " + Name(item.id) +
                                 ": no position a player can stand in, in cell " + Name(cell->id) +
                                 " or through a portal from it, has a " + Number(kReach) +
                                 " m reach to its focus");
                }
            }
        }
    } // namespace

    std::string ValidationProblem::ToString() const
    {
        return file + ":" + path + ": " + message;
    }

    std::vector<ValidationProblem> WorldValidator::Validate(const WorldData& world, ValidationDepth depth)
    {
        Sink sink;
        CheckOverlaps(world, sink);
        CheckConnectivity(world, sink);
        CheckReferences(world, sink);
        CheckOpenings(world, sink);
        CheckPlumbing(world, sink);
        CheckRealism(world, sink);
        if (depth == ValidationDepth::Full)
        {
            CheckReachability(world, sink);
        }
        std::stable_sort(sink.problems.begin(),
                         sink.problems.end(),
                         [](const ValidationProblem& a, const ValidationProblem& b)
                         { return a.rule < b.rule; });
        return std::move(sink.problems);
    }
} // namespace cnahouse::world
