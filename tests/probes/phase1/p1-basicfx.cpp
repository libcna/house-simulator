// HOUSE-00082 -- `BasicEffect` with three directional lights, ambient, emissive, specular and fog
// ALL AT ONCE, compared against the analytic value of the lighting model rather than against a
// screenshot.
//
// Tier S is BasicEffect. If the model differs from what the design assumes -- one missing term, a
// specular applied before rather than after fog, a doubled ambient -- every lighting decision in
// phases 12 and 16 inherits the error. So the probe implements the model in C++ and asserts the
// pixel.
//
// Two fixture details make the comparison exact rather than approximate:
//   * the quad is 0.02 units across under an orthographic frame, so the per-pixel eye vector
//     deviates from (0,0,1) by at most 0.002 and the analytic model needs no per-pixel integration;
//   * every light direction is a unit vector with exactly representable components.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kN = 32;
    constexpr float kHalf = 0.01f; // the quad is 0.02 world units across

    struct VertexPN
    {
        Vector3 Position;
        Vector3 Normal;
    };

    struct Light
    {
        Vector3 direction;
        Vector3 diffuse;
        Vector3 specular;
    };

    // The model, exactly as EasyGL's lit fragment shader computes it. Written out rather than
    // abbreviated, so a disagreement points at one term.
    Vector3 Analytic(const Vector3& normal,
                     const Vector3& eye,
                     const Light lights[3],
                     const Vector3& ambient,
                     const Vector3& diffuseColor,
                     const Vector3& emissive,
                     const Vector3& specularColor,
                     float specularPower,
                     float alpha,
                     bool fogEnabled,
                     const Vector3& fogColor,
                     float fogFactor)
    {
        Vector3 lightSum = ambient;
        Vector3 specularSum = Vector3::Zero;
        for (int i = 0; i < 3; ++i)
        {
            const Vector3 L = -lights[i].direction;
            const float dotL = Vector3::Dot(normal, L);
            const float zeroL = dotL >= 0.0f ? 1.0f : 0.0f;
            const float nDotL = dotL > 0.0f ? dotL : 0.0f;
            lightSum = lightSum + lights[i].diffuse * nDotL;
            const Vector3 h = Vector3::Normalize(eye - lights[i].direction);
            const float dotH = Vector3::Dot(h, normal);
            const float base = (dotH > 0.0f ? dotH : 0.0f) * zeroL;
            const float spec = std::pow(base, specularPower);
            specularSum = specularSum + lights[i].specular * spec;
        }
        Vector3 lit = lightSum * diffuseColor + emissive;
        // FragColor.rgb += specularRGB * FragColor.a -- the highlight is scaled by the final alpha
        lit = lit + specularSum * specularColor * alpha;
        if (fogEnabled)
        {
            lit = fogColor + (lit - fogColor) * fogFactor; // mix(fogColor, lit, fogFactor)
        }
        return lit;
    }

    int Byte(float v)
    {
        const float c = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return static_cast<int>(c * 255.0f + 0.5f);
    }

    class BasicFxProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            const VertexPN verts[4] = {
                {Vector3(-kHalf, -kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f)},
                {Vector3(kHalf, -kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f)},
                {Vector3(kHalf, kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f)},
                {Vector3(-kHalf, kHalf, 0.0f), Vector3(0.0f, 0.0f, 1.0f)},
            };
            const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
            VertexDeclaration decl({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
            });
            VertexBuffer vb(gd, decl, 4, BufferUsage::WriteOnly);
            vb.SetData(verts, 4);
            IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
            ib.SetData(indices, 6);

            // Unit directions with exactly representable components, chosen so all three lights
            // contribute a DIFFERENT amount -- a model that dropped light 2 would still look plausible
            // if every light contributed the same.
            const Light lights[3] = {
                {Vector3(0.0f, 0.0f, -1.0f), Vector3(0.40f, 0.00f, 0.00f), Vector3(0.20f, 0.00f, 0.00f)},
                {Vector3(0.0f, 0.6f, -0.8f), Vector3(0.00f, 0.50f, 0.00f), Vector3(0.00f, 0.20f, 0.00f)},
                {Vector3(0.8f, 0.0f, -0.6f), Vector3(0.00f, 0.00f, 0.50f), Vector3(0.00f, 0.00f, 0.20f)},
            };
            const Vector3 ambient(0.10f, 0.10f, 0.10f);
            const Vector3 diffuseColor(0.80f, 0.80f, 0.80f);
            const Vector3 emissive(0.02f, 0.02f, 0.02f);
            const Vector3 specularColor(1.0f, 1.0f, 1.0f);
            const float specularPower = 16.0f;
            const Vector3 fogColor(0.10f, 0.20f, 0.30f);
            const float fogStart = 2.0f, fogEnd = 8.0f, eyeDistance = 5.0f;
            const float fogFactor = 1.0f - (eyeDistance - fogStart) / (fogEnd - fogStart);

            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(
                Matrix::CreateLookAt(Vector3(0.0f, 0.0f, eyeDistance), Vector3::Zero, Vector3::Up));
            fx.setProjectionProperty(Matrix::CreateOrthographic(2.0f * kHalf, 2.0f * kHalf, 0.1f, 100.0f));
            fx.setTextureEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);
            fx.setLightingEnabledProperty(true);
            fx.setPreferPerPixelLightingProperty(true);
            fx.setAmbientLightColorProperty(ambient);
            fx.setDiffuseColorProperty(diffuseColor);
            fx.setEmissiveColorProperty(emissive);
            fx.setSpecularColorProperty(specularColor);
            fx.setSpecularPowerProperty(specularPower);
            fx.setAlphaProperty(1.0f);

            DirectionalLight* dl[3] = {&fx.getDirectionalLight0Property(),
                                       &fx.getDirectionalLight1Property(),
                                       &fx.getDirectionalLight2Property()};
            for (int i = 0; i < 3; ++i)
            {
                dl[i]->setEnabledProperty(true);
                dl[i]->setDirectionProperty(lights[i].direction);
                dl[i]->setDiffuseColorProperty(lights[i].diffuse);
                dl[i]->setSpecularColorProperty(lights[i].specular);
            }

            RenderTarget2D rt(gd, kN, kN, false, SurfaceFormat::Color, DepthFormat::None);
            std::vector<Color> out(static_cast<size_t>(kN) * kN);

            auto render = [&]
            {
                gd.SetRenderTarget(&rt);
                gd.Clear(Color::Black);
                gd.setBlendStateProperty(BlendState::Opaque);
                gd.setDepthStencilStateProperty(DepthStencilState::None);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
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
                return out[static_cast<size_t>(kN / 2) * kN + kN / 2];
            };

            const Vector3 normal(0.0f, 0.0f, 1.0f);
            const Vector3 eye(0.0f, 0.0f, 1.0f);

            auto compare = [&](const char* what, const Color& got, const Vector3& want, int tol)
            {
                const int wr = Byte(want.X), wg = Byte(want.Y), wb = Byte(want.Z);
                const int gr = got.getRProperty(), gg = got.getGProperty(), gb = got.getBProperty();
                const int d = std::max({std::abs(gr - wr), std::abs(gg - wg), std::abs(gb - wb)});
                char msg[200];
                std::snprintf(msg,
                              sizeof msg,
                              "got (%d,%d,%d) want (%d,%d,%d), max delta %d",
                              gr,
                              gg,
                              gb,
                              wr,
                              wg,
                              wb,
                              d);
                r.check(what, d <= tol, msg);
                return d;
            };

            // --- each contribution alone, so a disagreement localises --------------------------------
            fx.setFogEnabledProperty(false);
            Light none[3] = {{lights[0].direction, Vector3::Zero, Vector3::Zero},
                             {lights[1].direction, Vector3::Zero, Vector3::Zero},
                             {lights[2].direction, Vector3::Zero, Vector3::Zero}};
            for (int i = 0; i < 3; ++i)
            {
                dl[i]->setDiffuseColorProperty(Vector3::Zero);
                dl[i]->setSpecularColorProperty(Vector3::Zero);
            }
            compare("ambient + emissive alone",
                    render(),
                    Analytic(normal,
                             eye,
                             none,
                             ambient,
                             diffuseColor,
                             emissive,
                             specularColor,
                             specularPower,
                             1.0f,
                             false,
                             fogColor,
                             1.0f),
                    2);

            for (int i = 0; i < 3; ++i)
            {
                dl[i]->setDiffuseColorProperty(lights[i].diffuse);
            }
            Light diffuseOnly[3] = {{lights[0].direction, lights[0].diffuse, Vector3::Zero},
                                    {lights[1].direction, lights[1].diffuse, Vector3::Zero},
                                    {lights[2].direction, lights[2].diffuse, Vector3::Zero}};
            compare("+ three directional diffuse terms",
                    render(),
                    Analytic(normal,
                             eye,
                             diffuseOnly,
                             ambient,
                             diffuseColor,
                             emissive,
                             specularColor,
                             specularPower,
                             1.0f,
                             false,
                             fogColor,
                             1.0f),
                    2);

            for (int i = 0; i < 3; ++i)
            {
                dl[i]->setSpecularColorProperty(lights[i].specular);
            }
            compare("+ three specular terms",
                    render(),
                    Analytic(normal,
                             eye,
                             lights,
                             ambient,
                             diffuseColor,
                             emissive,
                             specularColor,
                             specularPower,
                             1.0f,
                             false,
                             fogColor,
                             1.0f),
                    2);

            // --- everything at once, which is what the task asks -------------------------------------
            fx.setFogEnabledProperty(true);
            fx.setFogColorProperty(fogColor);
            fx.setFogStartProperty(fogStart);
            fx.setFogEndProperty(fogEnd);
            const Color all = render();
            compare("+ fog: three lights, ambient, emissive, specular and fog together",
                    all,
                    Analytic(normal,
                             eye,
                             lights,
                             ambient,
                             diffuseColor,
                             emissive,
                             specularColor,
                             specularPower,
                             1.0f,
                             true,
                             fogColor,
                             fogFactor),
                    2);

            // A fog control: at fogStart the surface must be untouched, past fogEnd it must be the fog
            // colour exactly. Both are easy to get subtly wrong and neither is visible in a screenshot.
            fx.setFogStartProperty(5.0f);
            fx.setFogEndProperty(9.0f);
            compare("at fogStart the surface is untouched",
                    render(),
                    Analytic(normal,
                             eye,
                             lights,
                             ambient,
                             diffuseColor,
                             emissive,
                             specularColor,
                             specularPower,
                             1.0f,
                             false,
                             fogColor,
                             1.0f),
                    2);
            fx.setFogStartProperty(1.0f);
            fx.setFogEndProperty(4.0f);
            compare("past fogEnd the surface is exactly the fog colour", render(), fogColor, 2);

            // --- per-vertex vs per-pixel lighting -----------------------------------------------------
            fx.setFogEnabledProperty(false);
            fx.setPreferPerPixelLightingProperty(false);
            const Color perVertex = render();
            fx.setPreferPerPixelLightingProperty(true);
            const Color perPixel = render();
            char pp[160];
            std::snprintf(pp,
                          sizeof pp,
                          "per-vertex (%d,%d,%d) vs per-pixel (%d,%d,%d)",
                          perVertex.getRProperty(),
                          perVertex.getGProperty(),
                          perVertex.getBProperty(),
                          perPixel.getRProperty(),
                          perPixel.getGProperty(),
                          perPixel.getBProperty());
            r.note("PreferPerPixelLighting", pp);
            // On a flat quad with a constant normal and a near-constant eye vector the two models must
            // agree; a difference here would mean one of them is not evaluating the same lights.
            r.check("both lighting models agree on a flat constant-normal surface",
                    std::abs(perVertex.getRProperty() - perPixel.getRProperty()) <= 2 &&
                        std::abs(perVertex.getGProperty() - perPixel.getGProperty()) <= 2 &&
                        std::abs(perVertex.getBProperty() - perPixel.getBProperty()) <= 2,
                    pp);

            return r.finish("p1-basicfx");
        }
    };

} // namespace

P1_MAIN(BasicFxProbe, "p1-basicfx")
