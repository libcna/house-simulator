// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/MaterialBinder.hpp"

#include <cmath>
#include <format>

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/world/WorldTypes.hpp"

namespace cnahouse::rendering
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using util::Err;
    using util::ErrorCode;
    using util::Ok;

    namespace
    {
        Vector3 ToVector(const float (&rgb)[3]) noexcept
        {
            return Vector3(rgb[0], rgb[1], rgb[2]);
        }

        Vector3 Modulated(const float (&rgb)[3], const Vector3& multiplier) noexcept
        {
            return Vector3(rgb[0] * multiplier.X, rgb[1] * multiplier.Y, rgb[2] * multiplier.Z);
        }

        /// MEASURED: `Matrix::getIdentityProperty()` returns BY VALUE, not by reference, so this
        /// cannot hand back a reference to it. A shorter name for it at the call sites that need a
        /// default, and nothing more.
        Matrix Identity() noexcept
        {
            return Matrix::getIdentityProperty();
        }

        void ApplyMatrices(auto& effect, const DrawParams& draw)
        {
            effect.setWorldProperty(draw.world != nullptr ? *draw.world : Identity());
            effect.setViewProperty(draw.view != nullptr ? *draw.view : Identity());
            effect.setProjectionProperty(draw.projection != nullptr ? *draw.projection : Identity());
        }

        void ApplyFog(auto& effect, const DrawParams& draw)
        {
            effect.setFogEnabledProperty(draw.fog != nullptr);
            if (draw.fog != nullptr)
            {
                effect.setFogColorProperty(ToVector(draw.fog->colour));
                effect.setFogStartProperty(draw.fog->start);
                effect.setFogEndProperty(draw.fog->end);
            }
        }

        MaterialKind ToMaterialKind(world::EffectTier tier) noexcept
        {
            switch (tier)
            {
                case world::EffectTier::Basic:
                    return MaterialKind::Basic;
                case world::EffectTier::DualTexture:
                    return MaterialKind::DualTexture;
                case world::EffectTier::AlphaTest:
                    return MaterialKind::AlphaTest;
                case world::EffectTier::Skinned:
                    return MaterialKind::Skinned;
            }
            return MaterialKind::Basic;
        }

        MaterialDesc Describe(const world::MaterialDef& definition)
        {
            MaterialDesc desc;
            desc.kind = ToMaterialKind(definition.effectTierS);
            desc.diffuseTexture = definition.albedo;
            desc.diffuse[0] = definition.tint.X;
            desc.diffuse[1] = definition.tint.Y;
            desc.diffuse[2] = definition.tint.Z;
            desc.alpha = definition.alpha;
            desc.specularColour[0] = definition.specularColor.X;
            desc.specularColour[1] = definition.specularColor.Y;
            desc.specularColour[2] = definition.specularColor.Z;
            desc.specularPower = definition.specularPower;
            desc.twoSided = definition.twoSided;
            desc.lightingEnabled = definition.materialClass != world::MaterialClass::Emissive;
            if (definition.alphaCutoff.has_value())
            {
                desc.referenceAlpha = static_cast<int>(std::lround(*definition.alphaCutoff * 255.0F));
            }
            return desc;
        }
    } // namespace

    std::string_view MaterialKindName(MaterialKind kind) noexcept
    {
        switch (kind)
        {
            case MaterialKind::Basic:
                return "basic";
            case MaterialKind::DualTexture:
                return "dual-texture";
            case MaterialKind::AlphaTest:
                return "alpha-test";
            case MaterialKind::Skinned:
                return "skinned";
        }
        return "?";
    }

    MaterialBinder::MaterialBinder(Gfx::GraphicsDevice& device) noexcept
        : device_(&device)
    {
    }

    MaterialBinder::~MaterialBinder() = default;

    util::Result<void> MaterialBinder::Register(util::Id id, MaterialDesc desc)
    {
        if (!id.IsValid())
        {
            return Err(ErrorCode::InvalidArgument, "a material id of 0 means 'no material'");
        }
        if (materials_.contains(id))
        {
            return Err(ErrorCode::Duplicate,
                       std::format("material {:#010x} is already registered", id.Value()));
        }

        // Both of these are MEASURED impossibilities, refused HERE rather than inside a frame. A
        // material that cannot be drawn should fail where the error can name it, not two hundred
        // draws later where it can only name the effect.
        if (desc.kind == MaterialKind::Skinned && !desc.lightingEnabled)
        {
            // MEASURED (`HOUSE-00077`): `SkinnedEffect` throws "SkinnedEffect does not support
            // setting LightingEnabled to false.", exactly as XNA 4.0 does. `lightingEnabled` is the
            // material switch, so an unlit skinned material is the shape that would trip it.
            return Err(ErrorCode::Unsupported,
                       std::format("material {:#010x} is skinned and unlit, and SkinnedEffect "
                                   "refuses LightingEnabled = false",
                                   id.Value()));
        }
        if (desc.kind == MaterialKind::AlphaTest && (desc.referenceAlpha < 0 || desc.referenceAlpha > 255))
        {
            return Err(ErrorCode::OutOfRange,
                       std::format("material {:#010x} has reference alpha {}, outside 0..255",
                                   id.Value(),
                                   desc.referenceAlpha));
        }

        materials_.emplace(id, std::move(desc));
        return Ok();
    }

    util::Result<void> MaterialBinder::Register(const world::MaterialDef& definition)
    {
        if (definition.alpha < 0.0F || definition.alpha > 1.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       std::format("material {:#010x} has alpha {}, outside 0..1",
                                   definition.id.Value(),
                                   definition.alpha));
        }
        if (definition.alphaCutoff.has_value() &&
            (*definition.alphaCutoff < 0.0F || *definition.alphaCutoff > 1.0F))
        {
            return Err(ErrorCode::OutOfRange,
                       std::format("material {:#010x} has alpha cutoff {}, outside 0..1",
                                   definition.id.Value(),
                                   *definition.alphaCutoff));
        }
        if (definition.alphaMode == world::AlphaMode::Mask && !definition.alphaCutoff.has_value())
        {
            return Err(
                ErrorCode::InvalidData,
                std::format("material {:#010x} is alpha-masked but has no cutoff", definition.id.Value()));
        }
        return Register(definition.id, Describe(definition));
    }

    util::Result<void> MaterialBinder::RegisterAll(std::span<const world::MaterialDef> definitions)
    {
        std::vector<util::Id> inserted;
        inserted.reserve(definitions.size());
        for (const world::MaterialDef& definition : definitions)
        {
            if (auto result = Register(definition); !result)
            {
                for (const util::Id id : inserted)
                {
                    materials_.erase(id);
                }
                return result;
            }
            inserted.push_back(definition.id);
        }
        return Ok();
    }

    const MaterialDesc* MaterialBinder::Find(util::Id id) const noexcept
    {
        const auto it = materials_.find(id);
        return it == materials_.end() ? nullptr : &it->second;
    }

    util::Result<CullPolicy> MaterialBinder::CullFor(util::Id id, float determinant) const noexcept
    {
        const MaterialDesc* desc = Find(id);
        if (desc == nullptr)
        {
            return Err(ErrorCode::NotFound, std::format("no material {:#010x} is registered", id.Value()));
        }
        return PolicyForDeterminant(determinant, desc->twoSided);
    }

    Gfx::BasicEffect& MaterialBinder::BasicFor()
    {
        if (basic_ == nullptr)
        {
            basic_ = std::make_unique<Gfx::BasicEffect>(*device_);
            ++effectsCreated_;
        }
        return *basic_;
    }

    Gfx::DualTextureEffect& MaterialBinder::DualTextureFor()
    {
        if (dualTexture_ == nullptr)
        {
            dualTexture_ = std::make_unique<Gfx::DualTextureEffect>(*device_);
            ++effectsCreated_;
        }
        return *dualTexture_;
    }

    Gfx::AlphaTestEffect& MaterialBinder::AlphaTestFor()
    {
        if (alphaTest_ == nullptr)
        {
            alphaTest_ = std::make_unique<Gfx::AlphaTestEffect>(*device_);
            ++effectsCreated_;
        }
        return *alphaTest_;
    }

    Gfx::SkinnedEffect& MaterialBinder::SkinnedFor()
    {
        if (skinned_ == nullptr)
        {
            skinned_ = std::make_unique<Gfx::SkinnedEffect>(*device_);
            ++effectsCreated_;
        }
        return *skinned_;
    }

    Gfx::EnvironmentMapEffect& MaterialBinder::EnvironmentMapFor()
    {
        if (environmentMap_ == nullptr)
        {
            environmentMap_ = std::make_unique<Gfx::EnvironmentMapEffect>(*device_);
            ++effectsCreated_;
        }
        return *environmentMap_;
    }

    util::Result<Gfx::Effect*> MaterialBinder::Bind(util::Id id, const DrawParams& draw)
    {
        const MaterialDesc* desc = Find(id);
        if (desc == nullptr)
        {
            return Err(ErrorCode::NotFound, std::format("no material {:#010x} is registered", id.Value()));
        }

        switch (desc->kind)
        {
            case MaterialKind::Basic:
            {
                auto& effect = BasicFor();
                ApplyMatrices(effect, draw);
                effect.setDiffuseColorProperty(Modulated(desc->diffuse, draw.colourMultiplier));
                effect.setAlphaProperty(desc->alpha);
                effect.setSpecularColorProperty(ToVector(desc->specularColour));
                effect.setSpecularPowerProperty(desc->specularPower);
                effect.setAmbientLightColorProperty(draw.ambientLight);
                effect.setVertexColorEnabledProperty(desc->vertexColour);
                effect.setLightingEnabledProperty(desc->lightingEnabled);
                effect.setPreferPerPixelLightingProperty(desc->perPixelLighting);
                auto& key = effect.getDirectionalLight0Property();
                if (desc->lightingEnabled && draw.basicKey.has_value())
                {
                    key.setDirectionProperty(draw.basicKey->direction);
                    key.setDiffuseColorProperty(draw.basicKey->diffuse);
                    key.setSpecularColorProperty(draw.basicKey->specular);
                    key.setEnabledProperty(true);
                }
                else
                {
                    key.setEnabledProperty(false);
                }
                effect.getDirectionalLight1Property().setEnabledProperty(false);
                effect.getDirectionalLight2Property().setEnabledProperty(false);
                effect.setTextureEnabledProperty(draw.diffuse != nullptr);
                if (draw.diffuse != nullptr)
                {
                    effect.setTextureProperty(draw.diffuse);
                }
                ApplyFog(effect, draw);
                return &effect;
            }
            case MaterialKind::DualTexture:
            {
                if (draw.diffuse == nullptr || draw.lightmap == nullptr)
                {
                    return Err(ErrorCode::InvalidArgument,
                               std::format("material {:#010x} is dual-texture and needs both an "
                                           "albedo and a per-draw lightmap",
                                           id.Value()));
                }
                // MEASURED (`HOUSE-00078`): the product carries FNA's `*2` doubling factor, so a
                // lightmap authored for it must be authored at half. Do NOT compensate here as well --
                // the probe confirmed 128 x 128 -> 128, and a second correction would halve every room.
                auto& effect = DualTextureFor();
                ApplyMatrices(effect, draw);
                effect.setDiffuseColorProperty(Modulated(desc->diffuse, draw.colourMultiplier));
                effect.setAlphaProperty(desc->alpha);
                effect.setVertexColorEnabledProperty(desc->vertexColour);
                effect.setTextureProperty(draw.diffuse);
                effect.setTexture2Property(draw.lightmap);
                ApplyFog(effect, draw);
                return &effect;
            }
            case MaterialKind::AlphaTest:
            {
                if (draw.diffuse == nullptr)
                {
                    return Err(ErrorCode::InvalidArgument,
                               std::format("material {:#010x} is alpha-tested and needs an albedo "
                                           "texture",
                                           id.Value()));
                }
                auto& effect = AlphaTestFor();
                ApplyMatrices(effect, draw);
                effect.setDiffuseColorProperty(Modulated(desc->diffuse, draw.colourMultiplier));
                effect.setAlphaProperty(desc->alpha);
                effect.setVertexColorEnabledProperty(desc->vertexColour);
                // `Greater` and not `GreaterEqual`: with a reference of 128 this keeps 129 and drops
                // 128, and `HOUSE-00080` measured the boundary to be exact to one alpha value for all
                // six functions -- so which one is chosen is a visible decision, not a detail.
                effect.setAlphaFunctionProperty(Gfx::CompareFunction::Greater);
                effect.setReferenceAlphaProperty(desc->referenceAlpha);
                effect.setTextureProperty(draw.diffuse);
                ApplyFog(effect, draw);
                return &effect;
            }
            case MaterialKind::Skinned:
            {
                if (draw.bones == nullptr || draw.bones->empty())
                {
                    return Err(ErrorCode::InvalidArgument,
                               std::format("material {:#010x} is skinned and needs a non-empty bone "
                                           "palette",
                                           id.Value()));
                }
                if (draw.bones->size() > kMaxBones)
                {
                    // MEASURED (`HOUSE-00077`): 72 accepted, 73 throws "boneTransforms exceeds
                    // MaxBones.". Reported rather than thrown, because a skin one bone over the limit
                    // is a content problem and the frame should say so and keep going.
                    return Err(ErrorCode::OutOfRange,
                               std::format("material {:#010x} was bound with {} bones; the measured "
                                           "SkinnedEffect limit is {}",
                                           id.Value(),
                                           draw.bones->size(),
                                           kMaxBones));
                }
                if (draw.diffuse == nullptr)
                {
                    return Err(
                        ErrorCode::InvalidArgument,
                        std::format("material {:#010x} is skinned and needs an albedo texture", id.Value()));
                }
                auto& effect = SkinnedFor();
                ApplyMatrices(effect, draw);
                effect.setDiffuseColorProperty(Modulated(desc->diffuse, draw.colourMultiplier));
                effect.setAlphaProperty(desc->alpha);
                effect.setSpecularColorProperty(ToVector(desc->specularColour));
                effect.setSpecularPowerProperty(desc->specularPower);
                effect.setAmbientLightColorProperty(draw.ambientLight);
                effect.setPreferPerPixelLightingProperty(desc->perPixelLighting);
                effect.setWeightsPerVertexProperty(4);
                effect.setTextureProperty(draw.diffuse);
                ApplyFog(effect, draw);
                // The palette is fixed-length, so a shorter one is padded with identities. MEASURED
                // (`HOUSE-00075`): blend indices are SKIN-LOCAL, so slot i is joint i of this skin and
                // padding beyond the skin's joint count is never referenced.
                //
                // REUSED, not rebuilt. MEASURED (`HOUSE-00029`): a fresh 72-matrix vector costs
                // 149 ns against 107 ns to refill one, on a per-skinned-draw path.
                palette_.assign(kMaxBones, Identity());
                for (std::size_t i = 0; i < draw.bones->size(); ++i)
                {
                    palette_[i] = (*draw.bones)[i];
                }
                effect.SetBoneTransforms(palette_);
                return &effect;
            }
        }
        return Err(ErrorCode::Unknown, "unreachable: a material kind with no case");
    }

    util::Result<Gfx::Effect*> MaterialBinder::BindEnvironmentMap(util::Id id,
                                                                  const DrawParams& draw,
                                                                  const EnvironmentMapParams& environment)
    {
        const MaterialDesc* desc = Find(id);
        if (desc == nullptr)
        {
            return Err(ErrorCode::NotFound, std::format("no material {:#010x} is registered", id.Value()));
        }
        if (!desc->lightingEnabled)
        {
            return Err(ErrorCode::Unsupported,
                       std::format("material {:#010x} is unlit, and EnvironmentMapEffect requires "
                                   "lighting",
                                   id.Value()));
        }
        if (draw.diffuse == nullptr || environment.cubeMap == nullptr)
        {
            return Err(ErrorCode::InvalidArgument,
                       std::format("material {:#010x} needs both an albedo and a baked cube map for "
                                   "its environment pass",
                                   id.Value()));
        }
        if (!std::isfinite(environment.amount) || environment.amount < 0.0F || environment.amount > 1.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       std::format("material {:#010x} has environment-map amount {}, outside 0..1",
                                   id.Value(),
                                   environment.amount));
        }
        if (!std::isfinite(environment.fresnelFactor) || environment.fresnelFactor < 0.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       std::format("material {:#010x} has negative or non-finite Fresnel factor {}",
                                   id.Value(),
                                   environment.fresnelFactor));
        }

        auto& effect = EnvironmentMapFor();
        ApplyMatrices(effect, draw);
        effect.setDiffuseColorProperty(Modulated(desc->diffuse, draw.colourMultiplier));
        effect.setAlphaProperty(desc->alpha);
        effect.setTextureProperty(draw.diffuse);
        effect.setEnvironmentMapProperty(environment.cubeMap);
        effect.setEnvironmentMapAmountProperty(environment.amount);
        effect.setEnvironmentMapSpecularProperty(ToVector(desc->specularColour));
        effect.setFresnelFactorProperty(environment.fresnelFactor);
        ApplyFog(effect, draw);
        return &effect;
    }

} // namespace cnahouse::rendering
