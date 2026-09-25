// SPDX-License-Identifier: MIT
#include "cnahouse/weather/CoverageMask.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <memory>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::weather
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;

        util::Error Bad(ErrorCode code, std::string message, std::string_view name)
        {
            return Err(code, std::move(message), std::string(name));
        }

        util::Result<std::string> ReadName(System::IO::BinaryReader& reader, std::string_view file)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length > CoverageMask::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("worldHash claims {} bytes, above the {} limit",
                                       length,
                                       CoverageMask::kMaxNameBytes),
                           file);
            }
            const std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<int>(length));
            if (bytes.size() != length)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("worldHash was cut short: {} of {} bytes", bytes.size(), length),
                           file);
            }
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
    } // namespace

    util::Result<CoverageMask> CoverageMask::Read(System::IO::Stream& stream, std::string_view name)
    {
        try
        {
            System::IO::BinaryReader reader(&stream, true);
            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                return Bad(ErrorCode::InvalidData, std::format("magic is 0x{:08X}, not CCOV", magic), name);
            }
            const std::uint32_t version = reader.ReadUInt32();
            if (version != kVersion)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format("version {} is not supported; this build reads {}", version, kVersion),
                           name);
            }
            const std::uint32_t flags = reader.ReadUInt32();
            if (flags != 0u)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format("reserved header flags 0x{:08X} are set", flags),
                           name);
            }

            CoverageMask out;
            auto worldHash = ReadName(reader, name);
            if (!worldHash)
            {
                return worldHash.Error();
            }
            out.worldHash_ = std::move(worldHash.Value());
            out.originX_ = reader.ReadSingle();
            out.originZ_ = reader.ReadSingle();
            out.cellSize_ = reader.ReadSingle();
            out.ground_ = reader.ReadSingle();
            out.width_ = reader.ReadUInt32();
            out.height_ = reader.ReadUInt32();
            if (!std::isfinite(out.originX_) || !std::isfinite(out.originZ_) || !std::isfinite(out.ground_) ||
                out.cellSize_ != kCellSizeMetres)
            {
                return Bad(ErrorCode::InvalidData,
                           "the origin and ground must be finite and the grid cell must be 0.5 m",
                           name);
            }
            if (out.width_ == 0u || out.height_ == 0u || out.width_ > kMaxSamples / out.height_ ||
                out.width_ * out.height_ > kMaxSamples)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("grid dimensions {}x{} are empty or exceed {} samples",
                                       out.width_,
                                       out.height_,
                                       kMaxSamples),
                           name);
            }

            const std::size_t count = static_cast<std::size_t>(out.width_) * out.height_;
            out.heights_.reserve(count);
            for (std::size_t index = 0u; index < count; ++index)
            {
                const float height = reader.ReadSingle();
                if (std::isnan(height) || height == -std::numeric_limits<float>::infinity() ||
                    (std::isfinite(height) && !(height > out.ground_)))
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("sample {} is neither +infinity nor above ground", index),
                               name);
                }
                out.heights_.push_back(height);
            }
            if (!reader.ReadBytes(1).empty())
            {
                return Bad(ErrorCode::InvalidData, "there are bytes after the final sample", name);
            }
            return out;
        }
        catch (const std::exception& e)
        {
            return Bad(ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<CoverageMask> CoverageMask::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
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

    float CoverageMask::HeightAt(float x, float z) const noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(z) || heights_.empty())
        {
            return std::numeric_limits<float>::infinity();
        }
        const float column = (x - originX_) / cellSize_;
        const float row = (z - originZ_) / cellSize_;
        if (column < 0.0F || row < 0.0F)
        {
            return std::numeric_limits<float>::infinity();
        }
        const auto ix = static_cast<std::uint32_t>(std::floor(column));
        const auto iz = static_cast<std::uint32_t>(std::floor(row));
        if (ix >= width_ || iz >= height_)
        {
            return std::numeric_limits<float>::infinity();
        }
        return heights_[static_cast<std::size_t>(iz) * width_ + ix];
    }

    bool CoverageMask::IsExposed(const Microsoft::Xna::Framework::Vector3& position) const noexcept
    {
        const float cover = HeightAt(position.X, position.Z);
        return !std::isfinite(cover) || position.Y > cover;
    }

    bool CoverageMask::TeleportSheltered(Microsoft::Xna::Framework::Vector3& position,
                                         float volumeTop) const noexcept
    {
        const float cover = HeightAt(position.X, position.Z);
        if (!std::isfinite(cover) || position.Y > cover || !std::isfinite(volumeTop))
        {
            return false;
        }
        position.Y = volumeTop;
        return true;
    }

    std::size_t CoverageMask::CoveredSampleCount() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            heights_.begin(), heights_.end(), [](float height) { return std::isfinite(height); }));
    }
} // namespace cnahouse::weather
