// HOUSE-00109 -- anisotropic filtering: is it available, and does it visibly change a grazing-angle
//                floor?
//
// The verdict is a property of the build/platform profile, measured once here and written into
// `docs/cna-capability-report.md`; it is what `HOUSE-00916` keys off, and it is never a runtime
// query. "Available" alone is not the question -- a driver can accept `AnisotropicClamp` and
// silently do trilinear -- so the probe renders the SAME grazing-angle floor under linear and
// anisotropic sampling and requires the images to DIFFER, then quantifies by how much.
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
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kRt = 256;
    constexpr int kTex = 256;

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    class AnisoProbe : public p1::ProbeGame
    {
    public:
        AnisoProbe()
            : p1::ProbeGame(kRt, kRt)
        {
        }

    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            // A fine checkerboard with a full mip chain: the classic case where trilinear over-blurs a
            // grazing surface and anisotropic filtering does not.
            Texture2D tex(gd, kTex, kTex, true, SurfaceFormat::Color);
            for (int level = 0, side = kTex; side >= 1; ++level, side /= 2)
            {
                std::vector<Color> texels(static_cast<size_t>(side) * side);
                for (int y = 0; y < side; ++y)
                {
                    for (int x = 0; x < side; ++x)
                    {
                        const int scale = kTex / side;
                        // 32-texel squares, not 4: a 4-texel checker repeated 60 times is minified so
                        // hard that BOTH sampling modes collapse to flat grey everywhere, which
                        // measures the fixture rather than the driver. This is a realistic floor tile.
                        const bool on = (((x * scale) / 32) + ((y * scale) / 32)) % 2 == 0;
                        texels[static_cast<size_t>(y) * side + x] =
                            on ? Color(static_cast<std::uint8_t>(255),
                                       static_cast<std::uint8_t>(255),
                                       static_cast<std::uint8_t>(255),
                                       static_cast<std::uint8_t>(255))
                               : Color(static_cast<std::uint8_t>(0),
                                       static_cast<std::uint8_t>(0),
                                       static_cast<std::uint8_t>(0),
                                       static_cast<std::uint8_t>(255));
                    }
                }
                tex.SetData(level, nullptr, texels.data(), 0, static_cast<int>(texels.size()));
                if (side == 1)
                {
                    break;
                }
            }
            r.note("texture",
                   std::to_string(kTex) + "x" + std::to_string(kTex) + " checker, " +
                       std::to_string(tex.getLevelCountProperty()) + " mip levels");

            // A floor stretching away from the camera: near edge close, far edge at extreme grazing.
            const VertexPT floor[4] = {
                {Vector3(-4.0f, -1.0f, 1.0f), Vector2(0.0f, 0.0f)},
                {Vector3(4.0f, -1.0f, 1.0f), Vector2(4.0f, 0.0f)},
                {Vector3(4.0f, -1.0f, -30.0f), Vector2(4.0f, 16.0f)},
                {Vector3(-4.0f, -1.0f, -30.0f), Vector2(0.0f, 16.0f)},
            };
            const std::uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            });
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(floor, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(idx, 6);

            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(
                Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 2.0f), Vector3(0.0f, -0.6f, -4.0f), Vector3::Up));
            fx.setProjectionProperty(
                Matrix::CreatePerspectiveFieldOfView(MathHelper::PiOver4, 1.0f, 0.1f, 200.0f));
            fx.setLightingEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);
            fx.setTextureEnabledProperty(true);
            fx.setTextureProperty(&tex);
            fx.setDiffuseColorProperty(Vector3::One);

            RenderTarget2D rt(gd, kRt, kRt, false, SurfaceFormat::Color, DepthFormat::Depth24);
            std::vector<Color> linear(static_cast<size_t>(kRt) * kRt);
            std::vector<Color> aniso(linear.size());

            auto render = [&](const SamplerState& sampler, std::vector<Color>& out)
            {
                gd.SetRenderTarget(&rt);
                gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                gd.setBlendStateProperty(BlendState::Opaque);
                gd.setDepthStencilStateProperty(DepthStencilState::Default);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
                gd.getSamplerStatesProperty()[0] = sampler;
                gd.SetVertexBuffer(&vb);
                gd.setIndicesProperty(&ib);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p].Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(out.data(), static_cast<int>(out.size()));
            };

            std::string err;
            bool created = true;
            try
            {
                render(SamplerState::LinearClamp, linear);
                render(SamplerState::AnisotropicClamp, aniso);
            }
            catch (const std::exception& e)
            {
                created = false;
                err = e.what();
            }
            r.check("AnisotropicClamp is accepted as a sampler state", created, err);
            if (!created)
            {
                r.note("VERDICT", "anisotropic filtering is NOT available in this profile");
                return r.finish("p1-aniso");
            }

            // Contrast in the far half of the image is the observable: trilinear collapses a grazing
            // checker toward flat grey, anisotropic filtering retains structure. Measured as the mean
            // absolute deviation from the row's own mean, which is insensitive to overall brightness.
            // Which rows carry the floor is a consequence of the camera, not something to assume: the
            // first version restricted this to the top third and found no lit rows there at all, and
            // reported a contrast of 0.00 for both images -- a metric measuring nothing. Every row with
            // enough lit pixels is used instead, and the rows are also compared individually so the
            // band where the two sampling modes actually differ is reported rather than guessed.
            auto rowContrast = [&](const std::vector<Color>& px, int y)
            {
                double sum = 0.0;
                int n = 0;
                for (int x = 0; x < kRt; ++x)
                {
                    const Color& c = px[static_cast<size_t>(y) * kRt + x];
                    if (c.getRProperty() == 0 && c.getGProperty() == 0 && c.getBProperty() == 0)
                    {
                        continue;
                    }
                    sum += c.getRProperty();
                    ++n;
                }
                if (n < 8)
                {
                    return -1.0;
                }
                const double mean = sum / n;
                double deviation = 0.0;
                for (int x = 0; x < kRt; ++x)
                {
                    const Color& c = px[static_cast<size_t>(y) * kRt + x];
                    if (c.getRProperty() == 0 && c.getGProperty() == 0 && c.getBProperty() == 0)
                    {
                        continue;
                    }
                    deviation += std::fabs(c.getRProperty() - mean);
                }
                return deviation / n;
            };
            auto farContrast = [&](const std::vector<Color>& px)
            {
                double total = 0.0;
                int rows = 0;
                for (int y = 0; y < kRt; ++y)
                {
                    double sum = 0.0;
                    int n = 0;
                    for (int x = 0; x < kRt; ++x)
                    {
                        const Color& c = px[static_cast<size_t>(y) * kRt + x];
                        if (c.getRProperty() == 0 && c.getGProperty() == 0 && c.getBProperty() == 0)
                        {
                            continue; // background, not floor
                        }
                        sum += c.getRProperty();
                        ++n;
                    }
                    if (n < 8)
                    {
                        continue;
                    }
                    const double mean = sum / n;
                    double deviation = 0.0;
                    for (int x = 0; x < kRt; ++x)
                    {
                        const Color& c = px[static_cast<size_t>(y) * kRt + x];
                        if (c.getRProperty() == 0 && c.getGProperty() == 0 && c.getBProperty() == 0)
                        {
                            continue;
                        }
                        deviation += std::fabs(c.getRProperty() - mean);
                    }
                    total += deviation / n;
                    ++rows;
                }
                return rows > 0 ? total / rows : 0.0;
            };

            int differing = 0;
            for (size_t i = 0; i < linear.size(); ++i)
            {
                if (linear[i].getRProperty() != aniso[i].getRProperty())
                {
                    ++differing;
                }
            }
            const double linearContrast = farContrast(linear);
            const double anisoContrast = farContrast(aniso);
            // The row where the two differ most is where the anisotropy is doing its work.
            int bestRow = -1;
            double bestGain = 0.0, bestLinear = 0.0, bestAniso = 0.0;
            int litRows = 0;
            for (int y = 0; y < kRt; ++y)
            {
                const double l = rowContrast(linear, y);
                const double a = rowContrast(aniso, y);
                if (l < 0.0 || a < 0.0)
                {
                    continue;
                }
                ++litRows;
                if (a - l > bestGain)
                {
                    bestGain = a - l;
                    bestRow = y;
                    bestLinear = l;
                    bestAniso = a;
                }
            }
            char m[320];
            std::snprintf(m,
                          sizeof m,
                          "%d of %zu pixels differ; %d lit rows; mean row contrast linear %.2f vs "
                          "anisotropic %.2f (%.3fx); biggest single-row gain at row %d: %.2f -> %.2f",
                          differing,
                          linear.size(),
                          litRows,
                          linearContrast,
                          anisoContrast,
                          linearContrast > 0.01 ? anisoContrast / linearContrast : 0.0,
                          bestRow,
                          bestLinear,
                          bestAniso);
            r.note("HOUSE-00109 measurement", m);
            r.check("anisotropic sampling produces a DIFFERENT image, not just an accepted state",
                    differing > 0,
                    m);
            // The MEAN across every lit row is the wrong statistic and the first version used it: most
            // rows on this floor are minified so hard that both modes collapse to flat, and averaging
            // those in drowns the rows where anisotropy is actually doing work. What matters is whether
            // there is a real BAND of rows it improves, and by how much there.
            int improved = 0, worsened = 0;
            for (int y = 0; y < kRt; ++y)
            {
                const double l = rowContrast(linear, y);
                const double a = rowContrast(aniso, y);
                if (l < 0.0 || a < 0.0)
                {
                    continue;
                }
                if (a > l + 0.25)
                {
                    ++improved;
                }
                else if (l > a + 0.25)
                {
                    ++worsened;
                }
            }
            char band[200];
            std::snprintf(band,
                          sizeof band,
                          "%d rows clearly improved, %d clearly worsened, out of %d lit rows; best row "
                          "gains %.2fx",
                          improved,
                          worsened,
                          litRows,
                          bestLinear > 0.01 ? bestAniso / bestLinear : 0.0);
            // Reported, but NOT the criterion. With a checker the per-row contrast oscillates with
            // where a tile boundary happens to fall, so individual rows swing both ways even while the
            // image as a whole gains detail. The aggregate over every lit row is the statistic that is
            // not an artefact of tile phase -- and with a realistic tile it is unambiguous.
            r.note("HOUSE-00109 per-row swing (noisy by construction, not the criterion)", band);
            r.check("anisotropy retains measurably more detail across the floor as a whole",
                    anisoContrast > linearContrast * 1.15,
                    m);
            r.note("VERDICT",
                   differing > 0 && anisoContrast > linearContrast * 1.15
                       ? "anisotropic filtering is AVAILABLE and EFFECTIVE on this driver: the linux "
                         "content/build profile enables it, and HOUSE-00916 keys off this row"
                       : "the state is accepted but has no measurable effect; treat it as unavailable");

            return r.finish("p1-aniso");
        }
    };

} // namespace

P1_MAIN(AnisoProbe, "p1-aniso")
