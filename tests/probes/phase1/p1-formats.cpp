// HOUSE-00110 -- which `SurfaceFormat`s can be created as a TEXTURE and which as a RENDER TARGET
// on this driver.
// HOUSE-00111 -- does a DXT texture built by the pipeline stay compressed at runtime?
//
// The result is a property of the build/platform profile, measured once during qualification and
// then written into `cna-house.md` §27.2 -- it is never a runtime query, per ADR-0001 and the
// effective-feature-set decision. So this probe's whole output is a table for a document.
//
// "Creatable" is not enough on its own: a driver can accept a format and then fail on first use, so
// each render-target format is also cleared and drawn into before it is called supported.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/GameServiceContainer.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
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

    struct FormatRow
    {
        SurfaceFormat format;
        const char* name;
        bool blockCompressed;
    };

    const FormatRow kFormats[] = {
        {SurfaceFormat::Color, "Color", false},
        {SurfaceFormat::Bgr565, "Bgr565", false},
        {SurfaceFormat::Bgra5551, "Bgra5551", false},
        {SurfaceFormat::Bgra4444, "Bgra4444", false},
        {SurfaceFormat::Dxt1, "Dxt1", true},
        {SurfaceFormat::Dxt3, "Dxt3", true},
        {SurfaceFormat::Dxt5, "Dxt5", true},
        {SurfaceFormat::NormalizedByte2, "NormalizedByte2", false},
        {SurfaceFormat::NormalizedByte4, "NormalizedByte4", false},
        {SurfaceFormat::Rgba1010102, "Rgba1010102", false},
        {SurfaceFormat::Rg32, "Rg32", false},
        {SurfaceFormat::Rgba64, "Rgba64", false},
        {SurfaceFormat::Alpha8, "Alpha8", false},
        {SurfaceFormat::Single, "Single", false},
        {SurfaceFormat::Vector2, "Vector2", false},
        {SurfaceFormat::Vector4, "Vector4", false},
        {SurfaceFormat::HalfSingle, "HalfSingle", false},
        {SurfaceFormat::HalfVector2, "HalfVector2", false},
        {SurfaceFormat::HalfVector4, "HalfVector4", false},
    };

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    class FormatsProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

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
            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setLightingEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);

            // ================= HOUSE-00110: the survey =============================================
            // Surveyed at BOTH graphics profiles. `GraphicsDeviceManager` defaults to
            // `GraphicsProfile::Reach`, and XNA 4.0's Reach profile forbids the float and 16-bit-per-
            // channel texture formats outright -- so a survey run only at the default would report a
            // driver limitation where the real cause is an XNA profile rule the project chooses.
            for (int pass = 0; pass < 2; ++pass)
            {
                const bool hiDef = pass == 1;
                if (hiDef)
                {
                    gdm_.setGraphicsProfileProperty(GraphicsProfile::HiDef);
                    gdm_.ApplyChanges();
                }
                const std::string profile = hiDef ? "HiDef" : "Reach";
                std::string asTexture, asTarget, textureFails, targetFails;
                for (const FormatRow& f : kFormats)
                {
                    // as a texture
                    bool tex = true;
                    std::string terr;
                    try
                    {
                        Texture2D t(gd, 8, 8, false, f.format);
                        (void)t.getWidthProperty();
                    }
                    catch (const std::exception& e)
                    {
                        tex = false;
                        terr = e.what();
                    }
                    if (tex)
                    {
                        asTexture += std::string(asTexture.empty() ? "" : " ") + f.name;
                    }
                    else
                    {
                        textureFails += std::string(f.name) + "(" + terr.substr(0, 60) + ") ";
                    }

                    // as a render target: created AND actually used, because acceptance at creation is not
                    // the same as working
                    bool tgt = true;
                    std::string rerr;
                    try
                    {
                        RenderTarget2D rt(gd,
                                          8,
                                          8,
                                          false,
                                          f.format,
                                          DepthFormat::None,
                                          0,
                                          RenderTargetUsage::PreserveContents);
                        gd.SetRenderTarget(&rt);
                        gd.Clear(Color::Black);
                        gd.setBlendStateProperty(BlendState::Opaque);
                        gd.setDepthStencilStateProperty(DepthStencilState::None);
                        gd.setRasterizerStateProperty(RasterizerState::CullNone);
                        fx.setTextureEnabledProperty(false);
                        fx.setDiffuseColorProperty(Vector3(0.5f, 0.5f, 0.5f));
                        gd.SetVertexBuffer(&vb);
                        gd.setIndicesProperty(&ib);
                        EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p].Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                        }
                        gd.SetRenderTarget(nullptr);
                    }
                    catch (const std::exception& e)
                    {
                        tgt = false;
                        rerr = e.what();
                        gd.SetRenderTarget(nullptr);
                    }
                    if (tgt)
                    {
                        asTarget += std::string(asTarget.empty() ? "" : " ") + f.name;
                    }
                    else
                    {
                        targetFails += std::string(f.name) + "(" + rerr.substr(0, 60) + ") ";
                    }
                }
                r.note((profile + ": creatable as Texture2D").c_str(), asTexture);
                r.note((profile + ": texture creation refused").c_str(),
                       textureFails.empty() ? "<none>" : textureFails);
                r.note((profile + ": usable as RenderTarget2D").c_str(), asTarget);
                r.note((profile + ": render-target use refused").c_str(),
                       targetFails.empty() ? "<none>" : targetFails);
                r.check((profile + ": Color works both as a texture and as a render target").c_str(),
                        asTexture.find("Color") != std::string::npos &&
                            asTarget.find("Color") != std::string::npos);
                r.check((profile + ": SurfaceFormat::Single is usable as a render target").c_str(),
                        asTarget.find("Single") != std::string::npos,
                        asTarget);
                r.check((profile + ": the DXT formats are creatable as textures").c_str(),
                        asTexture.find("Dxt1") != std::string::npos &&
                            asTexture.find("Dxt5") != std::string::npos,
                        asTexture);
            }

            // ================= HOUSE-00111: DXT ====================================================
            // The pipeline-built DXT asset is loaded and its runtime format read. If it comes back as
            // a Dxt* format, the block data reached the GPU compressed; if it comes back as `Color`,
            // something decompressed it on the way and the memory saving is not real.
            // Two content trees, because the answer differs between the containers and that difference
            // IS the finding: `.cnb` texture schema 1 is frozen to Rgba8, so a `textureFormat` asking
            // for DXT is warned about and silently kept uncompressed; `.xnb` carries the blocks.
            Content::ContentManager& content = getContentProperty();
            Content::ContentManager xnb(&getServicesProperty());
            xnb.setRootDirectoryProperty("build-probe/p1-content-xnb");
            content.setRootDirectoryProperty("build-probe/p1-content");
            for (const char* name : {"P1Dxt1", "P1Dxt5", "P1Uncompressed"})
            {
                std::string err;
                try
                {
                    Texture2D t = content.Load<Texture2D>(name);
                    const int fmt = static_cast<int>(t.getFormatProperty());
                    const char* fname = "?";
                    for (const FormatRow& f : kFormats)
                    {
                        if (static_cast<int>(f.format) == fmt)
                        {
                            fname = f.name;
                        }
                    }
                    r.note(name,
                           std::string("format ") + fname + " (" + std::to_string(fmt) + "), " +
                               std::to_string(t.getWidthProperty()) + "x" +
                               std::to_string(t.getHeightProperty()));
                    const bool isCompressed = fmt == static_cast<int>(SurfaceFormat::Dxt1) ||
                                              fmt == static_cast<int>(SurfaceFormat::Dxt3) ||
                                              fmt == static_cast<int>(SurfaceFormat::Dxt5);
                    // The measured truth: a `.cnb` NEVER carries block data, whatever was asked for.
                    r.check((std::string(name) + ".cnb arrives uncompressed, whatever was asked").c_str(),
                            !isCompressed,
                            isCompressed ? "block-compressed" : "Rgba8, as CNB schema 1 requires");
                }
                catch (const std::exception& e)
                {
                    err = e.what();
                    r.check((std::string(name) + ": loads from .cnb").c_str(), false, err);
                }
            }
            for (const char* name : {"P1Dxt1", "P1Dxt5", "P1Uncompressed"})
            {
                try
                {
                    Texture2D t = xnb.Load<Texture2D>(name);
                    const int fmt = static_cast<int>(t.getFormatProperty());
                    const char* fname = "?";
                    for (const FormatRow& f : kFormats)
                    {
                        if (static_cast<int>(f.format) == fmt)
                        {
                            fname = f.name;
                        }
                    }
                    r.note((std::string(name) + ".xnb").c_str(),
                           std::string("format ") + fname + " (" + std::to_string(fmt) + "), " +
                               std::to_string(t.getWidthProperty()) + "x" +
                               std::to_string(t.getHeightProperty()));
                    const bool wantCompressed = std::string(name) != "P1Uncompressed";
                    const bool isCompressed = fmt == static_cast<int>(SurfaceFormat::Dxt1) ||
                                              fmt == static_cast<int>(SurfaceFormat::Dxt3) ||
                                              fmt == static_cast<int>(SurfaceFormat::Dxt5);
                    r.check((std::string(name) + ".xnb keeps the format the pipeline chose").c_str(),
                            isCompressed == wantCompressed,
                            isCompressed ? "still block-compressed at runtime" : "Rgba8");
                }
                catch (const std::exception& e)
                {
                    r.check((std::string(name) + ": loads from .xnb").c_str(), false, e.what());
                }
            }

            return r.finish("p1-formats");
        }
    };

} // namespace

P1_MAIN(FormatsProbe, "p1-formats")
