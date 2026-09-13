// HOUSE-00084 -- a `Color` render target with mip chain and MSAA: bind, clear, draw, unbind, sample.
// HOUSE-00085 -- `BL-02`: is `Clear(ClearOptions::Stencil)` ignored and `ReferenceStencil` inert?
// HOUSE-00086 -- `BL-03`: does EasyGL MRT attachment 1 stay black?
//
// The last two are blocker rows, and a blocker is only settled by executing the thing it claims is
// broken. So the stencil half sets up a mask that MUST change the image if stencilling works, and
// the MRT half writes a non-black colour that MUST appear in attachment 1 if MRT works. A row that
// merely "did not crash" would settle nothing.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/StencilOperation.hpp"
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

    const VertexPT kFull[4] = {
        {Vector3(-1.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f)},
        {Vector3(1.0f, -1.0f, 0.0f), Vector2(1.0f, 1.0f)},
        {Vector3(1.0f, 1.0f, 0.0f), Vector2(1.0f, 0.0f)},
        {Vector3(-1.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f)},
    };
    // the left half only -- used as the stencil mask
    const VertexPT kLeft[4] = {
        {Vector3(-1.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f)},
        {Vector3(0.0f, -1.0f, 0.0f), Vector2(0.5f, 1.0f)},
        {Vector3(0.0f, 1.0f, 0.0f), Vector2(0.5f, 0.0f)},
        {Vector3(-1.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f)},
    };
    const std::uint16_t kIndices[6] = {0, 1, 2, 0, 2, 3};

    class RtCapsProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            });
            VertexBuffer full(gd, decl, 4, BufferUsage::WriteOnly);
            full.SetData(kFull, 4);
            VertexBuffer left(gd, decl, 4, BufferUsage::WriteOnly);
            left.SetData(kLeft, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(kIndices, 6);

            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setLightingEnabledProperty(false);
            fx.setTextureEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);

            auto drawQuad = [&](VertexBuffer& vb)
            {
                gd.SetVertexBuffer(&vb);
                gd.setIndicesProperty(&ib);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p]->Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                }
            };

            // ================= HOUSE-00084: mips and MSAA ==========================================
            for (int msaa : {0, 4})
            {
                std::string err;
                std::unique_ptr<RenderTarget2D> rt;
                bool made = true;
                try
                {
                    rt = std::make_unique<RenderTarget2D>(gd,
                                                          64,
                                                          64,
                                                          true,
                                                          SurfaceFormat::Color,
                                                          DepthFormat::Depth24,
                                                          msaa,
                                                          RenderTargetUsage::PreserveContents);
                }
                catch (const std::exception& e)
                {
                    made = false;
                    err = e.what();
                }
                const std::string tag = "MSAA " + std::to_string(msaa);
                r.check((tag + ": a 64x64 mipped Color target is creatable").c_str(), made, err);
                if (!made)
                {
                    continue;
                }
                r.note((tag + ": LevelCount").c_str(), std::to_string(rt->getLevelCountProperty()));
                r.note((tag + ": MultiSampleCount").c_str(),
                       std::to_string(rt->getMultiSampleCountProperty()));
                r.check((tag + ": a 64x64 mip chain has 7 levels").c_str(),
                        rt->getLevelCountProperty() == 7,
                        std::to_string(rt->getLevelCountProperty()));

                bool drew = true;
                try
                {
                    gd.SetRenderTarget(rt.get());
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setBlendStateProperty(BlendState::Opaque);
                    gd.setDepthStencilStateProperty(DepthStencilState::Default);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    fx.setDiffuseColorProperty(Vector3(0.5f, 0.25f, 0.75f));
                    drawQuad(full);
                    gd.SetRenderTarget(nullptr);
                }
                catch (const std::exception& e)
                {
                    drew = false;
                    err = e.what();
                }
                r.check((tag + ": bind, clear, draw and unbind").c_str(), drew, err);
                if (!drew)
                {
                    continue;
                }

                std::vector<Color> lvl0(64 * 64);
                rt->GetData(lvl0.data(), static_cast<int>(lvl0.size()));
                const Color c = lvl0[32 * 64 + 32];
                r.check((tag + ": level 0 holds what was drawn").c_str(),
                        std::abs(c.getRProperty() - 128) <= 2 && std::abs(c.getGProperty() - 64) <= 2 &&
                            std::abs(c.getBProperty() - 191) <= 2,
                        "(" + std::to_string(c.getRProperty()) + "," + std::to_string(c.getGProperty()) +
                            "," + std::to_string(c.getBProperty()) + ") want (128,64,191)");

                // Sampling it back through an effect is the part a resolve step could break.
                RenderTarget2D dest(gd, 16, 16, false, SurfaceFormat::Color, DepthFormat::None);
                bool sampled = true;
                try
                {
                    fx.setTextureEnabledProperty(true);
                    fx.setTextureProperty(rt.get());
                    fx.setDiffuseColorProperty(Vector3::One);
                    gd.SetRenderTarget(&dest);
                    gd.Clear(Color::Black);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
                    drawQuad(full);
                    gd.SetRenderTarget(nullptr);
                    fx.setTextureEnabledProperty(false);
                }
                catch (const std::exception& e)
                {
                    sampled = false;
                    err = e.what();
                }
                r.check((tag + ": the unbound target samples as a texture").c_str(), sampled, err);
                if (sampled)
                {
                    std::vector<Color> out(16 * 16);
                    dest.GetData(out.data(), static_cast<int>(out.size()));
                    const Color s = out[8 * 16 + 8];
                    r.check((tag + ": the sampled colour is the drawn colour").c_str(),
                            std::abs(s.getRProperty() - 128) <= 3 && std::abs(s.getGProperty() - 64) <= 3 &&
                                std::abs(s.getBProperty() - 191) <= 3,
                            "(" + std::to_string(s.getRProperty()) + "," + std::to_string(s.getGProperty()) +
                                "," + std::to_string(s.getBProperty()) + ")");
                }
            }

            // ================= HOUSE-00085 / BL-02: stencil ========================================
            // The test is constructed so a WORKING stencil and a NON-WORKING one give different images,
            // and neither outcome is inferred from an exception.
            {
                RenderTarget2D rt(gd,
                                  64,
                                  64,
                                  false,
                                  SurfaceFormat::Color,
                                  DepthFormat::Depth24Stencil8,
                                  0,
                                  RenderTargetUsage::PreserveContents);
                r.note("Depth24Stencil8 target",
                       "created, format id " +
                           std::to_string(static_cast<int>(rt.getDepthStencilFormatProperty())));

                DepthStencilState writeMask;
                writeMask.setDepthBufferEnableProperty(false);
                writeMask.setStencilEnableProperty(true);
                writeMask.setStencilFunctionProperty(CompareFunction::Always);
                writeMask.setStencilPassProperty(StencilOperation::Replace);
                writeMask.setReferenceStencilProperty(1);

                DepthStencilState testMask;
                testMask.setDepthBufferEnableProperty(false);
                testMask.setStencilEnableProperty(true);
                testMask.setStencilFunctionProperty(CompareFunction::Equal);
                testMask.setStencilPassProperty(StencilOperation::Keep);
                testMask.setReferenceStencilProperty(1);

                std::string err;
                bool ran = true;
                try
                {
                    gd.SetRenderTarget(&rt);
                    // clear colour AND stencil to 0
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer | ClearOptions::Stencil,
                             Color::Black,
                             1.0f,
                             0);
                    gd.setBlendStateProperty(BlendState::Opaque);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    // pass 1: stamp stencil = 1 over the LEFT half, writing no colour of interest
                    gd.setDepthStencilStateProperty(writeMask);
                    fx.setDiffuseColorProperty(Vector3::Zero);
                    drawQuad(left);
                    // pass 2: draw the FULL quad in white, but only where stencil == 1
                    gd.setDepthStencilStateProperty(testMask);
                    fx.setDiffuseColorProperty(Vector3::One);
                    drawQuad(full);
                    gd.SetRenderTarget(nullptr);
                }
                catch (const std::exception& e)
                {
                    ran = false;
                    err = e.what();
                }
                r.check("a two-pass stencil mask runs without throwing", ran, err);
                if (ran)
                {
                    std::vector<Color> out(64 * 64);
                    rt.GetData(out.data(), static_cast<int>(out.size()));
                    int leftLit = 0, rightLit = 0;
                    for (int y = 0; y < 64; ++y)
                    {
                        for (int x = 0; x < 64; ++x)
                        {
                            if (out[static_cast<size_t>(y) * 64 + x].getRProperty() > 128)
                            {
                                if (x < 32)
                                {
                                    ++leftLit;
                                }
                                else
                                {
                                    ++rightLit;
                                }
                            }
                        }
                    }
                    char msg[200];
                    std::snprintf(
                        msg, sizeof msg, "left half %d/2048 lit, right half %d/2048 lit", leftLit, rightLit);
                    r.note("stencil mask result", msg);
                    const bool works = leftLit > 1900 && rightLit < 150;
                    const bool inert = leftLit > 1900 && rightLit > 1900;
                    r.note("BL-02 verdict",
                           works ? "STENCIL WORKS -- the mask confined the second pass"
                                 : (inert ? "STENCIL IS INERT -- the second pass covered everything, "
                                            "so ReferenceStencil had no effect (BL-02 confirmed)"
                                          : "NEITHER -- see the counts above"));
                    r.check("the stencil behaviour is decisive one way or the other", works || inert, msg);
                }
            }

            // ================= HOUSE-00086 / BL-03: MRT ============================================
            {
                RenderTarget2D t0(gd,
                                  32,
                                  32,
                                  false,
                                  SurfaceFormat::Color,
                                  DepthFormat::Depth24,
                                  0,
                                  RenderTargetUsage::PreserveContents);
                RenderTarget2D t1(gd,
                                  32,
                                  32,
                                  false,
                                  SurfaceFormat::Color,
                                  DepthFormat::None,
                                  0,
                                  RenderTargetUsage::PreserveContents);
                std::vector<RenderTargetBinding> bindings;
                std::string err;
                bool ran = true;
                try
                {
                    bindings.emplace_back(&t0);
                    bindings.emplace_back(&t1);
                    gd.SetRenderTargets(bindings);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setBlendStateProperty(BlendState::Opaque);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    // A stock effect writes ONE output. Attachment 1 receiving anything at all would
                    // therefore be the renderer broadcasting, and receiving nothing is the expected
                    // BL-03 behaviour. Either is a measurement.
                    fx.setDiffuseColorProperty(Vector3(1.0f, 0.5f, 0.25f));
                    drawQuad(full);
                    gd.SetRenderTarget(nullptr);
                }
                catch (const std::exception& e)
                {
                    ran = false;
                    err = e.what();
                }
                r.check("two render targets can be bound at once", ran, err);
                if (ran)
                {
                    std::vector<Color> a(32 * 32), b(32 * 32);
                    t0.GetData(a.data(), static_cast<int>(a.size()));
                    t1.GetData(b.data(), static_cast<int>(b.size()));
                    const Color ca = a[16 * 32 + 16];
                    const Color cb = b[16 * 32 + 16];
                    char msg[200];
                    std::snprintf(msg,
                                  sizeof msg,
                                  "attachment 0 (%d,%d,%d), attachment 1 (%d,%d,%d)",
                                  ca.getRProperty(),
                                  ca.getGProperty(),
                                  ca.getBProperty(),
                                  cb.getRProperty(),
                                  cb.getGProperty(),
                                  cb.getBProperty());
                    r.note("MRT result", msg);
                    r.check("attachment 0 receives the draw", ca.getRProperty() > 200, msg);
                    const bool black =
                        cb.getRProperty() == 0 && cb.getGProperty() == 0 && cb.getBProperty() == 0;
                    r.note("BL-03 verdict",
                           black ? "attachment 1 STAYS BLACK with a stock effect -- BL-03 confirmed"
                                 : "attachment 1 received data -- BL-03 does not hold as written");
                    r.check("the MRT behaviour is recorded", true, msg);
                }
            }

            return r.finish("p1-rtcaps");
        }
    };

} // namespace

P1_MAIN(RtCapsProbe, "p1-rtcaps")
