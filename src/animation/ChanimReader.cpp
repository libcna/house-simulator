// SPDX-License-Identifier: MIT
#include "cnahouse/animation/ChanimReader.hpp"

#include <format>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::anim
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;

        /// Every rejection goes through here, so every one of them names the file.
        util::Error Bad(ErrorCode code, std::string message, std::string_view name)
        {
            return Err(code, std::move(message), std::string(name));
        }

        Microsoft::Xna::Framework::Vector3 ReadVector3(System::IO::BinaryReader& reader)
        {
            const float x = reader.ReadSingle();
            const float y = reader.ReadSingle();
            const float z = reader.ReadSingle();
            return Microsoft::Xna::Framework::Vector3(x, y, z);
        }

        Microsoft::Xna::Framework::Quaternion ReadQuaternion(System::IO::BinaryReader& reader)
        {
            // `x y z w` -- **w LAST**, matching XNA's constructor and glTF. The w-first order some
            // maths libraries use produces a plausible-looking wrong pose and is invisible in a hex
            // dump, which is why `docs/anim-format.md` §2 says so and why this comment repeats it.
            const float x = reader.ReadSingle();
            const float y = reader.ReadSingle();
            const float z = reader.ReadSingle();
            const float w = reader.ReadSingle();
            return Microsoft::Xna::Framework::Quaternion(x, y, z, w);
        }

        Microsoft::Xna::Framework::Matrix ReadMatrix(System::IO::BinaryReader& reader)
        {
            // Row-major, in `Matrix`'s own member order, so this is a straight sixteen reads.
            float m[16];
            for (float& value : m)
            {
                value = reader.ReadSingle();
            }
            return Microsoft::Xna::Framework::Matrix(m[0],
                                                     m[1],
                                                     m[2],
                                                     m[3],
                                                     m[4],
                                                     m[5],
                                                     m[6],
                                                     m[7],
                                                     m[8],
                                                     m[9],
                                                     m[10],
                                                     m[11],
                                                     m[12],
                                                     m[13],
                                                     m[14],
                                                     m[15]);
        }

        /// `u16` byte length then UTF-8. NOT `BinaryReader::ReadString`, whose 7-bit-encoded length
        /// prefix is a .NET-specific encoding and hostile to any other writer of this format.
        util::Result<std::string> ReadName(System::IO::BinaryReader& reader, std::string_view file)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length > ChanimReader::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name claims {} bytes, above the {} limit",
                                       length,
                                       ChanimReader::kMaxNameBytes),
                           file);
            }
            if (length == 0)
            {
                return Bad(ErrorCode::InvalidData, "a name is empty", file);
            }
            const std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<int>(length));
            if (bytes.size() != length)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name was cut short: {} of {} bytes", bytes.size(), length),
                           file);
            }
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        util::Result<Skeleton>
        ReadSkeleton(System::IO::BinaryReader& reader, std::uint32_t boneCount, std::string_view file)
        {
            Skeleton skeleton;
            skeleton.boneNames.reserve(boneCount);
            skeleton.parent.reserve(boneCount);
            skeleton.bindPose.reserve(boneCount);
            skeleton.inverseBindPose.reserve(boneCount);

            std::unordered_set<std::string> seen;
            for (std::uint32_t i = 0; i < boneCount; ++i)
            {
                auto name = ReadName(reader, file);
                if (!name)
                {
                    return name.Error();
                }
                if (!seen.insert(*name).second)
                {
                    // The name list IS the binding (`HOUSE-00074`); a duplicate makes it ambiguous
                    // and would bind two different joints to one bone.
                    return Bad(ErrorCode::InvalidData,
                               std::format("joint {} repeats the name '{}'", i, *name),
                               file);
                }

                const std::int32_t parent = reader.ReadInt32();
                if (parent < -1 || parent >= static_cast<std::int32_t>(i))
                {
                    // Forward reference or cycle. Rejecting it here is what lets every consumer walk
                    // the hierarchy in one forward pass with no visited set.
                    return Bad(ErrorCode::InvalidData,
                               std::format("joint {} ('{}') has parent {}, which is not an earlier "
                                           "joint",
                                           i,
                                           *name,
                                           parent),
                               file);
                }

                skeleton.boneNames.push_back(std::move(*name));
                skeleton.parent.push_back(parent);
                skeleton.bindPose.push_back(ReadMatrix(reader));
                skeleton.inverseBindPose.push_back(ReadMatrix(reader));
            }

            skeleton.rootBone = reader.ReadInt32();
            if (skeleton.rootBone < -1 || skeleton.rootBone >= static_cast<std::int32_t>(boneCount))
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("rootBone is {}, outside -1..{}", skeleton.rootBone, boneCount - 1),
                           file);
            }

            skeleton.modelBoneIndex.assign(boneCount, -1);
            return skeleton;
        }

        util::Result<Clip>
        ReadClip(System::IO::BinaryReader& reader, std::uint32_t boneCount, std::string_view file)
        {
            Clip clip;
            auto name = ReadName(reader, file);
            if (!name)
            {
                return name.Error();
            }
            clip.name = std::move(*name);

            clip.duration = reader.ReadSingle();
            if (!(clip.duration > 0.0f))
            {
                // `!(x > 0)` rather than `x <= 0`, so a NaN duration is rejected too. A NaN compares
                // false against everything and would otherwise walk straight through a `<=` check.
                return Bad(ErrorCode::InvalidData,
                           std::format("clip '{}' has duration {}, which cannot be sampled",
                                       clip.name,
                                       static_cast<double>(clip.duration)),
                           file);
            }

            const std::uint32_t flags = reader.ReadUInt32();
            if ((flags & ~1u) != 0u)
            {
                // A newer writer put something here. Rejecting rather than masking is the point:
                // silently dropping a flag produces an animation that is subtly wrong forever.
                return Bad(ErrorCode::VersionMismatch,
                           std::format("clip '{}' sets reserved flag bits {:#010x}", clip.name, flags),
                           file);
            }
            clip.loops = (flags & 1u) != 0u;

            clip.strideLength = reader.ReadSingle();
            if (clip.strideLength < 0.0f)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("clip '{}' has a negative stride length", clip.name),
                           file);
            }

            const std::uint32_t footPlantCount = reader.ReadUInt32();
            if (footPlantCount > ChanimReader::kMaxFootPlants)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("clip '{}' claims {} foot plants, above the {} limit",
                                       clip.name,
                                       footPlantCount,
                                       ChanimReader::kMaxFootPlants),
                           file);
            }
            clip.footPlants.reserve(footPlantCount);
            float previousPlant = -1.0f;
            for (std::uint32_t i = 0; i < footPlantCount; ++i)
            {
                const float time = reader.ReadSingle();
                if (!(time >= 0.0f && time <= clip.duration))
                {
                    return Bad(ErrorCode::OutOfRange,
                               std::format("clip '{}' foot plant {} is at {}, outside 0..{}",
                                           clip.name,
                                           i,
                                           static_cast<double>(time),
                                           static_cast<double>(clip.duration)),
                               file);
                }
                if (time < previousPlant)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("clip '{}' foot plants are not ascending at {}", clip.name, i),
                               file);
                }
                previousPlant = time;
                clip.footPlants.push_back(time);
            }

            const std::uint32_t keyCount = reader.ReadUInt32();
            if (keyCount == 0u || keyCount > ChanimReader::kMaxKeysPerClip)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("clip '{}' claims {} keys, outside 1..{}",
                                       clip.name,
                                       keyCount,
                                       ChanimReader::kMaxKeysPerClip),
                           file);
            }

            clip.boneFirstKey.reserve(boneCount + 1u);
            std::uint32_t previousFirst = 0u;
            for (std::uint32_t i = 0; i <= boneCount; ++i)
            {
                const std::uint32_t first = reader.ReadUInt32();
                if (first < previousFirst || first > keyCount)
                {
                    // Non-decreasing and bounded, or the partition is broken and every later index
                    // reads someone else's track.
                    return Bad(ErrorCode::InvalidData,
                               std::format("clip '{}' boneFirstKey[{}] is {}, which breaks the "
                                           "partition (previous {}, keyCount {})",
                                           clip.name,
                                           i,
                                           first,
                                           previousFirst,
                                           keyCount),
                               file);
                }
                previousFirst = first;
                clip.boneFirstKey.push_back(first);
            }
            if (clip.boneFirstKey.back() != keyCount)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("clip '{}' boneFirstKey ends at {}, not at keyCount {}",
                                       clip.name,
                                       clip.boneFirstKey.back(),
                                       keyCount),
                           file);
            }

            clip.keys.reserve(keyCount);
            for (std::uint32_t i = 0; i < keyCount; ++i)
            {
                Keyframe key;
                key.time = reader.ReadSingle();
                if (!(key.time >= 0.0f && key.time <= clip.duration))
                {
                    return Bad(ErrorCode::OutOfRange,
                               std::format("clip '{}' key {} is at {}, outside 0..{}",
                                           clip.name,
                                           i,
                                           static_cast<double>(key.time),
                                           static_cast<double>(clip.duration)),
                               file);
                }
                key.translation = ReadVector3(reader);
                key.rotation = ReadQuaternion(reader);
                key.scale = ReadVector3(reader);
                clip.keys.push_back(key);
            }

            // Ascending WITHIN each bone's track, checked after the keys are read because the
            // partition is what says where each track starts. The sampler binary-searches, so
            // unsorted input does not fail -- it silently returns the wrong pose.
            for (std::uint32_t bone = 0; bone < boneCount; ++bone)
            {
                const auto [begin, end] = clip.TrackFor(bone);
                for (std::uint32_t i = begin + 1u; i < end; ++i)
                {
                    if (clip.keys[i].time < clip.keys[i - 1u].time)
                    {
                        return Bad(
                            ErrorCode::InvalidData,
                            std::format(
                                "clip '{}' bone {} has key times out of order at {}", clip.name, bone, i),
                            file);
                    }
                }
            }

            return clip;
        }

    } // namespace

    util::Result<ClipLibrary> ChanimReader::Read(System::IO::Stream& stream, std::string_view name)
    {
        try
        {
            System::IO::BinaryReader reader(&stream, true);

            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                // Said first and said plainly: the wrong file entirely. Reading on would interpret
                // arbitrary bytes as lengths and produce a far less useful error.
                return Bad(ErrorCode::InvalidData,
                           std::format("magic is {:#010x}, not 'CHAN' ({:#010x})", magic, kMagic),
                           name);
            }

            const std::uint32_t version = reader.ReadUInt32();
            if (version != kVersion)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format(
                               "version {} is not supported; this build reads version {}", version, kVersion),
                           name);
            }

            const std::uint32_t flags = reader.ReadUInt32();
            if (flags != 0u)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format("reserved header flags {:#010x} are set", flags),
                           name);
            }

            const std::uint32_t boneCount = reader.ReadUInt32();
            if (boneCount == 0u || boneCount > kMaxBones)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("boneCount is {}, outside 1..{}", boneCount, kMaxBones),
                           name);
            }

            const std::uint32_t clipCount = reader.ReadUInt32();
            if (clipCount > kMaxClips)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("clipCount is {}, above the {} limit", clipCount, kMaxClips),
                           name);
            }

            auto skeleton = ReadSkeleton(reader, boneCount, name);
            if (!skeleton)
            {
                return skeleton.Error();
            }

            std::vector<Clip> clips;
            clips.reserve(clipCount);
            std::unordered_set<std::string> clipNames;
            for (std::uint32_t i = 0; i < clipCount; ++i)
            {
                auto clip = ReadClip(reader, boneCount, name);
                if (!clip)
                {
                    return clip.Error();
                }
                if (!clipNames.insert(clip->name).second)
                {
                    return Bad(ErrorCode::Duplicate,
                               std::format("clip '{}' appears more than once", clip->name),
                               name);
                }
                clips.push_back(std::move(*clip));
            }

            ClipLibrary library;
            library.Assign(std::move(*skeleton), std::move(clips));
            return library;
        }
        catch (const std::exception& e)
        {
            // `BinaryReader` throws at the end of the stream, so TRUNCATION arrives here rather than
            // as a short read. It is the one failure the field-by-field checks above cannot see, and
            // it must be an error rather than a skeleton that is quietly missing its last joints.
            return Bad(ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<ClipLibrary> ChanimReader::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
            // `TitleContainer::OpenStream` is plain XNA 4.0 and is how every platform this project
            // targets opens a read-only asset -- including the Web build, where there is no
            // filesystem to open a `FileStream` on.
            std::unique_ptr<System::IO::Stream> stream =
                Microsoft::Xna::Framework::TitleContainer::OpenStream(std::string(contentPath));
            if (stream == nullptr)
            {
                return Bad(ErrorCode::NotFound, "the file could not be opened", contentPath);
            }
            return Read(*stream, contentPath);
        }
        catch (const std::exception& e)
        {
            return Bad(ErrorCode::NotFound, e.what(), contentPath);
        }
    }

} // namespace cnahouse::anim
