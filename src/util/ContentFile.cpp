// SPDX-License-Identifier: MIT
#include "cnahouse/util/ContentFile.hpp"

#include <exception>
#include <memory>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"

#include "System/IO/File.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::util
{
    namespace
    {
        [[nodiscard]] std::unique_ptr<System::IO::Stream> OpenTitle(std::string_view path)
        {
            return Microsoft::Xna::Framework::TitleContainer::OpenStream(std::string(path));
        }
    } // namespace

    bool IsTitlePath(std::string_view path) noexcept
    {
        const bool rooted = !path.empty() && (path.front() == '/' || path.front() == '\\');
        const bool drive = path.size() >= 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
        return !rooted && !drive;
    }

    Result<std::vector<std::uint8_t>> ReadContentBytes(std::string_view path)
    {
        try
        {
            if (!IsTitlePath(path))
            {
                const auto bytes = System::IO::File::ReadAllBytes(std::string(path));
                return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
            }
            const std::unique_ptr<System::IO::Stream> stream = OpenTitle(path);
            if (stream == nullptr)
            {
                return Err(ErrorCode::NotFound, "the title file could not be opened", std::string(path));
            }
            std::vector<std::uint8_t> bytes;
            std::uint8_t block[65536];
            for (;;)
            {
                const int read = stream->Read(block, 0, static_cast<int>(sizeof(block)));
                if (read <= 0)
                {
                    break;
                }
                bytes.insert(bytes.end(), block, block + read);
            }
            return bytes;
        }
        catch (const std::exception& e)
        {
            return Err(ErrorCode::IoFailure, e.what(), std::string(path));
        }
    }

    Result<std::string> ReadContentText(std::string_view path)
    {
        auto bytes = ReadContentBytes(path);
        if (!bytes)
        {
            return bytes.Error();
        }
        return std::string(bytes->begin(), bytes->end());
    }

    bool ContentExists(std::string_view path)
    {
        if (!IsTitlePath(path))
        {
            return System::IO::File::Exists(std::string(path));
        }
        try
        {
            return OpenTitle(path) != nullptr;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
} // namespace cnahouse::util
