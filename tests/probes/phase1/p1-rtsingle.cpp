// HOUSE-00083 -- settles `BL-09` / `Q-01`: can a `SurfaceFormat::Single` 2048x2048
// `RenderTarget2D` with `DepthFormat::Depth24` be created, rendered into, bound as an effect
// texture, and read back?
//
// Either answer is a result. If it works, Tier E's shadow map is a float depth buffer. If it
// fails, Tier E packs depth into RGBA8 and Tier S is unaffected either way. What must NOT happen is
// a verdict from reading a capability string, so every stage is executed and the exact failure --
// if any -- is captured rather than paraphrased.
//
// The stages are separated deliberately: creation can succeed where rendering fails, rendering can
// succeed where readback is unimplemented, and each of those leads somewhere different.
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

    constexpr int kShadow = 2048;

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    // Runs one stage, reporting either success or the exact exception text.
    template<typename Fn>
    bool Stage(p1::Report& r, const char* name, Fn&& fn, std::string& err)
    {
        try
        {
            fn();
            r.check(name, true);
            return true;
        }
        catch (const std::exception& e)
        {
            err = e.what();
            r.check(name, false, std::string("threw: ") + e.what());
            return false;
        }
        catch (...)
        {
            err = "<non-std exception>";
            r.check(name, false, err);
            return false;
        }
    }

    class RtSingleProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();
            std::string err;

            // --- stage 1: creation ------------------------------------------------------------------
            std::unique_ptr<RenderTarget2D> rt;
            const bool created = Stage(
                r,
                "create 2048x2048 SurfaceFormat::Single + Depth24",
                [&]
                {
                    rt = std::make_unique<RenderTarget2D>(gd,
                                                          kShadow,
                                                          kShadow,
                                                          false,
                                                          SurfaceFormat::Single,
                                                          DepthFormat::Depth24,
                                                          0,
                                                          RenderTargetUsage::PreserveContents);
                },
                err);
            if (created)
            {
                r.note("reported size",
                       std::to_string(rt->getWidthProperty()) + "x" +
                           std::to_string(rt->getHeightProperty()));
                r.check("the created target reports the format it was asked for",
                        rt->getFormatProperty() == SurfaceFormat::Single,
                        "format id " + std::to_string(static_cast<int>(rt->getFormatProperty())));
                r.check("the created target reports Depth24",
                        rt->getDepthStencilFormatProperty() == DepthFormat::Depth24,
                        "depth id " + std::to_string(static_cast<int>(rt->getDepthStencilFormatProperty())));
            }

            // Geometry: one tilted quad, so the values written vary across the target and a readback of
            // a constant would be visibly wrong.
            const VertexPT verts[4] = {
                {Vector3(-0.9f, -0.9f, 0.10f), Vector2(0.0f, 1.0f)},
                {Vector3(0.9f, -0.9f, 0.40f), Vector2(1.0f, 1.0f)},
                {Vector3(0.9f, 0.9f, 0.70f), Vector2(1.0f, 0.0f)},
                {Vector3(-0.9f, 0.9f, 0.30f), Vector2(0.0f, 0.0f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
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
            // 0.625 is exactly representable in binary, so a float round trip that changes it at all is
            // a real difference and not a rounding artefact.
            fx.setDiffuseColorProperty(Vector3(0.625f, 0.625f, 0.625f));

            // --- stage 2: render into it -------------------------------------------------------------
            bool rendered = false;
            if (created)
            {
                rendered = Stage(
                    r,
                    "bind it, clear it and draw depth-varying geometry into it",
                    [&]
                    {
                        gd.SetRenderTarget(rt.get());
                        gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                        gd.setBlendStateProperty(BlendState::Opaque);
                        gd.setDepthStencilStateProperty(DepthStencilState::Default);
                        gd.setRasterizerStateProperty(RasterizerState::CullNone);
                        gd.SetVertexBuffer(&vb);
                        gd.setIndicesProperty(&ib);
                        EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p]->Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                        }
                        gd.SetRenderTarget(nullptr);
                    },
                    err);
            }

            // --- stage 3: read it back as floats -----------------------------------------------------
            std::vector<float> texels;
            bool readBack = false;
            if (rendered)
            {
                readBack = Stage(
                    r,
                    "read it back with GetData(float*)",
                    [&]
                    {
                        texels.assign(static_cast<size_t>(kShadow) * kShadow, -1.0f);
                        rt->GetData(texels.data(), static_cast<int>(texels.size()));
                    },
                    err);
            }
            if (readBack)
            {
                int lit = 0, background = 0, other = 0;
                float minLit = 1e30f, maxLit = -1e30f;
                for (float v : texels)
                {
                    if (v > 0.6f && v < 0.65f)
                    {
                        ++lit;
                        if (v < minLit)
                        {
                            minLit = v;
                        }
                        if (v > maxLit)
                        {
                            maxLit = v;
                        }
                    }
                    else if (v == 0.0f)
                    {
                        ++background;
                    }
                    else
                    {
                        ++other;
                    }
                }
                char msg[220];
                std::snprintf(msg,
                              sizeof msg,
                              "%d texels in [0.6,0.65] (range %.6f..%.6f), %d at 0, %d other, of %zu",
                              lit,
                              static_cast<double>(minLit),
                              static_cast<double>(maxLit),
                              background,
                              other,
                              texels.size());
                r.note("readback histogram", msg);
                r.check("the drawn value survives the float round trip exactly",
                        lit > 0 && minLit == 0.625f && maxLit == 0.625f,
                        msg);
                r.check(
                    "the cleared area really is zero and covers the rest", background > 0 && other == 0, msg);
            }

            // --- stage 4: bind it as an effect texture ------------------------------------------------
            // A shadow map is only useful if it can be SAMPLED. The float target is bound as
            // BasicEffect's texture and drawn onto an ordinary Color target, which is read back: the
            // sampled value must arrive as 0.625 -> 159/255.
            if (readBack)
            {
                RenderTarget2D dest(gd, 32, 32, false, SurfaceFormat::Color, DepthFormat::None);
                bool sampled = Stage(
                    r,
                    "bind the Single target as an effect texture and sample it",
                    [&]
                    {
                        fx.setTextureEnabledProperty(true);
                        fx.setTextureProperty(rt.get());
                        fx.setDiffuseColorProperty(Vector3::One);
                        gd.SetRenderTarget(&dest);
                        gd.Clear(Color::Black);
                        gd.setDepthStencilStateProperty(DepthStencilState::None);
                        gd.setRasterizerStateProperty(RasterizerState::CullNone);
                        gd.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
                        gd.SetVertexBuffer(&vb);
                        gd.setIndicesProperty(&ib);
                        EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p]->Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                        }
                        gd.SetRenderTarget(nullptr);
                    },
                    err);
                if (sampled)
                {
                    std::vector<Color> out(32 * 32);
                    dest.GetData(out.data(), static_cast<int>(out.size()));
                    const int centre = out[16 * 32 + 16].getRProperty();
                    r.check("the sampled float value arrives in the shader (0.625 -> ~159)",
                            centre >= 157 && centre <= 161,
                            std::to_string(centre) + "/255; 0.625 * 255 = 159.4");
                }
            }

            // --- the fallback Tier E would need if any of the above had failed -------------------------
            // Measured unconditionally, so the RGBA8-packing path is known to work whichever way BL-09
            // went, and Tier E is never blocked on one format.
            {
                RenderTarget2D packed(gd, 256, 256, false, SurfaceFormat::Color, DepthFormat::Depth24);
                bool ok = Stage(
                    r,
                    "the RGBA8 fallback target is available regardless",
                    [&]
                    {
                        gd.SetRenderTarget(&packed);
                        gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                        gd.SetRenderTarget(nullptr);
                    },
                    err);
                (void)ok;
            }

            return r.finish("p1-rtsingle");
        }
    };

} // namespace

P1_MAIN(RtSingleProbe, "p1-rtsingle")
