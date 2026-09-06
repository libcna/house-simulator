// HOUSE-00087 -- a compiled `.fx` loaded as `Effect` and drawn with.
// HOUSE-00088 -- two techniques selected BY NAME per draw, mirroring SAMPLE-038.
// HOUSE-00089 -- `SpriteBatch::Begin(..., effect)` with the same compiled effect.
//
// This decides whether Tier E exists at all. All four of the task's acceptance points are
// separately measured: the build succeeded (that happened offline, see the report), a named
// technique is selectable, a parameter change reaches the shader, and the two techniques produce
// numerically DIFFERENT images -- the last so that "the technique switched" cannot be satisfied by
// an implementation that silently ran the same one twice.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechniqueCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetBinding.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include <memory>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kN = 32;

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    class FxLoadProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content-fx");

            // MEASURED: `Effect` is neither copyable nor default-constructible, so the reader is
            // registered for `std::shared_ptr<Effect>` and that is the type `Load` must be given.
            // `Load<Effect>` does not compile at all.
            Effect* fx = nullptr;
            std::shared_ptr<Effect> owned;
            std::string err;
            try
            {
                owned = content.Load<std::shared_ptr<Effect>>("P1Effect");
                fx = owned.get();
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("the compiled .fx loads as an Effect", fx != nullptr, err);
            if (fx == nullptr)
            {
                r.note("VERDICT", "compiled effects are unavailable; Tier E is deferred, Tier S is not");
                return r.finish("p1-fxload");
            }

            // --- techniques and parameters, by name --------------------------------------------------
            auto& techniques = fx->getTechniquesProperty();
            std::string names;
            for (int i = 0; i < techniques.getCountProperty(); ++i)
            {
                names += (i ? " " : "") + techniques[i].getNameProperty();
            }
            r.note("techniques", std::to_string(techniques.getCountProperty()) + ": " + names);
            auto& params = fx->getParametersProperty();
            std::string pnames;
            for (int i = 0; i < params.getCountProperty(); ++i)
            {
                pnames += (i ? " " : "") + params[i].getNameProperty();
            }
            r.note("parameters", std::to_string(params.getCountProperty()) + ": " + pnames);

            EffectTechnique* tint = techniques["Tint"];
            EffectTechnique* textured = techniques["Textured"];
            r.check("technique 'Tint' is selectable by name", tint != nullptr);
            r.check("technique 'Textured' is selectable by name", textured != nullptr);
            EffectParameter* wvp = params["WorldViewProj"];
            EffectParameter* tintColor = params["TintColor"];
            EffectParameter* baseTexture = params["BaseTexture"];
            r.check("parameter 'WorldViewProj' is present", wvp != nullptr);
            r.check("parameter 'TintColor' is present", tintColor != nullptr);
            r.check("parameter 'BaseTexture' is present", baseTexture != nullptr);
            if (tint == nullptr || textured == nullptr || wvp == nullptr || tintColor == nullptr)
            {
                return r.finish("p1-fxload");
            }

            // --- fixture ------------------------------------------------------------------------------
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
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(quad, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(idx, 6);

            // A uniform half-grey texture, so the 'Textured' technique's result is a KNOWN fraction of
            // 'Tint's -- the two techniques therefore differ numerically, not merely visually.
            std::vector<Color> grey(4,
                                    Color(static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(255)));
            Texture2D tex(gd, 2, 2);
            tex.SetData(grey.data(), 4);
            if (baseTexture != nullptr)
            {
                baseTexture->SetValue(&tex);
            }
            wvp->SetValue(Matrix::getIdentityProperty());

            RenderTarget2D rt(gd, kN, kN, false, SurfaceFormat::Color, DepthFormat::None);
            std::vector<Color> out(static_cast<size_t>(kN) * kN);
            auto render = [&](EffectTechnique* technique, const Vector4& colour)
            {
                fx->setCurrentTechniqueProperty(technique);
                tintColor->SetValue(colour);
                gd.SetRenderTarget(&rt);
                gd.Clear(Color::Black);
                gd.setBlendStateProperty(BlendState::Opaque);
                gd.setDepthStencilStateProperty(DepthStencilState::None);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
                gd.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
                gd.SetVertexBuffer(&vb);
                gd.setIndicesProperty(&ib);
                EffectPassCollection& passes = fx->getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p].Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(out.data(), static_cast<int>(out.size()));
                return out[static_cast<size_t>(kN / 2) * kN + kN / 2];
            };

            // --- the parameter must reach the shader ---------------------------------------------------
            const Color tintA = render(tint, Vector4(1.0f, 0.5f, 0.25f, 1.0f));
            char am[120];
            std::snprintf(am,
                          sizeof am,
                          "(%d,%d,%d) want (255,128,64)",
                          tintA.getRProperty(),
                          tintA.getGProperty(),
                          tintA.getBProperty());
            r.check("technique 'Tint' outputs exactly the TintColor parameter",
                    std::abs(tintA.getRProperty() - 255) <= 2 && std::abs(tintA.getGProperty() - 128) <= 2 &&
                        std::abs(tintA.getBProperty() - 64) <= 2,
                    am);

            const Color tintB = render(tint, Vector4(0.25f, 0.75f, 1.0f, 1.0f));
            char bm[120];
            std::snprintf(bm,
                          sizeof bm,
                          "(%d,%d,%d) want (64,191,255)",
                          tintB.getRProperty(),
                          tintB.getGProperty(),
                          tintB.getBProperty());
            r.check("changing the parameter changes the output",
                    std::abs(tintB.getRProperty() - 64) <= 2 && std::abs(tintB.getGProperty() - 191) <= 2 &&
                        std::abs(tintB.getBProperty() - 255) <= 2,
                    bm);

            // --- switching technique by name, per draw --------------------------------------------------
            const Color texA = render(textured, Vector4(1.0f, 0.5f, 0.25f, 1.0f));
            char tm[160];
            std::snprintf(tm,
                          sizeof tm,
                          "Tint (%d,%d,%d) vs Textured (%d,%d,%d); the 0.5 grey texture "
                          "must halve each channel",
                          tintA.getRProperty(),
                          tintA.getGProperty(),
                          tintA.getBProperty(),
                          texA.getRProperty(),
                          texA.getGProperty(),
                          texA.getBProperty());
            r.check("technique 'Textured' multiplies by the bound texture",
                    std::abs(texA.getRProperty() - 128) <= 3 && std::abs(texA.getGProperty() - 64) <= 3 &&
                        std::abs(texA.getBProperty() - 32) <= 3,
                    tm);
            r.check("the two techniques produce numerically different images",
                    std::abs(texA.getRProperty() - tintA.getRProperty()) > 32,
                    tm);

            // Back and forth in one frame, which is the SAMPLE-038 pattern the task names.
            const Color again = render(tint, Vector4(1.0f, 0.5f, 0.25f, 1.0f));
            r.check("switching back to the first technique restores its result",
                    again.getRProperty() == tintA.getRProperty() &&
                        again.getGProperty() == tintA.getGProperty() &&
                        again.getBProperty() == tintA.getBProperty(),
                    "(" + std::to_string(again.getRProperty()) + "," + std::to_string(again.getGProperty()) +
                        "," + std::to_string(again.getBProperty()) + ")");

            // --- HOUSE-00089: SpriteBatch with the compiled effect --------------------------------------
            {
                // SpriteBatch supplies its own transform, so the effect's WorldViewProj must be the
                // half-pixel-offset orthographic projection SpriteBatch would otherwise apply. XNA
                // games set it explicitly for exactly this reason.
                const Matrix spriteProjection = Matrix::CreateOrthographicOffCenter(
                    0.0f, static_cast<float>(kN), static_cast<float>(kN), 0.0f, 0.0f, 1.0f);
                wvp->SetValue(spriteProjection);
                fx->setCurrentTechniqueProperty(textured);
                tintColor->SetValue(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                SpriteBatch batch(gd);
                std::string sberr;
                bool ran = true;
                try
                {
                    gd.SetRenderTarget(&rt);
                    gd.Clear(Color::Black);
                    batch.Begin(SpriteSortMode::Immediate,
                                &BlendState::Opaque,
                                &SamplerState::PointClamp,
                                &DepthStencilState::None,
                                &RasterizerState::CullNone,
                                fx);
                    batch.Draw(tex, Rectangle(0, 0, kN, kN), Color::White);
                    batch.End();
                    gd.SetRenderTarget(nullptr);
                    rt.GetData(out.data(), static_cast<int>(out.size()));
                }
                catch (const std::exception& e)
                {
                    ran = false;
                    sberr = e.what();
                }
                r.check("SpriteBatch::Begin accepts the compiled effect", ran, sberr);
                if (ran)
                {
                    const Color c = out[static_cast<size_t>(kN / 2) * kN + kN / 2];
                    char sm[140];
                    std::snprintf(sm,
                                  sizeof sm,
                                  "(%d,%d,%d); the 0.5 grey sprite should arrive at ~128",
                                  c.getRProperty(),
                                  c.getGProperty(),
                                  c.getBProperty());
                    r.note("SpriteBatch + Effect", sm);
                    r.check("the sprite is drawn through the custom effect",
                            std::abs(c.getRProperty() - 128) <= 4,
                            sm);
                }
            }

            // --- HOUSE-00086 / BL-03, revisited now that compiled effects work --------------------------
            // The stock-effect measurement could only show that the renderer does not broadcast. A
            // technique declaring COLOR0 AND COLOR1 is the only way to ask whether attachment 1 is
            // reachable at all, and the two outputs carry deliberately different values so a copy of
            // attachment 0 is distinguishable from attachment 1's own output.
            {
                EffectTechnique* multi = techniques["MultiTarget"];
                r.check("the two-output technique compiled and is selectable", multi != nullptr);
                if (multi != nullptr)
                {
                    RenderTarget2D t0(gd,
                                      32,
                                      32,
                                      false,
                                      SurfaceFormat::Color,
                                      DepthFormat::None,
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
                    bindings.emplace_back(&t0);
                    bindings.emplace_back(&t1);
                    std::string mrtErr;
                    bool ran = true;
                    try
                    {
                        wvp->SetValue(Matrix::getIdentityProperty());
                        fx->setCurrentTechniqueProperty(multi);
                        tintColor->SetValue(Vector4(1.0f, 0.5f, 0.25f, 1.0f));
                        gd.SetRenderTargets(bindings);
                        gd.Clear(Color::Black);
                        gd.setBlendStateProperty(BlendState::Opaque);
                        gd.setDepthStencilStateProperty(DepthStencilState::None);
                        gd.setRasterizerStateProperty(RasterizerState::CullNone);
                        gd.SetVertexBuffer(&vb);
                        gd.setIndicesProperty(&ib);
                        EffectPassCollection& passes = fx->getCurrentTechniqueProperty()->getPassesProperty();
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p].Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                        }
                        gd.SetRenderTarget(nullptr);
                    }
                    catch (const std::exception& e)
                    {
                        ran = false;
                        mrtErr = e.what();
                    }
                    r.check("a two-output compiled effect draws to two bound targets", ran, mrtErr);
                    if (ran)
                    {
                        std::vector<Color> a(32 * 32), b(32 * 32);
                        t0.GetData(a.data(), static_cast<int>(a.size()));
                        t1.GetData(b.data(), static_cast<int>(b.size()));
                        const Color ca = a[16 * 32 + 16];
                        const Color cb = b[16 * 32 + 16];
                        char mm[240];
                        std::snprintf(mm,
                                      sizeof mm,
                                      "attachment 0 (%d,%d,%d) want (255,128,64); "
                                      "attachment 1 (%d,%d,%d) want (32,223,96)",
                                      ca.getRProperty(),
                                      ca.getGProperty(),
                                      ca.getBProperty(),
                                      cb.getRProperty(),
                                      cb.getGProperty(),
                                      cb.getBProperty());
                        r.note("MRT with a compiled effect", mm);
                        r.check("attachment 0 receives COLOR0", std::abs(ca.getRProperty() - 255) <= 3, mm);
                        const bool reached = std::abs(cb.getRProperty() - 32) <= 4 &&
                                             std::abs(cb.getGProperty() - 223) <= 4 &&
                                             std::abs(cb.getBProperty() - 96) <= 4;
                        const bool black =
                            cb.getRProperty() == 0 && cb.getGProperty() == 0 && cb.getBProperty() == 0;
                        r.note("BL-03 verdict",
                               reached ? "attachment 1 RECEIVES ITS OWN COLOR1 -- BL-03 does not hold "
                                         "for compiled effects"
                                       : (black ? "attachment 1 STAYS BLACK even for a compiled "
                                                  "two-output effect -- BL-03 confirmed in full"
                                                : "attachment 1 received something else -- see above"));
                        r.check("the MRT verdict is decisive", reached || black, mm);
                    }
                }
            }

            return r.finish("p1-fxload");
        }
    };

} // namespace

P1_MAIN(FxLoadProbe, "p1-fxload")
