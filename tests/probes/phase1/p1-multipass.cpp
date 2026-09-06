// HOUSE-00079 -- multi-pass additive lighting: the same geometry drawn twice, `Opaque` then
// `Additive` with a depth-EQUAL second pass. This is the Tier S lighting mechanism, so both of its
// assumptions are measured rather than assumed:
//
//   * the second pass reaches EVERY pixel the first pass wrote -- if the depth-equal comparison
//     z-fought, some pixels would be rejected and a room would show additive speckle;
//   * the sum is exact -- 60 + 40 must read back as exactly 100, not 99 or 101, or a three-light
//     room drifts.
//
// The fixture uses a quad tilted in depth, not a screen-parallel one. A screen-parallel quad has a
// constant interpolated depth and would pass a depth-equal test even on hardware that computes
// depth inconsistently between passes; a tilted quad makes every pixel a different depth value and
// is what actually exercises invariance.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kN = 64;

    class MultipassProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            // A quad tilted in Z: every pixel gets a distinct interpolated depth.
            struct V
            {
                Vector3 Position;
            };

            const V verts[4] = {
                {Vector3(-0.9f, 0.9f, 0.10f)},
                {Vector3(0.9f, 0.9f, 0.60f)},
                {Vector3(0.9f, -0.9f, 0.90f)},
                {Vector3(-0.9f, -0.9f, 0.40f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
            });
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(verts, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(indices, 6);

            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setLightingEnabledProperty(false);
            fx.setTextureEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);

            // Exactly representable as bytes, and their sum is too: 60 + 40 = 100.
            const float kFirst = 60.0f / 255.0f;
            const float kSecond = 40.0f / 255.0f;

            DepthStencilState depthEqual;
            depthEqual.setDepthBufferEnableProperty(true);
            depthEqual.setDepthBufferWriteEnableProperty(false);
            depthEqual.setDepthBufferFunctionProperty(CompareFunction::Equal);

            RenderTarget2D rt(gd, kN, kN, false, SurfaceFormat::Color, DepthFormat::Depth24);
            gd.SetRenderTarget(&rt);
            gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
            gd.setRasterizerStateProperty(RasterizerState::CullNone);

            auto draw = [&]
            {
                gd.SetVertexBuffer(&vb);
                gd.setIndicesProperty(&ib);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p].Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                }
            };

            // pass 1: opaque, writes depth
            gd.setBlendStateProperty(BlendState::Opaque);
            gd.setDepthStencilStateProperty(DepthStencilState::Default);
            fx.setDiffuseColorProperty(Vector3(kFirst, kFirst, kFirst));
            draw();

            std::vector<Color> after1(static_cast<size_t>(kN) * kN);
            gd.SetRenderTarget(nullptr);
            rt.GetData(after1.data(), static_cast<int>(after1.size()));

            int covered = 0;
            for (const auto& c : after1)
            {
                if (c.getRProperty() == 60)
                {
                    ++covered;
                }
            }
            r.check("the first pass wrote exactly the value it was given",
                    covered > 0,
                    std::to_string(covered) + " px at 60");

            // pass 2: additive, depth-equal, no depth write. Re-binding the target must PRESERVE its
            // contents, so the usage is explicit rather than the DiscardContents default.
            RenderTarget2D rt2(gd,
                               kN,
                               kN,
                               false,
                               SurfaceFormat::Color,
                               DepthFormat::Depth24,
                               0,
                               RenderTargetUsage::PreserveContents);
            gd.SetRenderTarget(&rt2);
            gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
            gd.setBlendStateProperty(BlendState::Opaque);
            gd.setDepthStencilStateProperty(DepthStencilState::Default);
            fx.setDiffuseColorProperty(Vector3(kFirst, kFirst, kFirst));
            draw();
            gd.setBlendStateProperty(BlendState::Additive);
            gd.setDepthStencilStateProperty(depthEqual);
            fx.setDiffuseColorProperty(Vector3(kSecond, kSecond, kSecond));
            draw();
            gd.SetRenderTarget(nullptr);

            std::vector<Color> both(after1.size());
            rt2.GetData(both.data(), static_cast<int>(both.size()));

            int coveredOnce = 0, exact = 0, onlyFirst = 0, other = 0;
            int worst = 0;
            for (size_t i = 0; i < both.size(); ++i)
            {
                const int v = both[i].getRProperty();
                if (v == 0)
                {
                    continue;
                }
                ++coveredOnce;
                if (v == 100)
                {
                    ++exact;
                }
                else if (v == 60)
                {
                    ++onlyFirst; // the depth-equal pass was rejected here
                }
                else
                {
                    ++other;
                    const int d = v > 100 ? v - 100 : 100 - v;
                    if (d > worst)
                    {
                        worst = d;
                    }
                }
            }
            r.note("second-pass coverage",
                   std::to_string(coveredOnce) + " lit px: " + std::to_string(exact) + " at 100, " +
                       std::to_string(onlyFirst) + " still at 60, " + std::to_string(other) + " other");
            r.check("a depth-equal second pass draws EVERY pixel of the first",
                    onlyFirst == 0,
                    std::to_string(onlyFirst) + " pixels rejected by the depth-equal test");
            r.check("the additive sum is exact (60 + 40 = 100 on every covered pixel)",
                    exact == coveredOnce && coveredOnce > 0,
                    std::to_string(exact) + "/" + std::to_string(coveredOnce) +
                        "; worst deviation among the rest " + std::to_string(worst));
            r.check("the two-pass coverage equals the single-pass coverage",
                    coveredOnce == covered,
                    std::to_string(coveredOnce) + " vs " + std::to_string(covered));

            // Three passes, because Tier S wants three lights: 60 + 40 + 40 = 140, still exact.
            gd.SetRenderTarget(&rt2);
            gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
            gd.setBlendStateProperty(BlendState::Opaque);
            gd.setDepthStencilStateProperty(DepthStencilState::Default);
            fx.setDiffuseColorProperty(Vector3(kFirst, kFirst, kFirst));
            draw();
            gd.setBlendStateProperty(BlendState::Additive);
            gd.setDepthStencilStateProperty(depthEqual);
            fx.setDiffuseColorProperty(Vector3(kSecond, kSecond, kSecond));
            draw();
            draw();
            gd.SetRenderTarget(nullptr);
            rt2.GetData(both.data(), static_cast<int>(both.size()));
            int exact3 = 0, lit3 = 0;
            for (const auto& c : both)
            {
                if (c.getRProperty() == 0)
                {
                    continue;
                }
                ++lit3;
                if (c.getRProperty() == 140)
                {
                    ++exact3;
                }
            }
            r.check("three passes still sum exactly (60 + 40 + 40 = 140)",
                    exact3 == lit3 && lit3 > 0,
                    std::to_string(exact3) + "/" + std::to_string(lit3));

            return r.finish("p1-multipass");
        }
    };

} // namespace

P1_MAIN(MultipassProbe, "p1-multipass")
