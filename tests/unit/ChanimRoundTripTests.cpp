// SPDX-License-Identifier: MIT
//
// `HOUSE-00223`. The one check neither side can make alone: a `.chanim` written by
// `tools/assets/anim_extract.py` is read by `src/animation/ChanimReader.cpp`, and what comes back
// out is what went in.
//
// `ChanimReaderTests` builds its bytes by hand, field at a time, because when it was written the
// writer did not exist. That makes it an excellent test of the READER and no test at all of the
// two agreeing: a writer that emitted the bind and inverse-bind matrices in the wrong order, or
// `w`-first quaternions, or a `boneFirstKey` off by one, would pass every test in this repository
// and fail in the game. This is the test that closes that.
//
// The fixture is generated at BUILD time into the build tree (`tests/CMakeLists.txt`) rather than
// committed, for the same reason no compiled content is committed: a checked-in binary produced by
// a tool in the same repository can go stale against the tool without anyone noticing.
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/animation/ChanimReader.hpp"

namespace
{
    using cnahouse::anim::ChanimReader;
    using cnahouse::anim::ClipLibrary;

    std::string FixturePath()
    {
        return std::string(CNAHOUSE_TEST_ANIM_FIXTURE);
    }

    /// The facts `anim_extract.py`'s `_skinned_fixture` authors. Written out here rather than read
    /// from the tool's own JSON report, because a test that asked the writer what it wrote and then
    /// checked the reader agreed would pass with both of them wrong in the same way.
    constexpr int kBoneCount = 5;
    constexpr const char* kJointNames[kBoneCount] = {
        "Hips", "LeftUpLeg", "LeftFoot", "RightUpLeg", "RightFoot"};
    constexpr float kStride = 1.4f;

    /// The hips' rotation at the end of `walk_fwd`: 40 degrees about the normalised axis (1, 2, 3).
    /// Four DIFFERENT components on purpose -- an identity quaternion cannot show a `w`-first write
    /// at either end of the round trip, and an injected `w`-first writer passed every test in this
    /// repository until the fixture stopped using one.
    constexpr float kFinalRotation[4] = {0.0914092f, 0.1828184f, 0.2742276f, 0.9396926f};

    ClipLibrary Load()
    {
        System::IO::FileStream stream(
            FixturePath(), System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto library = ChanimReader::Read(stream, "character.chanim");
        EXPECT_TRUE(library) << library.Error().ToString();
        return library ? std::move(*library) : ClipLibrary{};
    }

    class ChanimRoundTrip : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            System::IO::FileStream probe(
                FixturePath(), System::IO::FileMode::Open, System::IO::FileAccess::Read);
            (void)probe;
        }
    };

    TEST(ChanimRoundTrip, TheReaderAcceptsWhatTheWriterWrote)
    {
        System::IO::FileStream stream(
            FixturePath(), System::IO::FileMode::Open, System::IO::FileAccess::Read);
        const auto library = ChanimReader::Read(stream, "character.chanim");
        ASSERT_TRUE(library) << "anim_extract.py wrote a file ChanimReader refuses: "
                             << library.Error().ToString();
    }

    TEST(ChanimRoundTrip, TheJointListSurvivesInBlendIndexOrder)
    {
        const ClipLibrary library = Load();
        const auto& skeleton = library.GetSkeleton();
        ASSERT_EQ(skeleton.boneNames.size(), static_cast<std::size_t>(kBoneCount));

        // `HOUSE-00074`: this list IS the binding. Order is not a detail here -- slot *i* is blend
        // index *i*, and a writer that sorted the joints alphabetically would produce a file that
        // loads perfectly and deforms the character into a knot.
        for (int index = 0; index < kBoneCount; ++index)
        {
            EXPECT_EQ(skeleton.boneNames[static_cast<std::size_t>(index)], kJointNames[index])
                << "joint slot " << index;
        }
        EXPECT_EQ(skeleton.parent[0], -1) << "Hips is the root";
        EXPECT_EQ(skeleton.parent[1], 0) << "LeftUpLeg's parent is Hips";
        EXPECT_EQ(skeleton.parent[2], 1) << "LeftFoot's parent is LeftUpLeg";
        EXPECT_EQ(skeleton.rootBone, 0);
    }

    TEST(ChanimRoundTrip, MatricesArriveInXnaOrderWithTranslationInTheFourthRow)
    {
        // The claim `anim_extract.py`'s docstring makes: glTF's column-major array and XNA's
        // row-major array are the same sixteen floats, because XNA's matrix is the transpose. If
        // that is wrong the translation lands in the fourth COLUMN instead, every bind pose is
        // transposed, and the character explodes at the first draw.
        const ClipLibrary library = Load();
        const auto& skeleton = library.GetSkeleton();

        // Hips sits 0.95 m up. Its local bind pose is a pure translation.
        const auto& hips = skeleton.bindPose[0];
        EXPECT_NEAR(hips.M41, 0.0f, 1e-5f);
        EXPECT_NEAR(hips.M42, 0.95f, 1e-5f);
        EXPECT_NEAR(hips.M43, 0.0f, 1e-5f);
        EXPECT_NEAR(hips.M44, 1.0f, 1e-5f);
        // ...and the rotation block is the identity, not a transposed anything.
        EXPECT_NEAR(hips.M11, 1.0f, 1e-5f);
        EXPECT_NEAR(hips.M12, 0.0f, 1e-5f);
        EXPECT_NEAR(hips.M14, 0.0f, 1e-5f) << "the fourth COLUMN must be (0, 0, 0, 1)";
        EXPECT_NEAR(hips.M24, 0.0f, 1e-5f);
        EXPECT_NEAR(hips.M34, 0.0f, 1e-5f);

        // The inverse bind pose is the inverse of the joint's WORLD transform, so Hips' is a
        // translation by -0.95, and LeftUpLeg's is by -(0.95 - 0.45) = -0.5 in Y.
        EXPECT_NEAR(skeleton.inverseBindPose[0].M42, -0.95f, 1e-5f);
        EXPECT_NEAR(skeleton.inverseBindPose[1].M42, -0.5f, 1e-5f);
        EXPECT_NEAR(skeleton.inverseBindPose[1].M41, -0.1f, 1e-5f);
    }

    TEST(ChanimRoundTrip, TheClipsCarryTheirDurationStrideAndFootPlants)
    {
        const ClipLibrary library = Load();
        ASSERT_EQ(library.Clips().size(), 2u);

        const auto* walk = library.Find("walk_fwd");
        ASSERT_NE(walk, nullptr);
        EXPECT_NEAR(walk->duration, 1.0f, 1e-5f);
        // Measured by `measure_stride.py` and carried here by `anim_extract.py` rather than
        // measured a second time -- §47.4's rate matching is this one number.
        EXPECT_NEAR(walk->strideLength, kStride, kStride * 0.05f) << "the stride did not survive the sidecar";
        EXPECT_EQ(walk->footPlants.size(), 2u);
        for (const float plant : walk->footPlants)
        {
            EXPECT_GE(plant, 0.0f);
            EXPECT_LE(plant, walk->duration);
        }

        const auto* idle = library.Find("idle");
        ASSERT_NE(idle, nullptr);
        EXPECT_NEAR(idle->duration, 4.0f, 1e-5f);
        EXPECT_EQ(idle->strideLength, 0.0f) << "an idle is not locomotion (§47.4)";
        EXPECT_TRUE(idle->footPlants.empty());
    }

    TEST(ChanimRoundTrip, TheBoneFirstKeyPartitionIsUsableAsATrackIndex)
    {
        // The partition is the whole reason a key carries no bone index. If it is off by one, every
        // bone samples its neighbour's track -- which loads without error and animates the elbow
        // with the knee's curve.
        const ClipLibrary library = Load();
        const auto* walk = library.Find("walk_fwd");
        ASSERT_NE(walk, nullptr);
        ASSERT_EQ(walk->boneFirstKey.size(), static_cast<std::size_t>(kBoneCount) + 1u);
        EXPECT_EQ(walk->boneFirstKey.front(), 0u);
        EXPECT_EQ(walk->boneFirstKey.back(), walk->keys.size());

        for (int bone = 0; bone < kBoneCount; ++bone)
        {
            const std::uint32_t first = walk->boneFirstKey[static_cast<std::size_t>(bone)];
            const std::uint32_t last = walk->boneFirstKey[static_cast<std::size_t>(bone) + 1u];
            EXPECT_LE(first, last) << "joint " << bone << "'s range runs backwards";
            for (std::uint32_t index = first + 1u; index < last; ++index)
            {
                EXPECT_LE(walk->keys[index - 1u].time, walk->keys[index].time)
                    << "joint " << bone << " has keys out of order; the sampler binary-searches";
            }
        }

        // RightUpLeg is animated to exactly its bind pose, so its range is EMPTY -- the case
        // `docs/anim-format.md` §3.3 makes legal and which a writer that always emitted two keys
        // would never produce.
        EXPECT_EQ(walk->boneFirstKey[3], walk->boneFirstKey[4])
            << "RightUpLeg animates to its bind pose and should carry no keys at all";
        EXPECT_LT(walk->boneFirstKey[0], walk->boneFirstKey[1]) << "Hips moves";
        EXPECT_LT(walk->boneFirstKey[4], walk->boneFirstKey[5]) << "RightFoot moves";
    }

    TEST(ChanimRoundTrip, AQuaternionArrivesXYZWWithWLast)
    {
        // §2 of `docs/anim-format.md`: `Quaternion` is x y z **w**, matching XNA's constructor and
        // glTF, not the w-first order some maths libraries use. Getting this wrong produces a
        // character rotated by an arbitrary amount about an arbitrary axis, with no error anywhere.
        const ClipLibrary library = Load();
        const auto* walk = library.Find("walk_fwd");
        ASSERT_NE(walk, nullptr);
        ASSERT_GT(walk->boneFirstKey[1], walk->boneFirstKey[0]);

        const auto& last = walk->keys[walk->boneFirstKey[1] - 1u];
        EXPECT_NEAR(last.rotation.X, kFinalRotation[0], 1e-4f);
        EXPECT_NEAR(last.rotation.Y, kFinalRotation[1], 1e-4f);
        EXPECT_NEAR(last.rotation.Z, kFinalRotation[2], 1e-4f);
        EXPECT_NEAR(last.rotation.W, kFinalRotation[3], 1e-4f);
    }

    TEST(ChanimRoundTrip, EveryKeyTimeIsInsideItsClip)
    {
        const ClipLibrary library = Load();
        for (const auto& clip : library.Clips())
        {
            for (const auto& key : clip.keys)
            {
                EXPECT_GE(key.time, 0.0f) << clip.name;
                EXPECT_LE(key.time, clip.duration) << clip.name;
            }
        }
    }

} // namespace
