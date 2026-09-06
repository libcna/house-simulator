// SPDX-License-Identifier: MIT
//
// `HOUSE-00167`. Every test here builds a `.chanim` byte-for-byte in memory, so what is being read
// is known exactly rather than trusted from a generator that does not exist yet
// (`tools/assets/anim_extract.py` is `HOUSE-00223`). Most of the tests are REJECTIONS: a reader
// whose validation has never been shown to fire is a reader that will accept a corrupt file.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/animation/ChanimReader.hpp"

namespace
{
    using cnahouse::anim::ChanimReader;
    using cnahouse::anim::ClipLibrary;
    using cnahouse::util::ErrorCode;

    /// Builds a `.chanim` the way `docs/anim-format.md` describes it, one field at a time.
    class Writer
    {
    public:
        void U32(std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
            {
                bytes_.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu));
            }
        }

        void I32(std::int32_t value)
        {
            U32(static_cast<std::uint32_t>(value));
        }

        void U16(std::uint16_t value)
        {
            bytes_.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
        }

        void F32(float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            U32(bits);
        }

        void Name(const std::string& text)
        {
            U16(static_cast<std::uint16_t>(text.size()));
            for (const char c : text)
            {
                bytes_.push_back(static_cast<std::uint8_t>(c));
            }
        }

        void Identity()
        {
            for (int row = 0; row < 4; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    F32(row == column ? 1.0f : 0.0f);
                }
            }
        }

        void Vector3(float x, float y, float z)
        {
            F32(x);
            F32(y);
            F32(z);
        }

        void UnitQuaternion()
        {
            // x y z w, w LAST.
            F32(0.0f);
            F32(0.0f);
            F32(0.0f);
            F32(1.0f);
        }

        [[nodiscard]] std::vector<std::uint8_t>& Bytes() noexcept
        {
            return bytes_;
        }

    private:
        std::vector<std::uint8_t> bytes_;
    };

    /// A two-joint skeleton and one two-key clip. The smallest file that exercises every section.
    Writer MinimalFile()
    {
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0); // flags
        w.U32(2); // boneCount
        w.U32(1); // clipCount

        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();

        w.Name("spine");
        w.I32(0);
        w.Identity();
        w.Identity();

        w.I32(0); // rootBone

        w.Name("walk_fwd");
        w.F32(1.06f); // duration
        w.U32(1);     // flags: loops
        w.F32(1.43f); // strideLength
        w.U32(2);     // footPlantCount
        w.F32(0.0f);
        w.F32(0.53f);
        w.U32(3); // keyCount
        // boneFirstKey: bone 0 gets keys [0,2), bone 1 gets [2,3), terminator 3.
        w.U32(0);
        w.U32(2);
        w.U32(3);
        // bone 0, key 0
        w.F32(0.0f);
        w.Vector3(0.0f, 1.0f, 0.0f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);
        // bone 0, key 1
        w.F32(1.06f);
        w.Vector3(0.0f, 1.0f, -1.43f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);
        // bone 1, key 0
        w.F32(0.0f);
        w.Vector3(0.0f, 0.4f, 0.0f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);
        return w;
    }

    cnahouse::util::Result<ClipLibrary> ReadFrom(Writer& writer)
    {
        System::IO::MemoryStream stream(
            writer.Bytes().data(), static_cast<int>(writer.Bytes().size()), false);
        return ChanimReader::Read(stream, "test.chanim");
    }

    TEST(ChanimReaderTests, AWellFormedFileReadsBackExactlyWhatWasWritten)
    {
        Writer w = MinimalFile();
        auto library = ReadFrom(w);
        ASSERT_TRUE(library.HasValue()) << library.Error().ToString();

        const auto& skeleton = library->GetSkeleton();
        ASSERT_EQ(skeleton.Count(), 2u);
        EXPECT_EQ(skeleton.boneNames[0], "hips");
        EXPECT_EQ(skeleton.boneNames[1], "spine");
        EXPECT_EQ(skeleton.parent[0], -1);
        EXPECT_EQ(skeleton.parent[1], 0);
        EXPECT_EQ(skeleton.rootBone, 0);
        EXPECT_FALSE(skeleton.IsBound()) << "nothing has been bound to a model yet";

        ASSERT_EQ(library->Clips().size(), 1u);
        const auto* clip = library->Find("walk_fwd");
        ASSERT_NE(clip, nullptr);
        EXPECT_FLOAT_EQ(clip->duration, 1.06f);
        EXPECT_TRUE(clip->loops);
        EXPECT_FLOAT_EQ(clip->strideLength, 1.43f);
        ASSERT_EQ(clip->footPlants.size(), 2u);
        EXPECT_FLOAT_EQ(clip->footPlants[1], 0.53f);
        EXPECT_EQ(clip->keys.size(), 3u);
        EXPECT_EQ(library->Find("does_not_exist"), nullptr);
    }

    TEST(ChanimReaderTests, TheBoneTrackPartitionIsUsableWithNoSpecialCaseForTheLastBone)
    {
        Writer w = MinimalFile();
        auto library = ReadFrom(w);
        ASSERT_TRUE(library.HasValue()) << library.Error().ToString();
        const auto* clip = library->Find("walk_fwd");
        ASSERT_NE(clip, nullptr);

        const auto [begin0, end0] = clip->TrackFor(0);
        const auto [begin1, end1] = clip->TrackFor(1);
        EXPECT_EQ(begin0, 0u);
        EXPECT_EQ(end0, 2u);
        EXPECT_EQ(begin1, 2u);
        EXPECT_EQ(end1, 3u) << "the terminator entry is what makes the last bone ordinary";

        // A bone past the end returns an EMPTY range rather than reading off the end.
        const auto [beginPast, endPast] = clip->TrackFor(99);
        EXPECT_EQ(beginPast, endPast);
    }

    TEST(ChanimReaderTests, TheKeyValuesSurviveTheRoundTripIncludingQuaternionOrder)
    {
        // `w` last. The w-first order some maths libraries use is invisible in a hex dump and
        // produces a plausible-looking wrong pose, so it is asserted rather than assumed.
        Writer w = MinimalFile();
        auto library = ReadFrom(w);
        ASSERT_TRUE(library.HasValue()) << library.Error().ToString();
        const auto* clip = library->Find("walk_fwd");
        ASSERT_NE(clip, nullptr);

        EXPECT_FLOAT_EQ(clip->keys[1].time, 1.06f);
        EXPECT_FLOAT_EQ(clip->keys[1].translation.Z, -1.43f);
        EXPECT_FLOAT_EQ(clip->keys[0].rotation.W, 1.0f) << "the identity quaternion is (0,0,0,1)";
        EXPECT_FLOAT_EQ(clip->keys[0].rotation.X, 0.0f);
        EXPECT_FLOAT_EQ(clip->keys[2].translation.Y, 0.4f);
    }

    TEST(ChanimReaderTests, TheWrongFileEntirelyIsRejectedByTheMagic)
    {
        Writer w;
        w.U32(0x474E5089u); // a PNG signature's first four bytes
        w.U32(0);
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("CHAN"), std::string::npos) << library.Error().ToString();
    }

    TEST(ChanimReaderTests, AnUnknownVersionNamesBothVersions)
    {
        Writer w = MinimalFile();
        w.Bytes()[4] = 2; // version
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
        EXPECT_NE(library.Error().Message().find("version 2"), std::string::npos)
            << library.Error().ToString();
    }

    TEST(ChanimReaderTests, AReservedHeaderFlagIsRejectedRatherThanIgnored)
    {
        // Ignoring it means silently dropping whatever a newer writer meant by it.
        Writer w = MinimalFile();
        w.Bytes()[8] = 0x04;
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
    }

    TEST(ChanimReaderTests, AZeroBoneSkeletonIsRejected)
    {
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(0); // boneCount
        w.U32(0);
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST(ChanimReaderTests, MoreThanTwoHundredAndFiftySixBonesIsRejected)
    {
        // `Byte4` blend indices cannot address more (`HOUSE-00074`). The limit is 256 and NOT 72:
        // `SkinnedEffect::MaxBones` limits one draw, not one skeleton.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(257);
        w.U32(0);
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
        EXPECT_NE(library.Error().Message().find("256"), std::string::npos) << library.Error().ToString();
    }

    TEST(ChanimReaderTests, AForwardParentReferenceIsRejected)
    {
        // Parents strictly precede children, which is what lets every consumer walk the hierarchy in
        // one forward pass with no visited set -- and makes a cycle unrepresentable.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(2);
        w.U32(0);
        w.Name("hips");
        w.I32(1); // forward reference
        w.Identity();
        w.Identity();
        w.Name("spine");
        w.I32(0);
        w.Identity();
        w.Identity();
        w.I32(-1);

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("hips"), std::string::npos)
            << "the offending joint must be named: " << library.Error().ToString();
    }

    TEST(ChanimReaderTests, ADuplicateJointNameIsRejected)
    {
        // The name list IS the binding; a duplicate would bind two joints to one bone.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(2);
        w.U32(0);
        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.Name("hips");
        w.I32(0);
        w.Identity();
        w.Identity();
        w.I32(-1);

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("hips"), std::string::npos);
    }

    TEST(ChanimReaderTests, ANonPositiveDurationIsRejectedAndSoIsANaN)
    {
        Writer w = MinimalFile();
        // The duration is the first float of the clip, which begins after the skeleton.
        // Rewriting the whole file is simpler and less brittle than computing that offset.
        Writer zero;
        zero.U32(ChanimReader::kMagic);
        zero.U32(ChanimReader::kVersion);
        zero.U32(0);
        zero.U32(1);
        zero.U32(1);
        zero.Name("hips");
        zero.I32(-1);
        zero.Identity();
        zero.Identity();
        zero.I32(0);
        zero.Name("broken");
        zero.F32(0.0f);
        auto library = ReadFrom(zero);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);

        // And a NaN, which is why the check is `!(x > 0)` rather than `x <= 0`: a NaN compares
        // false against everything and walks straight through a `<=`.
        Writer nan;
        nan.U32(ChanimReader::kMagic);
        nan.U32(ChanimReader::kVersion);
        nan.U32(0);
        nan.U32(1);
        nan.U32(1);
        nan.Name("hips");
        nan.I32(-1);
        nan.Identity();
        nan.Identity();
        nan.I32(0);
        nan.Name("broken");
        nan.U32(0x7FC00000u); // quiet NaN
        auto nanLibrary = ReadFrom(nan);
        ASSERT_FALSE(nanLibrary.HasValue());
        EXPECT_EQ(nanLibrary.Error().Code(), ErrorCode::InvalidData);
    }

    TEST(ChanimReaderTests, ABrokenBoneFirstKeyPartitionIsRejected)
    {
        // Every later index would read someone else's track, which does not fail -- it silently
        // animates the wrong bone.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(2);
        w.U32(1);
        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.Name("spine");
        w.I32(0);
        w.Identity();
        w.Identity();
        w.I32(0);
        w.Name("clip");
        w.F32(1.0f);
        w.U32(0);
        w.F32(0.0f);
        w.U32(0);
        w.U32(2); // keyCount
        w.U32(0);
        w.U32(2);
        w.U32(1); // DECREASING: not a partition

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("partition"), std::string::npos)
            << library.Error().ToString();
    }

    TEST(ChanimReaderTests, KeyTimesOutOfOrderWithinOneBoneAreRejected)
    {
        // The sampler binary-searches, so unsorted input does not fail: it returns the wrong pose.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(1);
        w.U32(1);
        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.I32(0);
        w.Name("clip");
        w.F32(1.0f);
        w.U32(0);
        w.F32(0.0f);
        w.U32(0);
        w.U32(2);
        w.U32(0);
        w.U32(2);
        w.F32(0.8f); // later key first
        w.Vector3(0.0f, 0.0f, 0.0f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);
        w.F32(0.2f);
        w.Vector3(0.0f, 0.0f, 0.0f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("out of order"), std::string::npos)
            << library.Error().ToString();
    }

    TEST(ChanimReaderTests, AKeyTimeBeyondTheDurationIsRejected)
    {
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(1);
        w.U32(1);
        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.I32(0);
        w.Name("clip");
        w.F32(1.0f);
        w.U32(0);
        w.F32(0.0f);
        w.U32(0);
        w.U32(1);
        w.U32(0);
        w.U32(1);
        w.F32(2.5f); // past the end
        w.Vector3(0.0f, 0.0f, 0.0f);
        w.UnitQuaternion();
        w.Vector3(1.0f, 1.0f, 1.0f);

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST(ChanimReaderTests, AFootPlantOutsideTheClipIsRejected)
    {
        // It silently breaks §48's stride-length correction, which is the whole point of the marker.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(1);
        w.U32(1);
        w.Name("hips");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.I32(0);
        w.Name("clip");
        w.F32(1.0f);
        w.U32(0);
        w.F32(0.0f);
        w.U32(1);
        w.F32(1.5f);

        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST(ChanimReaderTests, ATruncatedFileIsAnErrorAndNotAShortSkeleton)
    {
        // The one failure the field-by-field checks cannot see: `BinaryReader` throws at the end of
        // the stream, and that has to arrive as an error rather than as a skeleton quietly missing
        // its last joints.
        Writer w = MinimalFile();
        w.Bytes().resize(w.Bytes().size() / 2);
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("ended early"), std::string::npos)
            << library.Error().ToString();
    }

    TEST(ChanimReaderTests, EveryErrorNamesTheFile)
    {
        // The context is what makes a message actionable when forty characters load at once.
        Writer w;
        w.U32(0xDEADBEEFu);
        auto library = ReadFrom(w);
        ASSERT_FALSE(library.HasValue());
        EXPECT_EQ(library.Error().Context(), "test.chanim");
        EXPECT_NE(library.Error().ToString().find("test.chanim"), std::string::npos);
    }

    TEST(ChanimReaderTests, AFileWithNoClipsIsLegal)
    {
        // A skeleton with no animation is a static prop that still needs a bind pose, and rejecting
        // it would mean inventing a second format for that case.
        Writer w;
        w.U32(ChanimReader::kMagic);
        w.U32(ChanimReader::kVersion);
        w.U32(0);
        w.U32(1);
        w.U32(0);
        w.Name("root");
        w.I32(-1);
        w.Identity();
        w.Identity();
        w.I32(-1);

        auto library = ReadFrom(w);
        ASSERT_TRUE(library.HasValue()) << library.Error().ToString();
        EXPECT_EQ(library->Clips().size(), 0u);
        EXPECT_EQ(library->GetSkeleton().Count(), 1u);
        EXPECT_EQ(library->GetSkeleton().rootBone, -1);
    }
} // namespace
