// SPDX-License-Identifier: MIT
//
// `HOUSE-01741`. The CPU side is a fixed array with a preset-selected ceiling; the real-device
// integration test owns the DynamicVertexBuffer and one-draw-per-material boundary.
#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/rendering/ParticleRenderer.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    using cnahouse::rendering::ParticleQuad;

    ParticleQuad Quad(std::uint8_t material = 0u)
    {
        ParticleQuad quad;
        quad.centre = Xna::Vector3(1.0F, 2.0F, -4.0F);
        quad.halfSize = Xna::Vector2(0.1F, 0.4F);
        quad.colour = Xna::Color(128, 128, 128, 128);
        quad.material = material;
        return quad;
    }

    TEST(ParticleRendererTests, EveryPresetUsesTheSameBoundedFixedPool)
    {
        using cnahouse::app::QualityPreset;
        using cnahouse::rendering::ParticleLimitFor;
        using cnahouse::rendering::SettingsFor;
        EXPECT_EQ(ParticleLimitFor(SettingsFor(QualityPreset::Low).particles), 500u);
        EXPECT_EQ(ParticleLimitFor(SettingsFor(QualityPreset::Medium).particles), 1000u);
        EXPECT_EQ(ParticleLimitFor(SettingsFor(QualityPreset::High).particles), 1000u);
        EXPECT_EQ(ParticleLimitFor(SettingsFor(QualityPreset::Ultra).particles), 2000u);

        cnahouse::rendering::Camera camera;
        cnahouse::rendering::ParticleRenderer renderer(camera);
        renderer.BeginFrame(SettingsFor(QualityPreset::Low).particles);
        const ParticleQuad* storage = renderer.Particles().data();
        for (std::size_t index = 0u; index < renderer.Capacity(); ++index)
        {
            ASSERT_TRUE(renderer.Submit(Quad()));
        }
        EXPECT_FALSE(renderer.Submit(Quad()));
        EXPECT_EQ(renderer.Particles().size(), 500u);
        EXPECT_EQ(renderer.RejectedCount(), 1u);

        renderer.BeginFrame(SettingsFor(QualityPreset::Ultra).particles);
        EXPECT_EQ(renderer.Particles().data(), storage) << "BeginFrame must reuse the in-object pool";
        EXPECT_TRUE(renderer.Particles().empty());
        EXPECT_EQ(renderer.Capacity(), cnahouse::rendering::kMaximumParticleQuads);

        ParticleQuad invalid = Quad();
        invalid.halfSize.X = 0.0F;
        EXPECT_FALSE(renderer.Submit(invalid));
        invalid = Quad(static_cast<std::uint8_t>(cnahouse::rendering::kMaximumParticleMaterials));
        EXPECT_FALSE(renderer.Submit(invalid));
    }

    TEST(ParticleRendererTests, QuadFacesTheCameraAndKeepsItsRequestedDimensions)
    {
        cnahouse::rendering::Camera camera;
        camera.eye = Xna::Vector3(0.0F, 1.0F, 2.0F);
        camera.target = Xna::Vector3(0.0F, 1.0F, 0.0F);
        const ParticleQuad particle = Quad();
        const auto vertices = cnahouse::rendering::ParticleBillboardVertices(particle, camera);

        for (const auto& vertex : vertices)
        {
            EXPECT_FLOAT_EQ(vertex.Position.Z, particle.centre.Z);
            EXPECT_EQ(vertex.Color, particle.colour);
        }
        EXPECT_NEAR(vertices[1].Position.X - vertices[0].Position.X, 2.0F * particle.halfSize.X, 1.0e-6F);
        EXPECT_NEAR(vertices[3].Position.Y - vertices[0].Position.Y, 2.0F * particle.halfSize.Y, 1.0e-6F);
    }
} // namespace
