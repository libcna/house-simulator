// HOUSE-00078 -- `DualTextureEffect`: does the SECOND UV channel reach the effect, and is the
// product the analytic one?
//
// Both halves matter and they are separable, so the fixture separates them: the two channels carry
// DIFFERENT coordinates (channel 1 is mirrored in X), so an implementation that quietly fed
// TEXCOORD0 to both samplers would produce a symmetric result and fail on every asymmetric texel.
//
// The comparison is against the formula FNA's `DualTextureEffect.fx` defines and CNA implements --
// `color = tex0; color.rgb *= 2; color *= tex1 * diffuse` -- not against a screenshot. The textures
// are built with `SetData` rather than through the content pipeline so no premultiplication policy
// enters the arithmetic (HOUSE-00065), and every texel is opaque, where premultiplication is the
// identity anyway.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kN = 8; // render target and both textures are kN x kN, one texel per pixel

    // XNA 4.0 has no built-in two-channel vertex type; a game declares its own, which is exactly what
    // `cna-house` will do for lightmapped geometry.
    struct VertexDualTexture
    {
        Vector3 Position;
        Vector2 TexCoord0;
        Vector2 TexCoord1;
    };

    int Expected(int albedo, int lightmap)
    {
        // out = clamp(2 * a/255 * l/255) in float space, then back to a byte.
        const float v =
            2.0f * (static_cast<float>(albedo) / 255.0f) * (static_cast<float>(lightmap) / 255.0f);
        const float c = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return static_cast<int>(c * 255.0f + 0.5f);
    }

    class DualTexProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            // --- the two textures, both opaque so premultiplication is the identity -----------------
            // Kept below the doubling ceiling: max product is 2*240*126/255 = 237, so nothing clamps
            // and a clamp would therefore be a real failure rather than an artefact of the fixture.
            std::vector<Color> albedoTexels(kN * kN), lightTexels(kN * kN);
            auto albedoAt = [](int x, int y) { return ((x + y) % 2) ? 240 : 80; };
            auto lightAt = [](int x, int y) { return 16 * x + 2 * y; };
            for (int y = 0; y < kN; ++y)
            {
                for (int x = 0; x < kN; ++x)
                {
                    const int a = albedoAt(x, y);
                    const int l = lightAt(x, y);
                    albedoTexels[static_cast<size_t>(y) * kN + x] = Color(static_cast<std::uint8_t>(a),
                                                                          static_cast<std::uint8_t>(a),
                                                                          static_cast<std::uint8_t>(a),
                                                                          static_cast<std::uint8_t>(255));
                    lightTexels[static_cast<size_t>(y) * kN + x] = Color(static_cast<std::uint8_t>(l),
                                                                         static_cast<std::uint8_t>(l),
                                                                         static_cast<std::uint8_t>(l),
                                                                         static_cast<std::uint8_t>(255));
                }
            }
            Texture2D albedo(gd, kN, kN);
            Texture2D light(gd, kN, kN);
            albedo.SetData(albedoTexels.data(), static_cast<int>(albedoTexels.size()));
            light.SetData(lightTexels.data(), static_cast<int>(lightTexels.size()));

            // --- a quad in clip space, one texel per pixel --------------------------------------------
            // World/View/Projection stay identity, so positions ARE normalised device coordinates and
            // the mapping from render-target pixel to source texel is exact rather than approximate.
            // Channel 1 is mirrored in X: if the effect fed TEXCOORD0 to both samplers, every texel
            // where the two disagree would come out wrong, and the checker guarantees most do.
            const VertexDualTexture verts[4] = {
                {Vector3(-1.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f)},
                {Vector3(1.0f, 1.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(0.0f, 0.0f)},
                {Vector3(1.0f, -1.0f, 0.0f), Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f)},
                {Vector3(-1.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f), Vector2(1.0f, 1.0f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
                VertexElement(20, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 1),
            });
            r.check("a two-channel VertexDeclaration reports the expected stride",
                    decl.getVertexStrideProperty() == static_cast<int>(sizeof(VertexDualTexture)),
                    std::to_string(decl.getVertexStrideProperty()) + " vs " +
                        std::to_string(sizeof(VertexDualTexture)));

            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(verts, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(indices, 6);

            DualTextureEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setTextureProperty(&albedo);
            fx.setTexture2Property(&light);
            fx.setVertexColorEnabledProperty(false);

            RenderTarget2D rt(gd, kN, kN, false, SurfaceFormat::Color, DepthFormat::None);
            gd.SetRenderTarget(&rt);
            gd.Clear(Color::Black);
            gd.setBlendStateProperty(BlendState::Opaque);
            gd.setDepthStencilStateProperty(DepthStencilState::None);
            gd.setRasterizerStateProperty(RasterizerState::CullNone);
            // Point sampling is what makes pixel (i,j) read texel (i,j) and nothing else.
            gd.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
            gd.getSamplerStatesProperty()[1] = SamplerState::PointClamp;
            gd.SetVertexBuffer(&vb);
            gd.setIndicesProperty(&ib);
            EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
            for (int p = 0; p < passes.getCountProperty(); ++p)
            {
                passes[p].Apply();
                gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
            }
            gd.SetRenderTarget(nullptr);

            std::vector<Color> out(kN * kN);
            rt.GetData(out.data(), static_cast<int>(out.size()));

            // --- compare against the analytic product ------------------------------------------------
            int worst = 0, offBy = 0, wrongChannel = 0;
            std::string firstBad;
            for (int y = 0; y < kN; ++y)
            {
                for (int x = 0; x < kN; ++x)
                {
                    const int got = out[static_cast<size_t>(y) * kN + x].getRProperty();
                    const int want = Expected(albedoAt(x, y), lightAt(kN - 1 - x, y));
                    // what it would be if the effect wrongly used channel 0 for both samplers
                    const int wantIfSameChannel = Expected(albedoAt(x, y), lightAt(x, y));
                    const int d = got > want ? got - want : want - got;
                    if (d > worst)
                    {
                        worst = d;
                    }
                    if (d > 2)
                    {
                        ++offBy;
                        if (got == wantIfSameChannel)
                        {
                            ++wrongChannel;
                        }
                        if (firstBad.empty())
                        {
                            char b[180];
                            std::snprintf(b,
                                          sizeof b,
                                          "(%d,%d) got %d want %d (same-channel would be %d)",
                                          x,
                                          y,
                                          got,
                                          want,
                                          wantIfSameChannel);
                            firstBad = b;
                        }
                    }
                }
            }
            r.check("the product matches the analytic value within 2/255",
                    offBy == 0,
                    std::to_string(offBy) + " of " + std::to_string(kN * kN) +
                        " texels off by more than 2; worst delta " + std::to_string(worst) + "; " + firstBad);
            r.check("the second UV channel really is the second channel",
                    wrongChannel == 0,
                    wrongChannel == 0 ? "no texel matches the same-channel prediction where it should not"
                                      : std::to_string(wrongChannel) + " texels match TEXCOORD0-for-both");
            r.note("worst absolute delta", std::to_string(worst) + "/255");

            // A grey/grey control that would be invisible without the doubling factor: 128 x 128 must
            // come out near 128, not near 64. This is the exact bug CNA's own Task 383 fixed, so the
            // probe keeps a permanent witness for it.
            std::vector<Color> grey(kN * kN,
                                    Color(static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(128),
                                          static_cast<std::uint8_t>(255)));
            Texture2D g1(gd, kN, kN), g2(gd, kN, kN);
            g1.SetData(grey.data(), static_cast<int>(grey.size()));
            g2.SetData(grey.data(), static_cast<int>(grey.size()));
            fx.setTextureProperty(&g1);
            fx.setTexture2Property(&g2);
            gd.SetRenderTarget(&rt);
            gd.Clear(Color::Black);
            gd.SetVertexBuffer(&vb);
            gd.setIndicesProperty(&ib);
            for (int p = 0; p < passes.getCountProperty(); ++p)
            {
                passes[p].Apply();
                gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
            }
            gd.SetRenderTarget(nullptr);
            rt.GetData(out.data(), static_cast<int>(out.size()));
            const int mid = out[static_cast<size_t>(kN / 2) * kN + kN / 2].getRProperty();
            r.check("the *2 doubling factor is present (128 x 128 -> ~128, not ~64)",
                    mid >= 126 && mid <= 130,
                    std::to_string(mid) + "; without the factor it would be ~64");

            return r.finish("p1-dualtex");
        }
    };

} // namespace

P1_MAIN(DualTexProbe, "p1-dualtex")
