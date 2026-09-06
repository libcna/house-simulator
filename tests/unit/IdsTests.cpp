// SPDX-License-Identifier: MIT
//
// `HOUSE-00026`'s acceptance: (1) collisions are detected and fatal at load; (2) `Id("L0_KITCHEN")`
// is constexpr-comparable.
#include <gtest/gtest.h>

#include "cnahouse/util/Ids.hpp"

namespace
{
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;

    class IdsTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
        }

        void TearDown() override
        {
            IdRegistry::ResetForTesting();
        }
    };

    TEST_F(IdsTest, IdsAreComparableAtCompileTime)
    {
        // Asserted with `static_assert`, not `EXPECT_EQ`: the point is that this happens during
        // compilation, so an id comparison in a hot loop costs a single integer compare.
        static_assert(Id::Of("L0_KITCHEN") == Id::Of("L0_KITCHEN"));
        static_assert(Id::Of("L0_KITCHEN") != Id::Of("L0_HALL"));
        static_assert(Id::Of("L0_KITCHEN").IsValid());
        static_assert(!Id{}.IsValid());
        SUCCEED();
    }

    TEST_F(IdsTest, TheHashIsStableAcrossRuns)
    {
        // A literal, not a recomputation: an id reaches a save file and a content manifest, so a change
        // to the hash function is a breaking format change and this test is what makes that visible
        // rather than mysterious.
        EXPECT_EQ(Id::Of("L0_KITCHEN").Value(), 0x31EA1732u)
            << "the FNV-1a value of this name changed; that is a save-format break, not a refactor";
    }

    TEST_F(IdsTest, ZeroIsReservedForNoId)
    {
        // FNV-1a can produce 0, and 0 means "no id" everywhere in this project. The nudge is why.
        EXPECT_TRUE(Id::Of("").IsValid()) << "even the empty name must not collide with 'no id'";
        EXPECT_NE(Id::Of("").Value(), 0u);
    }

    TEST_F(IdsTest, InterningRegistersTheNameForDiagnostics)
    {
        const Id id = cnahouse::util::Intern("L0_KITCHEN");
        EXPECT_EQ(IdRegistry::NameOf(id), "L0_KITCHEN")
            << "a log line naming a bare hash is a log line nobody can act on";
        EXPECT_EQ(IdRegistry::Size(), 1u);
    }

    TEST_F(IdsTest, InterningTheSameNameTwiceIsNotAConflict)
    {
        (void)cnahouse::util::Intern("L0_KITCHEN");
        (void)cnahouse::util::Intern("L0_KITCHEN");
        EXPECT_FALSE(IdRegistry::HadConflict());
        EXPECT_EQ(IdRegistry::Size(), 1u);
    }

    TEST_F(IdsTest, ACollisionIsDetectedAndBothNamesAreReported)
    {
        // A REAL FNV-1a collision, not a manufactured one: "n512789" and "n749192" both hash to
        // 0xEB03B14B. Found by searching once, offline, and pinned here -- so the test exercises the
        // genuine detection path, runs in microseconds, and does not depend on a search budget.
        //
        // This is the case `HOUSE-00026` calls fatal at load. Two rooms sharing an id would silently
        // become one room, which would look like world-data corruption and be nearly impossible to
        // trace back to a hash.
        static constexpr std::string_view kFirst = "n512789";
        static constexpr std::string_view kSecond = "n749192";
        static_assert(Id::Of(kFirst) == Id::Of(kSecond), "the pinned collision pair no longer collides");

        const Id first = cnahouse::util::Intern(kFirst);
        EXPECT_FALSE(IdRegistry::HadConflict()) << "the first name is not a conflict with itself";

        const Id second = cnahouse::util::Intern(kSecond);
        EXPECT_EQ(first, second) << "by construction these share a hash";
        EXPECT_TRUE(IdRegistry::HadConflict()) << "and the registry must say so, not swallow it";
        EXPECT_EQ(IdRegistry::ConflictExisting(), kFirst);
        EXPECT_EQ(IdRegistry::ConflictIncoming(), kSecond)
            << "BOTH names are reported, because a message naming only one is unactionable";
    }

    TEST_F(IdsTest, ClearConflictResetsTheReport)
    {
        IdRegistry::ClearConflict();
        EXPECT_FALSE(IdRegistry::HadConflict());
        EXPECT_TRUE(IdRegistry::ConflictExisting().empty());
    }

    TEST_F(IdsTest, AnUninternedIdHasNoName)
    {
        EXPECT_TRUE(IdRegistry::NameOf(Id::Of("never-interned")).empty())
            << "the reverse map reports absence, it does not invent a name";
    }

    TEST_F(IdsTest, IdsWorkAsHashMapKeys)
    {
        std::unordered_map<Id, int> byId;
        byId[Id::Of("a")] = 1;
        byId[Id::Of("b")] = 2;
        EXPECT_EQ(byId[Id::Of("a")], 1);
        EXPECT_EQ(byId.size(), 2u);
    }

} // namespace
