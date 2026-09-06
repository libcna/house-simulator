// SPDX-License-Identifier: MIT
#include "cnahouse/content/Caches.hpp"

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"

namespace cnahouse::content
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// MEASURED (`HOUSE-00070`, `HOUSE-00087`, `HOUSE-00098`): `Load<T>` returns `Model`, `Texture2D`,
        /// `SoundEffect` and `Video` **by value**, while `Effect`'s reader is registered for
        /// `std::shared_ptr<Effect>`. None of that is guessable from the C# API and CNA reports a mismatch
        /// clearly, so each loader is written out once here rather than assumed at every call site.
        template<typename T>
        AssetCache<T>::Loader ByValueLoader()
        {
            return [](Xna::Content::ContentManager& content, const std::string& name) -> T
            { return content.Load<T>(name); };
        }

    } // namespace

    Caches::Caches(Xna::Content::ContentManager& content)
        : models(content, ByValueLoader<Xna::Graphics::Model>(), "Models/Fallback/box", util::LogCat::Content)
        , textures(content,
                   ByValueLoader<Xna::Graphics::Texture2D>(),
                   "Textures/Fallback/grey",
                   util::LogCat::Content)
        , sounds(
              content, ByValueLoader<Xna::Audio::SoundEffect>(), "Audio/Fallback/silent", util::LogCat::Audio)
        , missing_(content,
                   ByValueLoader<Xna::Graphics::Texture2D>(),
                   "Textures/Fallback/missing",
                   util::LogCat::Content)
    {
    }

    Xna::Graphics::Texture2D* Caches::MissingTexture()
    {
        return missing_.Fallback();
    }

    void Caches::Clear()
    {
        models.Clear();
        textures.Clear();
        sounds.Clear();
        missing_.Clear();
    }

} // namespace cnahouse::content
