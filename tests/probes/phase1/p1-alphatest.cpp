// HOUSE-00080 -- `AlphaTestEffect`: where exactly is the cutoff, and does two-sided rendering work?
//
// Foliage is the reason both questions matter. A leaf card is a single quad, drawn from both sides,
// whose silhouette IS the alpha cutoff. So the probe measures the boundary texel by texel with an
// alpha ramp rather than eyeballing a leaf, and it checks the back face by rotating the quad 180
// degrees rather than by trusting `CullNone`.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
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

    constexpr int kN = 256; // 256 columns -> one column per alpha value, so the cutoff is exact

    struct VertexPT
    {
        Vector3 Position;
        Vector2 TexCoord;
    };

    class AlphaTestProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            // One texel per alpha value 0..255, all with the same opaque-white RGB, so the ONLY thing
            // that varies along the row is alpha and the cutoff column is read straight off.
            std::vector<Color> ramp(kN);
            for (int x = 0; x < kN; ++x)
            {
                ramp[static_cast<size_t>(x)] = Color(static_cast<std::uint8_t>(255),
                                                     static_cast<std::uint8_t>(255),
                                                     static_cast<std::uint8_t>(255),
                                                     static_cast<std::uint8_t>(x));
            }
            Texture2D tex(gd, kN, 1);
            tex.SetData(ramp.data(), static_cast<int>(ramp.size()));

            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            });
            // Counter-clockwise seen from +Z -- the glTF convention HOUSE-00071 settled, so
            // `CullClockwise` shows the front and `CullCounterClockwise` shows only the back.
            // Wound counter-clockwise AS SEEN ON SCREEN, i.e. bottom-left -> bottom-right -> top-right.
            // Getting this backwards is easy and was the first version's bug: in normalised device
            // coordinates +Y is UP, so top-left -> top-right -> bottom-right is CLOCKWISE and the whole
            // quad vanishes under `CullClockwise`. Procedurally authored geometry has to be wound to
            // match the glTF convention HOUSE-00071 settled; it does not inherit it.
            const VertexPT front[4] = {
                {Vector3(-1.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f)},
                {Vector3(1.0f, -1.0f, 0.0f), Vector2(1.0f, 1.0f)},
                {Vector3(1.0f, 1.0f, 0.0f), Vector2(1.0f, 0.0f)},
                {Vector3(-1.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(front, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(indices, 6);

            AlphaTestEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setTextureProperty(&tex);
            fx.setVertexColorEnabledProperty(false);
            fx.setDiffuseColorProperty(Vector3::One);

            RenderTarget2D rt(gd, kN, 4, false, SurfaceFormat::Color, DepthFormat::None);
            std::vector<Color> out(static_cast<size_t>(kN) * 4);

            auto render = [&](CullMode cull, const Matrix& world)
            {
                RasterizerState rs;
                rs.setCullModeProperty(cull);
                fx.setWorldProperty(world);
                gd.SetRenderTarget(&rt);
                gd.Clear(Color::Black);
                gd.setBlendStateProperty(BlendState::Opaque);
                gd.setDepthStencilStateProperty(DepthStencilState::None);
                gd.setRasterizerStateProperty(rs);
                gd.getSamplerStatesProperty()[0] = SamplerState::PointClamp;
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

            // Finds the lowest column that survived the test, reading the middle row.
            auto firstLitColumn = [&]
            {
                for (int x = 0; x < kN; ++x)
                {
                    if (out[static_cast<size_t>(1) * kN + x].getRProperty() > 0)
                    {
                        return x;
                    }
                }
                return -1;
            };
            auto litCount = [&]
            {
                int n = 0;
                for (int x = 0; x < kN; ++x)
                {
                    if (out[static_cast<size_t>(1) * kN + x].getRProperty() > 0)
                    {
                        ++n;
                    }
                }
                return n;
            };

            // --- the cutoff, for each comparison function ---------------------------------------------
            struct Case
            {
                CompareFunction fn;
                const char* name;
                int reference;
                int wantFirst;
                int wantCount;
            };

            const Case cases[] = {
                // Greater/128 keeps alpha 129..255 -> 127 columns starting at 129
                {CompareFunction::Greater, "Greater", 128, 129, 127},
                {CompareFunction::GreaterEqual, "GreaterEqual", 128, 128, 128},
                {CompareFunction::Less, "Less", 128, 0, 128},
                {CompareFunction::Equal, "Equal", 128, 128, 1},
                {CompareFunction::Always, "Always", 128, 0, 256},
                {CompareFunction::Never, "Never", 128, -1, 0},
            };
            for (const Case& c : cases)
            {
                fx.setAlphaFunctionProperty(c.fn);
                fx.setReferenceAlphaProperty(c.reference);
                render(CullMode::CullClockwiseFace, Matrix::getIdentityProperty());
                const int first = firstLitColumn();
                const int n = litCount();
                char msg[160];
                std::snprintf(msg,
                              sizeof msg,
                              "first lit column %d (want %d), %d lit (want %d)",
                              first,
                              c.wantFirst,
                              n,
                              c.wantCount);
                r.check((std::string("AlphaFunction::") + c.name + " cuts where XNA says").c_str(),
                        first == c.wantFirst && n == c.wantCount,
                        msg);
            }

            // --- two-sided rendering ------------------------------------------------------------------
            fx.setAlphaFunctionProperty(CompareFunction::Greater);
            fx.setReferenceAlphaProperty(128);
            render(CullMode::CullClockwiseFace, Matrix::getIdentityProperty());
            const int frontLit = litCount();
            render(CullMode::CullCounterClockwiseFace, Matrix::getIdentityProperty());
            const int frontCulled = litCount();
            r.check("the front face is culled by the wrong cull mode",
                    frontCulled == 0,
                    std::to_string(frontCulled) + " px");
            // rotate 180 degrees about Y: the quad now presents its back face to the camera
            const Matrix flip = Matrix::CreateRotationY(MathHelper::Pi);
            render(CullMode::CullClockwiseFace, flip);
            const int backUnderFrontMode = litCount();
            render(CullMode::CullCounterClockwiseFace, flip);
            const int backUnderBackMode = litCount();
            r.check("a back-facing leaf card is invisible under the single-sided state",
                    backUnderFrontMode == 0,
                    std::to_string(backUnderFrontMode) + " px");
            r.check("it becomes visible under the reversed cull state",
                    backUnderBackMode == frontLit && frontLit > 0,
                    std::to_string(backUnderBackMode) + " vs " + std::to_string(frontLit));
            render(CullMode::None, flip);
            const int backUnderNone = litCount();
            render(CullMode::None, Matrix::getIdentityProperty());
            const int frontUnderNone = litCount();
            r.check("CullNone draws the card from BOTH sides -- the foliage state",
                    backUnderNone == frontLit && frontUnderNone == frontLit && frontLit > 0,
                    std::to_string(frontUnderNone) + " front / " + std::to_string(backUnderNone) +
                        " back, against " + std::to_string(frontLit));

            return r.finish("p1-alphatest");
        }
    };

} // namespace

P1_MAIN(AlphaTestProbe, "p1-alphatest")
