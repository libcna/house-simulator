// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Result.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class Model;
}

namespace cnahouse::anim
{

    /// @brief One joint pose at one time. `cna-house.md` §47.0.
    ///
    /// **No bone index.** Which bone a key belongs to is given by `Clip::boneFirstKey`, which is the
    /// same information without repeating it 100 000 times -- a key is 40 bytes on disc and an index
    /// would add 10 % to the largest section of the file for something already implied by position.
    struct Keyframe
    {
        float time = 0.0f;
        Microsoft::Xna::Framework::Vector3 translation;
        Microsoft::Xna::Framework::Quaternion rotation;
        Microsoft::Xna::Framework::Vector3 scale;
    };

    /// @brief One animation. Keys are sorted by bone, then by time.
    struct Clip
    {
        std::string name;
        /// @brief Seconds. Always > 0 -- a clip that cannot be sampled is rejected at load.
        float duration = 0.0f;
        bool loops = false;

        /// @brief Metres of ground travel per cycle; **0** for a non-locomotion clip.
        ///
        /// `cna-house.md` §47.4: this single number is what removes foot sliding, by driving playback
        /// from the distance actually covered rather than from a hand-tuned rate.
        float strideLength = 0.0f;

        /// @brief Contact times in [0, duration], ascending. §48's foot-plant markers.
        std::vector<float> footPlants;

        std::vector<Keyframe> keys;

        /// @brief `boneCount + 1` entries: bone *b*'s track is `[boneFirstKey[b], boneFirstKey[b+1])`.
        ///
        /// The extra entry removes the special case for the last bone. An EMPTY range is legal and
        /// means that bone holds its bind pose.
        std::vector<std::uint32_t> boneFirstKey;

        /// @brief The half-open key range for @p bone, or an empty range if it has none.
        [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> TrackFor(std::size_t bone) const noexcept
        {
            if (bone + 1 >= boneFirstKey.size())
            {
                return {0u, 0u};
            }
            return {boneFirstKey[bone], boneFirstKey[bone + 1]};
        }
    };

    /// @brief The skeleton, in **glTF skin-joint order -- which is blend-index order**.
    ///
    /// MEASURED (`HOUSE-00074`): vertex blend indices are SKIN-LOCAL, not `Model::Bones` indices, and
    /// nothing in the compiled `Model` reproduces which bone slot *i* is. `boneNames` **is** the
    /// binding, which is the whole reason the `.chanim` sidecar exists.
    struct Skeleton
    {
        std::vector<std::string> boneNames;
        /// @brief `-1` for a root. Strictly less than the child's own index, so the absolute-transform
        ///        walk is one forward pass and a cycle is unrepresentable.
        std::vector<std::int32_t> parent;
        /// @brief Local, per bone.
        std::vector<Microsoft::Xna::Framework::Matrix> bindPose;
        /// @brief World -> bone at bind time. Taken from the source asset offline, never from CNA.
        std::vector<Microsoft::Xna::Framework::Matrix> inverseBindPose;

        /// @brief Filled by `ClipLibrary::BindTo`. Index into `Model::Bones`, or -1 before binding.
        ///
        /// **Not stored in the file**, because it is a claim about a model the file has never seen.
        std::vector<std::int32_t> modelBoneIndex;

        /// @brief The joint locomotion is measured against -- usually the hips -- or `-1`.
        std::int32_t rootBone = -1;

        [[nodiscard]] std::size_t Count() const noexcept
        {
            return boneNames.size();
        }

        /// @brief Whether `BindTo` has run and every joint resolved.
        [[nodiscard]] bool IsBound() const noexcept;
    };

    /// @brief A skeleton and its clips: what one `.chanim` file contains.
    class ClipLibrary
    {
    public:
        [[nodiscard]] const Skeleton& GetSkeleton() const noexcept
        {
            return skeleton_;
        }

        [[nodiscard]] const std::vector<Clip>& Clips() const noexcept
        {
            return clips_;
        }

        /// @brief The clip named @p name, or null.
        [[nodiscard]] const Clip* Find(std::string_view name) const;

        /// @brief Resolves every joint name against @p model's bones.
        ///
        /// **A mismatch is a fatal content error naming the joint, never a silent deformation**
        /// (`cna-house.md` §47.0). It is RETURNED rather than thrown: §47.0's sketch said "throws",
        /// but `docs/conventions.md` §5.4 makes malformed content a recoverable failure, and a
        /// `Result` still lets the caller treat it as fatal while a throw does not let it do
        /// anything else. Recorded in `plan.md` under `HOUSE-00167`.
        ///
        /// Resolution uses `ModelBoneCollection::TryGetValue`, because MEASURED: the by-name
        /// indexer `bones[name]` **throws** for an unknown name rather than returning null.
        [[nodiscard]] util::Result<void> BindTo(const Microsoft::Xna::Framework::Graphics::Model& model);

        /// @brief Replaces the contents. Used by the reader, which builds one of these.
        void Assign(Skeleton skeleton, std::vector<Clip> clips);

    private:
        Skeleton skeleton_;
        std::vector<Clip> clips_;
        std::unordered_map<std::string, std::size_t> byName_;
    };

} // namespace cnahouse::anim
