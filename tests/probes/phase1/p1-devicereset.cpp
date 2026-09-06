// HOUSE-00108 -- can resources be rebuilt from CPU state after the graphics device is disturbed?
//
// The task named EasyGL's `DebugSimulateContextLoss`. That call lives in
// `CNA/Internal/Renderers/Common/IGraphicsRenderer.hpp` -- a `CNA/` include of a `CNA::Internal::`
// type -- so ADR-0001 forbids it twice over and `cna-house` can never call it, in a probe or
// anywhere else. Rather than reach for it, this probe asks the question the task was really for,
// through the XNA 4.0 surface that `cna-house` is allowed to use:
//
//   `GraphicsDevice::Reset()` plus the `DeviceLost` / `DeviceResetting` / `DeviceReset` events.
//
// What must hold for the Web port to be de-risked is that a resource created before a reset either
// survives it or can be rebuilt from CPU-side state afterwards, and that the events fire so a game
// knows when to do the rebuilding. Both are measured; neither is assumed.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <memory>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    class DeviceResetProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            r.note("scope",
                   "EasyGL's DebugSimulateContextLoss is a CNA::Internal call behind a CNA/ include, "
                   "so ADR-0001 forbids it. The XNA-legal GraphicsDevice::Reset path is probed "
                   "instead, which is the path cna-house would actually use.");

            // Build the CPU-side state a game would keep, and the GPU resources derived from it.
            std::vector<Color> texels(16 * 16);
            for (int y = 0; y < 16; ++y)
            {
                for (int x = 0; x < 16; ++x)
                {
                    texels[static_cast<size_t>(y) * 16 + x] = Color(static_cast<std::uint8_t>(x * 16),
                                                                    static_cast<std::uint8_t>(y * 16),
                                                                    static_cast<std::uint8_t>(128),
                                                                    static_cast<std::uint8_t>(255));
                }
            }
            const VertexPT quad[4] = {
                {Vector3(-1.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f)},
                {Vector3(1.0f, -1.0f, 0.0f), Vector2(1.0f, 1.0f)},
                {Vector3(1.0f, 1.0f, 0.0f), Vector2(1.0f, 0.0f)},
                {Vector3(-1.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f)},
            };
            const std::uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            });

            auto build = [&](std::unique_ptr<Texture2D>& tex,
                             std::unique_ptr<VertexBuffer>& vb,
                             std::unique_ptr<IndexBuffer>& ib)
            {
                tex = std::make_unique<Texture2D>(gd, 16, 16);
                tex->SetData(texels.data(), static_cast<int>(texels.size()));
                vb = std::make_unique<VertexBuffer>(gd, decl, 4, BufferUsage::WriteOnly);
                vb->SetData(quad, 4);
                ib = std::make_unique<IndexBuffer>(
                    gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
                ib->SetData(idx, 6);
            };

            std::unique_ptr<Texture2D> tex;
            std::unique_ptr<VertexBuffer> vb;
            std::unique_ptr<IndexBuffer> ib;
            build(tex, vb, ib);

            RenderTarget2D rt(gd, 32, 32, false, SurfaceFormat::Color, DepthFormat::None);
            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setLightingEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);
            fx.setTextureEnabledProperty(true);
            fx.setDiffuseColorProperty(Vector3::One);

            std::vector<Color> out(32 * 32);
            auto renderAndSample = [&]
            {
                fx.setTextureProperty(tex.get());
                gd.SetRenderTarget(&rt);
                gd.Clear(Color::Black);
                gd.setBlendStateProperty(BlendState::Opaque);
                gd.setDepthStencilStateProperty(DepthStencilState::None);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
                gd.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
                gd.SetVertexBuffer(vb.get());
                gd.setIndicesProperty(ib.get());
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p].Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(out.data(), static_cast<int>(out.size()));
                return out[16 * 32 + 16];
            };

            const Color before = renderAndSample();
            r.note("sampled centre before the reset",
                   "(" + std::to_string(before.getRProperty()) + "," + std::to_string(before.getGProperty()) +
                       "," + std::to_string(before.getBProperty()) + ")");
            r.check("the scene renders before the reset", before.getBProperty() > 100);

            // Subscribe before resetting, so the events are observed rather than inferred.
            int lost = 0, resetting = 0, reset = 0;
            gd.DeviceLost += [&lost](System::Object*, const System::EventArgs&) { ++lost; };
            gd.DeviceResetting += [&resetting](System::Object*, const System::EventArgs&) { ++resetting; };
            gd.DeviceReset += [&reset](System::Object*, const System::EventArgs&) { ++reset; };

            std::string err;
            bool didReset = true;
            try
            {
                gd.Reset();
            }
            catch (const std::exception& e)
            {
                didReset = false;
                err = e.what();
            }
            r.check("GraphicsDevice::Reset() is callable", didReset, err);
            r.note("events fired",
                   "DeviceLost " + std::to_string(lost) + ", DeviceResetting " + std::to_string(resetting) +
                       ", DeviceReset " + std::to_string(reset));
            r.check("a reset raises at least one of the three device events",
                    lost + resetting + reset > 0,
                    "a game that rebuilds resources on DeviceReset needs the event to arrive");

            if (didReset)
            {
                // The question that actually matters: after a reset, can the SAME CPU state produce a
                // working set of resources again?
                bool rebuilt = true;
                try
                {
                    build(tex, vb, ib);
                }
                catch (const std::exception& e)
                {
                    rebuilt = false;
                    err = e.what();
                }
                r.check("every resource can be rebuilt from CPU state after the reset", rebuilt, err);
                if (rebuilt)
                {
                    const Color after = renderAndSample();
                    char m[160];
                    std::snprintf(m,
                                  sizeof m,
                                  "before (%d,%d,%d), after (%d,%d,%d)",
                                  before.getRProperty(),
                                  before.getGProperty(),
                                  before.getBProperty(),
                                  after.getRProperty(),
                                  after.getGProperty(),
                                  after.getBProperty());
                    r.note("sampled centre after the rebuild", m);
                    r.check("the rebuilt scene renders identically to the original",
                            after.getRProperty() == before.getRProperty() &&
                                after.getGProperty() == before.getGProperty() &&
                                after.getBProperty() == before.getBProperty(),
                            m);
                }
            }

            return r.finish("p1-devicereset");
        }
    };

} // namespace

P1_MAIN(DeviceResetProbe, "p1-devicereset")
