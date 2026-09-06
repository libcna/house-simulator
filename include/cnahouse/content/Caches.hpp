// SPDX-License-Identifier: MIT
#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/content/AssetCache.hpp"

namespace cnahouse::content
{

    /// @brief The four typed caches, constructed with the loader each asset type actually needs.
    ///
    /// **The loaders differ because `ContentManager::Load<T>` has three different shapes**, measured in
    /// phase 1 and not guessable from the C# API: `Model` and `Video` return **by value**, `Effect` is
    /// registered for `std::shared_ptr<Effect>`, and `Texture2D` and `SoundEffect` return by value too.
    /// Wrapping each in a lambda here is what keeps that inconsistency in one file instead of at every
    /// call site.
    ///
    /// The fallback names are the assets `tools/assets/make_fallback_assets.py` generates -- authored
    /// procedurally, so their provenance is settled completely (ADR-0012) and no licence question
    /// reaches the one asset guaranteed to be in every build.
    struct Caches
    {
        explicit Caches(Microsoft::Xna::Framework::Content::ContentManager& content);

        AssetCache<Microsoft::Xna::Framework::Graphics::Model> models;
        AssetCache<Microsoft::Xna::Framework::Graphics::Texture2D> textures;
        AssetCache<Microsoft::Xna::Framework::Audio::SoundEffect> sounds;

        /// @brief The magenta checker, for a texture slot that must show it is wrong.
        ///
        /// Separate from `textures`' own fallback, which is neutral grey: a missing *albedo* should
        /// keep the lighting readable, while a missing *required* texture should be unmissable.
        [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D* MissingTexture();

        void Clear();

    private:
        AssetCache<Microsoft::Xna::Framework::Graphics::Texture2D> missing_;
    };

} // namespace cnahouse::content
