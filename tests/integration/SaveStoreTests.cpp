// SPDX-License-Identifier: MIT
//
// `HOUSE-00152`. An integration test, because `StorageDevice` touches the real filesystem and a
// stub would test the stub -- and because the behaviour under test IS the atomic sequence, which
// only exists on a real filesystem.
#include <gtest/gtest.h>

#include "cnahouse/persistence/DesktopSaveStore.hpp"

namespace
{
    using cnahouse::persistence::DesktopSaveStore;
    using cnahouse::util::ErrorCode;

    constexpr std::string_view kSave = "cnahouse-test-save.json";

    std::unique_ptr<DesktopSaveStore> OpenStore()
    {
        auto store = DesktopSaveStore::Open();
        EXPECT_TRUE(store) << (store ? "" : store.Error().ToString());
        return store ? std::move(store).Value() : nullptr;
    }

    class SaveStoreTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            store_ = OpenStore();
            ASSERT_NE(store_, nullptr);
            Cleanup();
        }

        void TearDown() override
        {
            if (store_ != nullptr)
            {
                Cleanup();
            }
        }

        void Cleanup()
        {
            (void)store_->Delete(kSave);
            (void)store_->Delete(DesktopSaveStore::BackupName(kSave));
            (void)store_->Delete(DesktopSaveStore::TempName(kSave));
        }

        std::unique_ptr<DesktopSaveStore> store_;
    };

    TEST_F(SaveStoreTest, TheLocationIsTheMeasuredOneNotTheAssumedOne)
    {
        // `HOUSE-00102` found `StorageDevice`'s `<app>` component is the literal string `game`, because
        // the only way to change it is `SetAppNameEXT`, which ADR-0001 forbids. The CONTAINER name is
        // what identifies this game, and it is plain XNA.
        const std::string location = store_->Location();
        EXPECT_NE(location.find("/game/"), std::string::npos) << location;
        EXPECT_TRUE(location.ends_with(DesktopSaveStore::kContainerName)) << location;
    }

    TEST_F(SaveStoreTest, WriteThenReadRoundTripsExactly)
    {
        constexpr std::string_view kPayload = R"({"schema":"cna-house/save/1","doors":{"D1":"open"}})";
        auto written = store_->Write(kSave, kPayload);
        ASSERT_TRUE(written) << written.Error().ToString();

        auto read = store_->Read(kSave);
        ASSERT_TRUE(read) << read.Error().ToString();
        EXPECT_EQ(*read, kPayload);
    }

    TEST_F(SaveStoreTest, TheSecondWriteLeavesTheFirstAsABackup)
    {
        // The backup exists precisely for the case where the current save fails its checksum, which is
        // why reading it is a first-class operation rather than a caller assembling a filename.
        ASSERT_TRUE(store_->Write(kSave, "first"));
        ASSERT_TRUE(store_->Write(kSave, "second"));

        auto current = store_->Read(kSave);
        ASSERT_TRUE(current);
        EXPECT_EQ(*current, "second");

        auto backup = store_->ReadBackup(kSave);
        ASSERT_TRUE(backup) << backup.Error().ToString();
        EXPECT_EQ(*backup, "first") << "one good file is always on disk";
    }

    TEST_F(SaveStoreTest, NoTemporaryFileSurvivesASuccessfulWrite)
    {
        // A leftover `.tmp` would accumulate one per save and would eventually be mistaken for a save.
        ASSERT_TRUE(store_->Write(kSave, "payload"));
        EXPECT_FALSE(store_->Exists(DesktopSaveStore::TempName(kSave)));
    }

    TEST_F(SaveStoreTest, ReadingAMissingSaveIsNotFoundRatherThanAThrow)
    {
        auto read = store_->Read("no-such-save.json");
        ASSERT_FALSE(read);
        EXPECT_EQ(read.Error().Code(), ErrorCode::NotFound);
    }

    TEST_F(SaveStoreTest, ReadingAMissingBackupIsNotFound)
    {
        // The first save of a new game has no backup, and that is normal rather than an error the
        // caller should be surprised by.
        ASSERT_TRUE(store_->Write(kSave, "only"));
        auto backup = store_->ReadBackup(kSave);
        ASSERT_FALSE(backup);
        EXPECT_EQ(backup.Error().Code(), ErrorCode::NotFound);
    }

    TEST_F(SaveStoreTest, ListShowsWhatWasWritten)
    {
        ASSERT_TRUE(store_->Write(kSave, "payload"));
        auto listed = store_->List();
        ASSERT_TRUE(listed) << listed.Error().ToString();
        bool found = false;
        for (const std::string& name : *listed)
        {
            found = found || name.find(kSave) != std::string::npos;
        }
        EXPECT_TRUE(found);
    }

    TEST_F(SaveStoreTest, DeleteRemovesItAndDeletingTwiceIsNotAnError)
    {
        ASSERT_TRUE(store_->Write(kSave, "payload"));
        ASSERT_TRUE(store_->Exists(kSave));
        ASSERT_TRUE(store_->Delete(kSave));
        EXPECT_FALSE(store_->Exists(kSave));
        // Idempotent, because "make sure this is gone" is a thing callers legitimately want to say.
        EXPECT_TRUE(store_->Delete(kSave));
    }

    TEST_F(SaveStoreTest, AnEmptyPayloadIsWrittenAndReadBackAsEmpty)
    {
        // Degenerate but reachable: a save of a house in exactly its canonical initial state is an
        // empty delta (ADR-0008), and it must not be indistinguishable from a missing file.
        ASSERT_TRUE(store_->Write(kSave, ""));
        EXPECT_TRUE(store_->Exists(kSave));
        auto read = store_->Read(kSave);
        ASSERT_TRUE(read);
        EXPECT_TRUE(read->empty());
    }

    TEST_F(SaveStoreTest, ALargePayloadSurvivesTheChunkedRead)
    {
        // The read loop is chunked at 8 KB; ADR-0008 sizes a heavily explored house at ~90 KB, so the
        // multi-chunk path is the normal one and not an edge case.
        std::string payload;
        payload.reserve(200000);
        for (int i = 0; i < 20000; ++i)
        {
            payload += "0123456789";
        }
        ASSERT_TRUE(store_->Write(kSave, payload));
        auto read = store_->Read(kSave);
        ASSERT_TRUE(read);
        EXPECT_EQ(read->size(), payload.size());
        EXPECT_EQ(*read, payload);
    }

} // namespace
