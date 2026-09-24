// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "cnahouse/world/ChunkData.hpp"

namespace cnahouse::world
{

    /// @brief One material's worth of a neighbourhood asset's mesh. `docs/neighbourhood-format.md`.
    ///
    /// The vertices arrive packed in the layout's own byte order and are kept that way, for
    /// `Chunk`'s reason: they are uploaded verbatim, and unpacking them into a C++ struct only to
    /// pack them again would be two conversions for no gain. The layout is `ChunkLayout::Basic`
    /// today and is READ rather than assumed, so a later one is a version bump and not a guess.
    struct NeighbourPrimitive
    {
        std::uint16_t material = 0u;
        ChunkLayout layout = ChunkLayout::Basic;
        /// @brief `false` for `u16` indices, `true` for `u32`.
        bool wideIndices = false;
        Microsoft::Xna::Framework::BoundingBox bounds;
        std::uint32_t vertexCount = 0u;
        std::vector<std::uint8_t> vertices;
        std::uint32_t indexCount = 0u;
        std::vector<std::uint8_t> indices;
    };

    /// @brief One asset of §11.4's neighbourhood, in ITS OWN SPACE.
    ///
    /// **Not in world space, which is the difference from a `Chunk` and the reason this format
    /// exists.** The retained neighbourhood places 118 instances of 34 assets; §26.1 picks an instance's LOD
    /// by its projected height and §26.2's impostor takes over at `impostorFrom`. A mesh with the placement
    /// baked into it can be neither shared between instances nor swapped for another band. The transform is
    /// applied at draw time from the `neighbourhood` row: `Matrix::CreateRotationY(yaw) *
    /// Matrix::CreateTranslation(position)`.
    struct NeighbourAsset
    {
        /// @brief The id a `layout.exterior.json` row's `asset` names.
        std::string asset;
        /// @brief The union of the primitives' boxes, in the asset's own space.
        Microsoft::Xna::Framework::BoundingBox bounds;
        std::vector<NeighbourPrimitive> primitives;

        /// @brief Vertex plus index bytes -- what uploading this asset once costs.
        [[nodiscard]] std::uint64_t GeometryBytes() const;
    };

    /// @brief Everything `content/world/neighbourhood.bin` holds.
    ///
    /// **The instances are deliberately not here.** `layout.exterior.json`'s `neighbourhood` rows
    /// carry the position, the yaw, the LOD group and the impostor distance, and `WorldLoader`
    /// already reads them into `ExteriorContents::neighbourhood`. A second copy would be a second
    /// answer to where a house stands.
    struct NeighbourhoodLibrary
    {
        std::string worldHash;
        std::vector<std::string> materials;
        /// @brief Sorted by `asset`, which is what `Find` relies on.
        std::vector<NeighbourAsset> assets;

        /// @brief The asset @p id names, or `nullptr` when the file does not hold it.
        ///
        /// A binary search, because the format states the order: a row resolves through this on
        /// every instance, and §11.4 has 122 of them.
        [[nodiscard]] const NeighbourAsset* Find(std::string_view id) const;

        /// @brief Total bytes of vertex and index data, which is what uploading the lot costs.
        [[nodiscard]] std::uint64_t GeometryBytes() const;
    };

} // namespace cnahouse::world
