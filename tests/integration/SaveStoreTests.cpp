// SPDX-License-Identifier: MIT
//
// `HOUSE-00152`. An integration test, because `StorageDevice` touches the real filesystem and a
// stub would test the stub -- and because the behaviour under test IS the atomic sequence, which
// only exists on a real filesystem.
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/Settings.hpp"
#include "cnahouse/persistence/DesktopSaveStore.hpp"

namespace
{
    using cnahouse::persistence::DesktopSaveStore;
    using cnahouse::util::ErrorCode;

    /// A name unique to THIS PROCESS.
    ///
    /// Every test in this fixture deletes the save in `SetUp` and `TearDown`, and they all write
    /// into the user's real save directory -- so two `ctest -j2` processes running two of these
    /// tests at once delete each other's file mid-test. Measured: `ctest -L integration -j2` fails
    /// `TheSecondWriteLeavesTheFirstAsABackup` intermittently while the same test passes five
    /// times out of five on its own. CI runs the suite serially and never saw it.
    ///
    /// A fixed name was the bug; a random suffix per process is the fix. It is generated once, so
    /// every test in one process shares it and `Cleanup` still works.
    const std::string& SaveName()
    {
        static const std::string name = []
        {
            std::random_device source;
            return "cnahouse-test-save-" + std::to_string(source()) + ".json";
        }();
        return name;
    }

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
            (void)store_->Delete(SaveName());
            (void)store_->Delete(DesktopSaveStore::BackupName(SaveName()));
            (void)store_->Delete(DesktopSaveStore::TempName(SaveName()));
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
        auto written = store_->Write(SaveName(), kPayload);
        ASSERT_TRUE(written) << written.Error().ToString();

        auto read = store_->Read(SaveName());
        ASSERT_TRUE(read) << read.Error().ToString();
        EXPECT_EQ(*read, kPayload);
    }

    TEST_F(SaveStoreTest, AnUnreadableCurrentFileReportsSaveFailureRatherThanAborting)
    {
#if defined(__linux__) && !defined(__ANDROID__)
        ASSERT_TRUE(store_->Write(SaveName(), "preserve me"));
        std::filesystem::path path;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(store_->Location()))
        {
            if (entry.path().filename() == SaveName())
            {
                path = entry.path();
                break;
            }
        }
        ASSERT_FALSE(path.empty());
        const auto mode = std::filesystem::status(path).permissions();
        std::filesystem::permissions(path, std::filesystem::perms::none);
        const auto unreadable = store_->Read(SaveName());
        if (unreadable)
        {
            std::filesystem::permissions(path, mode);
            GTEST_SKIP() << "this process can read permission-denied files";
        }
        const auto failed = store_->Write(SaveName(), "do not replace it");
        std::filesystem::permissions(path, mode);
        ASSERT_FALSE(failed);
        const auto preserved = store_->Read(SaveName());
        ASSERT_TRUE(preserved);
        EXPECT_EQ(*preserved, "preserve me");
#else
        GTEST_SKIP() << "Linux file-permission regression";
#endif
    }

    TEST_F(SaveStoreTest, TheSecondWriteLeavesTheFirstAsABackup)
    {
        // The backup exists precisely for the case where the current save fails its checksum, which is
        // why reading it is a first-class operation rather than a caller assembling a filename.
        ASSERT_TRUE(store_->Write(SaveName(), "first"));
        ASSERT_TRUE(store_->Write(SaveName(), "second"));

        auto current = store_->Read(SaveName());
        ASSERT_TRUE(current);
        EXPECT_EQ(*current, "second");

        auto backup = store_->ReadBackup(SaveName());
        ASSERT_TRUE(backup) << backup.Error().ToString();
        EXPECT_EQ(*backup, "first") << "one good file is always on disk";
    }

    TEST_F(SaveStoreTest, NoTemporaryFileSurvivesASuccessfulWrite)
    {
        // A leftover `.tmp` would accumulate one per save and would eventually be mistaken for a save.
        ASSERT_TRUE(store_->Write(SaveName(), "payload"));
        EXPECT_FALSE(store_->Exists(DesktopSaveStore::TempName(SaveName())));
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
        ASSERT_TRUE(store_->Write(SaveName(), "only"));
        auto backup = store_->ReadBackup(SaveName());
        ASSERT_FALSE(backup);
        EXPECT_EQ(backup.Error().Code(), ErrorCode::NotFound);
    }

    TEST_F(SaveStoreTest, ListShowsWhatWasWritten)
    {
        ASSERT_TRUE(store_->Write(SaveName(), "payload"));
        auto listed = store_->List();
        ASSERT_TRUE(listed) << listed.Error().ToString();
        bool found = false;
        for (const std::string& name : *listed)
        {
            found = found || name.find(SaveName()) != std::string::npos;
        }
        EXPECT_TRUE(found);
    }

    TEST_F(SaveStoreTest, DeleteRemovesItAndDeletingTwiceIsNotAnError)
    {
        ASSERT_TRUE(store_->Write(SaveName(), "payload"));
        ASSERT_TRUE(store_->Exists(SaveName()));
        ASSERT_TRUE(store_->Delete(SaveName()));
        EXPECT_FALSE(store_->Exists(SaveName()));
        // Idempotent, because "make sure this is gone" is a thing callers legitimately want to say.
        EXPECT_TRUE(store_->Delete(SaveName()));
    }

    TEST_F(SaveStoreTest, AnEmptyPayloadIsWrittenAndReadBackAsEmpty)
    {
        // Degenerate but reachable: a save of a house in exactly its canonical initial state is an
        // empty delta (ADR-0008), and it must not be indistinguishable from a missing file.
        ASSERT_TRUE(store_->Write(SaveName(), ""));
        EXPECT_TRUE(store_->Exists(SaveName()));
        auto read = store_->Read(SaveName());
        ASSERT_TRUE(read);
        EXPECT_TRUE(read->empty());
    }

    TEST_F(SaveStoreTest, TwoHundredSettingsWritesRemainReadable)
    {
        // HOUSE-02598's stability run writes the real settings payload through the shipping
        // atomic store. Re-serialising and reading the final generation matters: counting calls
        // to a fake store would miss both backup churn and a truncated final file.
        constexpr int kWrites = 200;
        cnahouse::app::Settings settings = cnahouse::app::Settings::Defaults();
        for (int generation = 0; generation < kWrites; ++generation)
        {
            settings.invertY = (generation % 2) != 0;
            settings.mouseSensitivity = 0.5F + static_cast<float>(generation % 10) * 0.1F;
            const auto written = store_->Write(SaveName(), settings.ToJson());
            ASSERT_TRUE(written) << "settings generation " << generation << ": "
                                 << written.Error().ToString();
        }

        const auto text = store_->Read(SaveName());
        ASSERT_TRUE(text) << text.Error().ToString();
        const auto reread = cnahouse::app::Settings::FromJson(*text, SaveName());
        ASSERT_TRUE(reread) << reread.Error().ToString();
        EXPECT_EQ(reread->invertY, settings.invertY);
        EXPECT_FLOAT_EQ(reread->mouseSensitivity, settings.mouseSensitivity);
        std::printf("  stability settings writes: %d; final payload: %zu bytes\n", kWrites, text->size());
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
        ASSERT_TRUE(store_->Write(SaveName(), payload));
        auto read = store_->Read(SaveName());
        ASSERT_TRUE(read);
        EXPECT_EQ(read->size(), payload.size());
        EXPECT_EQ(*read, payload);
    }

} // namespace
