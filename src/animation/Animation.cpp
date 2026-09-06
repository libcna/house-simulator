// SPDX-License-Identifier: MIT
#include "cnahouse/animation/Animation.hpp"

#include <format>

#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBoneCollection.hpp"

namespace cnahouse::anim
{
    using util::Err;
    using util::ErrorCode;
    using util::Ok;

    bool Skeleton::IsBound() const noexcept
    {
        if (boneNames.empty() || modelBoneIndex.size() != boneNames.size())
        {
            return false;
        }
        for (const std::int32_t index : modelBoneIndex)
        {
            if (index < 0)
            {
                return false;
            }
        }
        return true;
    }

    const Clip* ClipLibrary::Find(std::string_view name) const
    {
        const auto it = byName_.find(std::string(name));
        return it == byName_.end() ? nullptr : &clips_[it->second];
    }

    void ClipLibrary::Assign(Skeleton skeleton, std::vector<Clip> clips)
    {
        skeleton_ = std::move(skeleton);
        clips_ = std::move(clips);
        byName_.clear();
        for (std::size_t i = 0; i < clips_.size(); ++i)
        {
            byName_.emplace(clips_[i].name, i);
        }
    }

    util::Result<void> ClipLibrary::BindTo(const Microsoft::Xna::Framework::Graphics::Model& model)
    {
        const auto& bones = model.getBonesProperty();

        // Reset first: a second `BindTo` against a different model must not leave half the previous
        // model's indices behind, which would deform silently rather than fail.
        skeleton_.modelBoneIndex.assign(skeleton_.boneNames.size(), -1);

        for (std::size_t i = 0; i < skeleton_.boneNames.size(); ++i)
        {
            const std::string& name = skeleton_.boneNames[i];

            // `TryGetValue`, NOT `bones[name]`. MEASURED: the by-name indexer **throws**
            // `"ModelBoneCollection: bone not found: <name>"` for an unknown name; it does not
            // return null, which is what the first version of this function assumed and what a test
            // with a deliberately wrong joint name caught. `TryGetValue` is plain XNA 4.0 (only the
            // iterators on this collection are CNAEXT) and turns the miss into the `Result` this
            // project reports failures with, rather than an exception crossing a non-content
            // boundary.
            Microsoft::Xna::Framework::Graphics::ModelBone* bone = nullptr;
            if (!bones.TryGetValue(name, bone) || bone == nullptr)
            {
                // The joint is NAMED, and so is its blend index. A message saying only "the skeleton
                // does not match" would leave someone comparing two lists of sixty-two names by eye.
                skeleton_.modelBoneIndex.assign(skeleton_.boneNames.size(), -1);
                return Err(
                    ErrorCode::SchemaMismatch,
                    std::format("skin joint {} is named '{}', which the model has no bone for", i, name),
                    "chanim/BindTo");
            }
            skeleton_.modelBoneIndex[i] = bone->getIndexProperty();
        }
        return Ok();
    }

} // namespace cnahouse::anim
