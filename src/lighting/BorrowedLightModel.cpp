// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/BorrowedLightModel.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::lighting
{
    namespace
    {
        constexpr float kPortalTransfer = 0.30F;
        constexpr float kSourceCap = 0.35F;

        [[nodiscard]] float LocalBrightness(const RoomLightState& state) noexcept
        {
            // The same saturating blend as `RoomLightState::Level`, but without `borrowed` and
            // without the ambient floor. One direct source can lift the unused range left by the
            // other; neither can exceed the renderer's normalised range.
            const float artificial = std::clamp(state.artificial, 0.0F, 1.0F);
            const float daylight = std::clamp(state.daylight, 0.0F, 1.0F);
            const float strongest = std::max(artificial, daylight);
            const float rest = std::min(artificial, daylight);
            return strongest + (1.0F - strongest) * rest;
        }
    } // namespace

    BorrowedLightModel::BorrowedLightModel(const world::WorldData& world,
                                           std::span<const visibility::PortalRuntime> portals)
        : portals_(portals)
        , firstHop_(world.Cells().size())
        , secondHop_(world.Cells().size())
    {
        const std::span<const world::Cell> cells = world.Cells();
        std::unordered_map<std::uint32_t, std::size_t> cellIndices;
        cellIndices.reserve(cells.size());
        for (std::size_t index = 0; index < cells.size(); ++index)
        {
            cellIndices.emplace(cells[index].id.Value(), index);
        }

        const std::span<const world::Portal> worldPortals = world.Portals();
        const std::size_t count = std::min(worldPortals.size(), portals.size());
        edges_.reserve(count * 2U);
        for (std::size_t portalIndex = 0; portalIndex < count; ++portalIndex)
        {
            const world::Portal& portal = worldPortals[portalIndex];
            const auto a = cellIndices.find(portal.cellA.Value());
            const auto b = cellIndices.find(portal.cellB.Value());
            if (a == cellIndices.end() || b == cellIndices.end())
            {
                continue;
            }
            const float portalArea = std::max(portal.Width(), 0.0F) * std::max(portal.Height(), 0.0F);
            const float areaA = world::WorldData::FootprintArea(cells[a->second]);
            const float areaB = world::WorldData::FootprintArea(cells[b->second]);
            if (portalArea <= 0.0F || areaA <= 0.0F || areaB <= 0.0F)
            {
                continue;
            }
            edges_.push_back(Edge{a->second, b->second, portalIndex, portalArea / areaB * kPortalTransfer});
            edges_.push_back(Edge{b->second, a->second, portalIndex, portalArea / areaA * kPortalTransfer});
        }
    }

    float BorrowedLightModel::Transfer(const Edge& edge) const noexcept
    {
        const visibility::PortalRuntime& portal = portals_[edge.portal];
        const float aperture = portal.Aperture();
        if (!portal.PassesLight() || !std::isfinite(aperture) || aperture <= 0.0F)
        {
            return 0.0F;
        }
        return edge.scale * std::clamp(aperture, 0.0F, 1.0F);
    }

    void BorrowedLightModel::Evaluate(std::span<const RoomLightState> states,
                                      std::span<float> borrowed) noexcept
    {
        std::fill(borrowed.begin(), borrowed.end(), 0.0F);
        const std::size_t cellCount = std::min({states.size(), borrowed.size(), firstHop_.size()});
        for (std::size_t source = 0; source < cellCount; ++source)
        {
            const float sourceBrightness = LocalBrightness(states[source]);
            if (sourceBrightness <= 0.0F)
            {
                continue;
            }

            std::fill(firstHop_.begin(), firstHop_.end(), 0.0F);
            std::fill(secondHop_.begin(), secondHop_.end(), 0.0F);
            const float sourceCap = sourceBrightness * kSourceCap;

            for (const Edge& edge : edges_)
            {
                if (edge.from == source && edge.to < cellCount)
                {
                    firstHop_[edge.to] += sourceBrightness * Transfer(edge);
                }
            }
            for (float& level : firstHop_)
            {
                level = std::min(level, sourceCap);
            }

            for (const Edge& edge : edges_)
            {
                if (edge.from < cellCount && edge.to < cellCount && edge.to != source)
                {
                    secondHop_[edge.to] += firstHop_[edge.from] * Transfer(edge);
                }
            }

            for (std::size_t target = 0; target < cellCount; ++target)
            {
                if (target == source)
                {
                    continue;
                }
                const float fromSource = std::min(firstHop_[target] + secondHop_[target], sourceCap);
                borrowed[target] = std::min(borrowed[target] + fromSource, 1.0F);
            }
        }
    }

} // namespace cnahouse::lighting
