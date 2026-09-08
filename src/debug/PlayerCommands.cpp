// SPDX-License-Identifier: MIT
#include "cnahouse/debug/PlayerCommands.hpp"

#include <format>

namespace cnahouse::debug
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
    }

    void RegisterPlayerCommands(Console& console, PlayerCommandContext context)
    {
        console.Register("teleport",
                         "teleport <cellId>",
                         [context](std::span<const std::string_view> args) -> CommandResult
                         {
                             if (context.player == nullptr || context.world == nullptr)
                             {
                                 return {false, "teleport: no player in this session"};
                             }
                             if (args.size() != 1)
                             {
                                 return {false, "teleport: usage is 'teleport <cellId>'"};
                             }
                             const util::Id wanted = util::Intern(args[0]);
                             const world::Cell* cell = nullptr;
                             for (const world::Cell& candidate : context.world->Cells())
                             {
                                 if (candidate.id == wanted)
                                 {
                                     cell = &candidate;
                                     break;
                                 }
                             }
                             if (cell == nullptr || cell->boxes.empty())
                             {
                                 // Naming the thing that was not found, because a console user's next move is
                                 // to check their spelling and "error" does not help them do it.
                                 return {false, std::format("teleport: no cell '{}'", args[0])};
                             }

                             const world::Footprint& box = cell->boxes.front();
                             const float x = (box.minX + box.maxX) * 0.5F;
                             const float z = (box.minZ + box.maxZ) * 0.5F;
                             // The cell's FLOOR, not the middle of its volume: a body dropped into the middle
                             // of a room falls, which turns a debugging aid into a fall and a landing sound.
                             float floorY = 0.0F;
                             for (const world::Level& level : context.world->Levels())
                             {
                                 if (level.id == cell->level)
                                 {
                                     floorY = level.ffl;
                                     break;
                                 }
                             }
                             const float rise = context.player->Rise();
                             context.player->position = Xna::Vector3(x, floorY + rise + 0.005F, z);
                             context.player->velocity = Xna::Vector3();
                             context.player->fall = physics::FallState{};

                             if (context.tracker != nullptr)
                             {
                                 // §16.4's incremental test would answer a query about the attic with the
                                 // kitchen the player just left, expanded by 5 cm. Forgetting sends it to the
                                 // grid, which is what a teleport, a spawn and a save load all need.
                                 context.tracker->Forget();
                                 if (context.index != nullptr)
                                 {
                                     context.tracker->Update(
                                         *context.world, *context.index, context.player->position);
                                 }
                             }
                             return {true,
                                     std::format("teleport: {} at {:.2f}, {:.2f}, {:.2f}",
                                                 args[0],
                                                 context.player->position.X,
                                                 context.player->position.Y,
                                                 context.player->position.Z)};
                         });

        console.Register("noclip",
                         "noclip [on|off]",
                         [context](std::span<const std::string_view> args) -> CommandResult
                         {
                             if (context.player == nullptr)
                             {
                                 return {false, "noclip: no player in this session"};
                             }
                             if (args.empty())
                             {
                                 context.player->noclip = !context.player->noclip;
                             }
                             else if (args.size() == 1 && (args[0] == "on" || args[0] == "off"))
                             {
                                 context.player->noclip = args[0] == "on";
                             }
                             else
                             {
                                 return {false, "noclip: usage is 'noclip [on|off]'"};
                             }
                             if (!context.player->noclip)
                             {
                                 // Coming back to the world: drop whatever velocity the free
                                 // movement had, or the body shoots off the moment collision
                                 // starts caring about it again.
                                 context.player->velocity = Xna::Vector3();
                                 context.player->fall = physics::FallState{};
                             }
                             return {true, context.player->noclip ? "noclip: on" : "noclip: off"};
                         });
    }

} // namespace cnahouse::debug
