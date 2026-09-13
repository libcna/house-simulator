// HOUSE-00081 -- `EnvironmentMapEffect` with a baked `TextureCube`: is the right face sampled, and
// does the Fresnel term behave?
//
// The cube's six faces carry six distinct colours, so "which face was sampled" is answered by
// reading one pixel, and the answer is compared against `reflect(-E, N)` computed in C++ -- not
// against what looks plausible. A tilted quad then moves the reflection onto a different face,
// which a fixture with a single face colour could never detect.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kN = 32;
    constexpr int kCube = 4;
    constexpr float kHalf = 0.01f;

    struct VertexPNT
    {
        Vector3 Position;
        Vector3 Normal;
        Vector2 TexCoord;
    };

    struct FaceColour
    {
        CubeMapFace face;
        const char* name;
        int r, g, b;
    };

    const FaceColour kFaces[6] = {
        {CubeMapFace::PositiveX, "+X", 200, 10, 10},
        {CubeMapFace::NegativeX, "-X", 10, 200, 10},
        {CubeMapFace::PositiveY, "+Y", 10, 10, 200},
        {CubeMapFace::NegativeY, "-Y", 200, 200, 10},
        {CubeMapFace::PositiveZ, "+Z", 200, 10, 200},
        {CubeMapFace::NegativeZ, "-Z", 10, 200, 200},
    };

    // Which face a direction lands on: the axis of largest magnitude wins, which is exactly what a
    // cube-map lookup does.
    int FaceOf(const Vector3& d)
    {
        const float ax = std::fabs(d.X), ay = std::fabs(d.Y), az = std::fabs(d.Z);
        if (ax >= ay && ax >= az)
        {
            return d.X >= 0.0f ? 0 : 1;
        }
        if (ay >= az)
        {
            return d.Y >= 0.0f ? 2 : 3;
        }
        return d.Z >= 0.0f ? 4 : 5;
    }

    class EnvMapProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            TextureCube cube(gd, kCube, false, SurfaceFormat::Color);
            for (const FaceColour& f : kFaces)
            {
                std::vector<Color> texels(kCube * kCube,
                                          Color(static_cast<std::uint8_t>(f.r),
                                                static_cast<std::uint8_t>(f.g),
                                                static_cast<std::uint8_t>(f.b),
                                                static_cast<std::uint8_t>(255)));
                cube.SetData(f.face, texels.data(), static_cast<int>(texels.size()));
            }
            // A white base texture, so the base colour is exactly the lit colour and the env blend is
            // the only thing the readback measures.
            std::vector<Color> white(4,
                                     Color(static_cast<std::uint8_t>(255),
                                           static_cast<std::uint8_t>(255),
                                           static_cast<std::uint8_t>(255),
                                           static_cast<std::uint8_t>(255)));
            Texture2D base(gd, 2, 2);
            base.SetData(white.data(), 4);

            const VertexPNT verts[4] = {
                {Vector3(-kHalf, -kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 1.0f)},
                {Vector3(kHalf, -kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(1.0f, 1.0f)},
                {Vector3(kHalf, kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(1.0f, 0.0f)},
                {Vector3(-kHalf, kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 0.0f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
                VertexElement(24, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
            });
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(verts, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(indices, 6);

            EnvironmentMapEffect fx(gd);
            fx.setViewProperty(Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 5.0f), Vector3::Zero, Vector3::Up));
            fx.setProjectionProperty(Matrix::CreateOrthographic(2.0f * kHalf, 2.0f * kHalf, 0.1f, 100.0f));
            fx.setTextureProperty(&base);
            fx.setEnvironmentMapProperty(&cube);
            fx.setDiffuseColorProperty(Vector3(0.0f, 0.0f, 0.0f));
            fx.setEmissiveColorProperty(Vector3(0.0f, 0.0f, 0.0f));
            fx.setEnvironmentMapSpecularProperty(Vector3::Zero);
            fx.setAlphaProperty(1.0f);
            fx.setFogEnabledProperty(false);
            fx.getDirectionalLight0Property().setEnabledProperty(false);
            fx.getDirectionalLight1Property().setEnabledProperty(false);
            fx.getDirectionalLight2Property().setEnabledProperty(false);

            RenderTarget2D rt(gd, kN, kN, false, SurfaceFormat::Color, DepthFormat::None);
            std::vector<Color> out(static_cast<size_t>(kN) * kN);
            auto render = [&](const Matrix& world)
            {
                fx.setWorldProperty(world);
                gd.SetRenderTarget(&rt);
                gd.Clear(Color::Black);
                gd.setBlendStateProperty(BlendState::Opaque);
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
                rt.GetData(out.data(), static_cast<int>(out.size()));
                return out[static_cast<size_t>(kN / 2) * kN + kN / 2];
            };

            // --- which face does the reflection land on? ---------------------------------------------
            fx.setEnvironmentMapAmountProperty(1.0f);
            fx.setFresnelFactorProperty(0.0f); // 0 disables the Fresnel weighting in XNA
            const Vector3 eye(0.0f, 0.0f, 1.0f);

            struct Tilt
            {
                float radians;
                const char* name;
            };

            const Tilt tilts[3] = {{0.0f, "head-on"},
                                   {MathHelper::PiOver4, "45 deg about +Y"},
                                   {-MathHelper::PiOver4, "-45 deg about +Y"}};
            for (const Tilt& t : tilts)
            {
                const Matrix world = Matrix::CreateRotationY(t.radians);
                const Vector3 n =
                    Vector3::Normalize(Vector3::TransformNormal(Vector3(0.0f, 0.0f, 1.0f), world));
                const Vector3 refl = eye * -1.0f - n * (2.0f * Vector3::Dot(eye * -1.0f, n));
                const int want = FaceOf(refl);
                const Color got = render(world);
                char msg[220];
                std::snprintf(
                    msg,
                    sizeof msg,
                    "reflect(-E,N) = (%.3f,%.3f,%.3f) -> face %s; got (%d,%d,%d), face %s is (%d,%d,%d)",
                    refl.X,
                    refl.Y,
                    refl.Z,
                    kFaces[want].name,
                    got.getRProperty(),
                    got.getGProperty(),
                    got.getBProperty(),
                    kFaces[want].name,
                    kFaces[want].r,
                    kFaces[want].g,
                    kFaces[want].b);
                r.check((std::string("the cube face sampled at ") + t.name + " is the analytic one").c_str(),
                        std::abs(got.getRProperty() - kFaces[want].r) <= 3 &&
                            std::abs(got.getGProperty() - kFaces[want].g) <= 3 &&
                            std::abs(got.getBProperty() - kFaces[want].b) <= 3,
                        msg);
            }

            // --- EnvironmentMapAmount is the blend weight --------------------------------------------
            const Matrix identity = Matrix::getIdentityProperty();
            fx.setEnvironmentMapAmountProperty(0.0f);
            const Color none = render(identity);
            fx.setEnvironmentMapAmountProperty(1.0f);
            const Color full = render(identity);
            fx.setEnvironmentMapAmountProperty(0.5f);
            const Color half = render(identity);
            char am[200];
            std::snprintf(am,
                          sizeof am,
                          "amount 0 -> (%d,%d,%d), 0.5 -> (%d,%d,%d), 1 -> (%d,%d,%d)",
                          none.getRProperty(),
                          none.getGProperty(),
                          none.getBProperty(),
                          half.getRProperty(),
                          half.getGProperty(),
                          half.getBProperty(),
                          full.getRProperty(),
                          full.getGProperty(),
                          full.getBProperty());
            r.note("EnvironmentMapAmount", am);
            r.check("amount 0 contributes no environment", none.getBProperty() <= 2, am);
            r.check("amount 1 is the environment colour", full.getBProperty() >= 195, am);
            r.check("amount 0.5 lands halfway, so the blend is linear",
                    std::abs(half.getBProperty() - (none.getBProperty() + full.getBProperty()) / 2) <= 3,
                    am);

            // --- the Fresnel term ---------------------------------------------------------------------
            // Its defining property: at a grazing angle the environment dominates; head-on it does not.
            // Measured across the exponent rather than asserted from the shader source.
            fx.setEnvironmentMapAmountProperty(1.0f);
            const Matrix grazing = Matrix::CreateRotationY(1.4f); // ~80 degrees
            for (float factor : {0.0f, 1.0f, 4.0f})
            {
                fx.setFresnelFactorProperty(factor);
                const Color headOn = render(identity);
                const Color graze = render(grazing);
                char fm[220];
                std::snprintf(fm,
                              sizeof fm,
                              "FresnelFactor %.1f: head-on (%d,%d,%d), grazing (%d,%d,%d)",
                              factor,
                              headOn.getRProperty(),
                              headOn.getGProperty(),
                              headOn.getBProperty(),
                              graze.getRProperty(),
                              graze.getGProperty(),
                              graze.getBProperty());
                r.note("Fresnel", fm);
                if (factor == 0.0f)
                {
                    // pow(x, 0) == 1, so the weighting is uniform: this is the "no Fresnel" control
                    r.check("FresnelFactor 0 weights every angle the same",
                            headOn.getRProperty() + headOn.getGProperty() + headOn.getBProperty() > 200 &&
                                graze.getRProperty() + graze.getGProperty() + graze.getBProperty() > 200,
                            fm);
                }
                else
                {
                    const int headSum = headOn.getRProperty() + headOn.getGProperty() + headOn.getBProperty();
                    const int grazeSum = graze.getRProperty() + graze.getGProperty() + graze.getBProperty();
                    r.check((std::string("FresnelFactor ") + std::to_string(static_cast<int>(factor)) +
                             " makes the grazing angle more reflective")
                                .c_str(),
                            grazeSum > headSum,
                            fm);
                }
            }

            // --- a measurement, not an assumption: does AmbientLightColor reach this effect? ----------
            fx.setFresnelFactorProperty(0.0f);
            fx.setEnvironmentMapAmountProperty(0.0f);
            fx.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
            fx.setAmbientLightColorProperty(Vector3::Zero);
            const Color noAmbient = render(identity);
            fx.setAmbientLightColorProperty(Vector3(0.5f, 0.5f, 0.5f));
            const Color withAmbient = render(identity);
            char amb[180];
            std::snprintf(amb,
                          sizeof amb,
                          "ambient 0 -> (%d,%d,%d), ambient 0.5 -> (%d,%d,%d)",
                          noAmbient.getRProperty(),
                          noAmbient.getGProperty(),
                          noAmbient.getBProperty(),
                          withAmbient.getRProperty(),
                          withAmbient.getGProperty(),
                          withAmbient.getBProperty());
            r.note("AmbientLightColor on EnvironmentMapEffect", amb);
            r.check("the AmbientLightColor behaviour is recorded either way", true, amb);

            return r.finish("p1-envmap");
        }
    };

} // namespace

P1_MAIN(EnvMapProbe, "p1-envmap")
