// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/RenderStates.hpp"

#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

namespace cnahouse::rendering
{

    const Microsoft::Xna::Framework::Graphics::RasterizerState& StateFor(CullPolicy policy) noexcept
    {
        namespace Gfx = Microsoft::Xna::Framework::Graphics;
        switch (policy)
        {
            case CullPolicy::ImportedFront:
            case CullPolicy::ProceduralFront:
                // MEASURED (`HOUSE-00071`): on an open single-sided quad, `CullClockwise` drew 8 960 px and
                // `CullCounterClockwise` drew 0. A CLOSED solid measures nothing here -- with the near
                // faces culled you see the far ones through them and the silhouette is identical, which is
                // why that probe needed a second fixture.
                return Gfx::RasterizerState::CullClockwise;
            case CullPolicy::Mirrored:
                return Gfx::RasterizerState::CullCounterClockwise;
            case CullPolicy::TwoSided:
                return Gfx::RasterizerState::CullNone;
        }
        return Gfx::RasterizerState::CullClockwise;
    }

    CullPolicy PolicyForDeterminant(float worldDeterminant, bool twoSided) noexcept
    {
        if (twoSided)
        {
            // Two-sided wins: a foliage card is meant to be seen from behind whether or not its
            // placement mirrors, and applying the mirror rule on top would cull it from one side.
            return CullPolicy::TwoSided;
        }
        return worldDeterminant < 0.0f ? CullPolicy::Mirrored : CullPolicy::ImportedFront;
    }

    const Microsoft::Xna::Framework::Graphics::DepthStencilState& DepthEqualReadOnly() noexcept
    {
        namespace Gfx = Microsoft::Xna::Framework::Graphics;

        struct Holder
        {
            Holder()
            {
                state.setDepthBufferEnableProperty(true);
                state.setDepthBufferWriteEnableProperty(false);
                state.setDepthBufferFunctionProperty(Gfx::CompareFunction::Equal);
            }

            Gfx::DepthStencilState state;
        };

        static const Holder state;
        return state.state;
    }

} // namespace cnahouse::rendering
