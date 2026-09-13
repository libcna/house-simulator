// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace cnahouse::visibility
{

    /// @brief Which stock effect draws a chunk of @p layout.
    ///
    /// Not a decision made here: `docs/chunk-format.md` §3 says the layout COMES FROM the material's
    /// `effectTierS`, so this reads back what the content build already wrote down. `Skinned` has no
    /// layout because a skinned prop is an animated one and animated props are not batched (§17.4).
    [[nodiscard]] constexpr world::EffectTier EffectForLayout(world::ChunkLayout layout) noexcept
    {
        switch (layout)
        {
            case world::ChunkLayout::Basic:
                return world::EffectTier::Basic;
            case world::ChunkLayout::Dual:
                return world::EffectTier::DualTexture;
            case world::ChunkLayout::AlphaTest:
                return world::EffectTier::AlphaTest;
        }
        return world::EffectTier::Basic;
    }

    /// @brief §7.5's pass for a static chunk of @p layout.
    ///
    /// **`Pass::Transparent` is not reachable from a chunk, and that is the format's doing.** §7.5's
    /// transparent pass is *"glass, water, curtains, particles"*, and what decides it is the
    /// material's `alphaMode` (§22.1) -- which `chunks.bin` does not carry: `docs/chunk-format.md`
    /// §5 collapses §17.4's four-part key to the material id precisely because `alphaMode` is a
    /// field of the material. `HOUSE-00385` authored that table, but a chunk render item does not
    /// yet carry the resolved record; `HOUSE-00891` adds the runtime registry and `HOUSE-00898`
    /// consumes its blend mode. A `blend` material's chunks are therefore `basic` today and are
    /// drawn opaque, which is what the blockout's `BLOCKOUT_glass` is. Transparent items arrive
    /// through `Add` until then; guessing from a material NAME here would be a second opinion about
    /// a decision the data already states.
    [[nodiscard]] constexpr rendering::Pass PassForLayout(world::ChunkLayout layout) noexcept
    {
        // `alphatest` is the one alpha mode the format does carry, because it is a different vertex
        // layout and not just a different blend state -- §7.5's E, before F, so its cut-outs write
        // the depth the sorted pass tests against.
        return layout == world::ChunkLayout::AlphaTest ? rendering::Pass::AlphaTest
                                                       : rendering::Pass::OpaqueStatic;
    }

    /// @brief One thing to draw, carrying §25.1 step 5's sort key (`HOUSE-00675`).
    struct RenderItem
    {
        /// @brief §7.5's pass. The enum's order IS the frame order (`Renderer`), so it sorts first
        ///        and the sort can never reorder the passes themselves.
        rendering::Pass pass = rendering::Pass::OpaqueStatic;
        /// @brief Which stock effect object gets bound. Second because `MaterialBinder` keeps one
        ///        instance per KIND, so this is the granularity at which a shader actually changes.
        world::EffectTier effect = world::EffectTier::Basic;
        /// @brief Index into `world::ChunkLibrary::materials`. Third: a material change is a texture
        ///        rebind and a parameter write, which is cheaper than an effect change.
        std::uint16_t material = 0u;
        /// @brief The chunk index for a static item, the instance index for a dynamic one. Last,
        ///        because two draws sharing an effect and a material cost nothing to reorder -- so
        ///        this key is here to make the order DEFINED, not to make it fast.
        std::uint32_t geometry = 0u;
        /// @brief Metres from the eye to the bounds' centre. Only `Transparent` sorts by it.
        float depth = 0.0F;
    };

    /// @brief §25.1's step 5 -- *"sort by pass/effect/material"* -- as the frame's draw list.
    ///
    /// **Why the sort is visibility's job and not the renderer's.** The list is built from the
    /// visible set and from nothing else, and each pass then walks its own slice of it. Keeping the
    /// sort on this side means §71.2's *state changes* budget -- 90 typical, 300 a hard fail -- is a
    /// number this object can be asked for, on any machine, without a GPU in the room.
    ///
    /// **`Transparent` is the exception, and it is not negotiable.** Glass, water, curtains and
    /// particles blend with what is behind them, so §7.5 draws them back to front whatever their
    /// material is. Sorting that pass by material would be fewer state changes and a visibly wrong
    /// picture, which is the one trade a draw list must never make.
    ///
    /// **Buffers are reused.** `Clear` keeps the capacity, so a steady frame allocates nothing: the
    /// list is rebuilt from scratch every frame by design (the visible set is), and a per-frame
    /// allocation of 620 items would be the cost of that design rather than the design itself.
    class RenderList
    {
    public:
        /// @brief Empties the list, keeping the memory.
        void Clear() noexcept
        {
            items_.clear();
            sorted_ = true; // vacuously: an empty list is in order.
        }

        /// @brief Adds one item the caller has classified itself.
        ///
        /// This is how anything that is not a static chunk arrives: `HOUSE-00673`'s dynamic
        /// instances, and the transparent things `PassForLayout` explains it cannot recognise.
        void Add(const RenderItem& item)
        {
            items_.push_back(item);
            sorted_ = false;
        }

        /// @brief Adds §17.4's visible chunks, taking each one's key from the chunk itself.
        ///
        /// @param chunks indices into `library.chunks`, as `ChunkCuller::Chunks()` returns them.
        /// @param eye the camera position, for the depth the transparent pass sorts on.
        ///
        /// An index past the end of the library is skipped rather than dereferenced: the visible set
        /// and the library are two files that agree only because the content build says so
        /// (`worldHash`), and a mismatch must not be a read past the end of a vector.
        void AddChunks(const world::ChunkLibrary& library,
                       std::span<const std::uint32_t> chunks,
                       const Microsoft::Xna::Framework::Vector3& eye);

        /// @brief §25.1's step 5, in place.
        void Sort();

        /// @brief The whole list, in whatever order it is currently in.
        [[nodiscard]] std::span<const RenderItem> Items() const noexcept
        {
            return items_;
        }

        /// @brief The slice belonging to @p pass, sorting first if anything has been added since.
        ///
        /// **Non-const, and it sorts, deliberately.** The slice is a contiguous range only because
        /// `pass` is the first sort key, so answering this on an unsorted list means either a binary
        /// search over unsorted data or an empty span -- a wrong answer or a pass that silently
        /// draws nothing. Sorting instead makes the accessor total: whatever a caller does, what
        /// comes back is that pass's items in the order §7.5 wants them.
        [[nodiscard]] std::span<const RenderItem> ItemsFor(rendering::Pass pass);

        /// @brief Whether @p pass has anything in the list.
        ///
        /// Const and order-independent -- a linear scan, not a search -- because a pass is asked
        /// whether it is active BEFORE the frame is submitted, and sorting the list to answer that
        /// would make asking a question change what `Items` returns.
        [[nodiscard]] bool Has(rendering::Pass pass) const noexcept
        {
            for (const RenderItem& item : items_)
            {
                if (item.pass == pass)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::size_t Size() const noexcept
        {
            return items_.size();
        }

        [[nodiscard]] bool IsSorted() const noexcept
        {
            return sorted_;
        }

        /// @brief How many items the buffer can hold without allocating.
        ///
        /// Exposed so the reuse claim above is a testable one rather than a comment: `Clear` is
        /// meant to keep the memory, and the only way to see that from outside is to ask. Comparing
        /// the data POINTER across a clear does not work -- an allocator that has just been handed a
        /// block back will happily hand the same address out again.
        [[nodiscard]] std::size_t Capacity() const noexcept
        {
            return items_.capacity();
        }

        /// @brief How many times the bound state changes down the list -- what §71.2 budgets 90 of.
        ///
        /// COUNTED, not estimated: a change is an item whose `(pass, effect, material)` differs from
        /// its predecessor's. Const, and it does not sort, so that the number before `Sort` and the
        /// number after it are both askable -- which is the only way a test can show that the sort
        /// is doing something rather than that the content happened to be in order.
        [[nodiscard]] int StateChanges() const noexcept;

        /// @brief One draw call per item. §71.2 budgets 620.
        [[nodiscard]] int DrawCalls() const noexcept
        {
            return static_cast<int>(items_.size());
        }

    private:
        std::vector<RenderItem> items_;
        bool sorted_ = true;
    };

} // namespace cnahouse::visibility
