// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <random>
#include <string>

#include "System/Environment.hpp"

namespace
{
    // Every integration process gets a private profile before any StorageDevice is opened.
    // Real Settings/Shift/Alt+Enter callbacks must never change the owner's preferences.
    class TestStorageEnvironment final : public ::testing::Environment
    {
    public:
        void SetUp() override
        {
#if defined(__linux__) && !defined(__ANDROID__)
            previous_ = System::Environment::GetEnvironmentVariable("XDG_DATA_HOME");
            root_ = std::filesystem::temp_directory_path() /
                    ("cnahouse-integration-" + std::to_string(std::random_device{}()));
            ASSERT_TRUE(std::filesystem::create_directory(root_));
            ownsRoot_ = true;
            System::Environment::SetEnvironmentVariable("XDG_DATA_HOME", root_.string());
#endif
        }

        void TearDown() override
        {
#if defined(__linux__) && !defined(__ANDROID__)
            System::Environment::SetEnvironmentVariable("XDG_DATA_HOME", previous_);
            if (ownsRoot_)
            {
                // Only the uniquely created, process-owned temporary profile, never user data.
                std::filesystem::remove_all(root_);
            }
#endif
        }

    private:
        std::optional<std::string> previous_;
        std::filesystem::path root_;
        bool ownsRoot_ = false;
    };

    [[maybe_unused]] const ::testing::Environment* const storageEnvironment =
        ::testing::AddGlobalTestEnvironment(new TestStorageEnvironment());
} // namespace
