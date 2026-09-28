// SPDX-License-Identifier: MIT
#include "cnahouse/audio/SkyExposure.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::audio
{
    util::Result<SkyExposure>
    SkyExposure::Read(System::IO::Stream& stream, std::string_view name, std::string_view worldHash)
    {
        try
        {
            System::IO::BinaryReader reader(&stream, true);
            const auto text = [&reader]()
            {
                const auto size = reader.ReadUInt16();
                if (size == 0U || size > 1024U)
                {
                    throw std::runtime_error("invalid CSKY string length");
                }
                const auto bytes = reader.ReadBytes(size);
                if (bytes.size() != size)
                {
                    throw std::runtime_error("truncated CSKY string");
                }
                return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            };
            const auto fraction = [&reader]()
            {
                const float value = reader.ReadSingle();
                if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
                {
                    throw std::runtime_error("CSKY exposure is not a finite fraction");
                }
                return value;
            };
            if (reader.ReadUInt32() != 0x594B5343U || reader.ReadUInt32() != 2U || reader.ReadUInt32() != 0U)
            {
                throw std::runtime_error("unsupported CSKY header");
            }
            if (text() != worldHash)
            {
                throw std::runtime_error("CSKY world hash does not match chunks");
            }
            const auto rays = reader.ReadUInt32();
            if (rays == 0U || rays > 1048576U || reader.ReadUInt32() != 8U)
            {
                throw std::runtime_error("invalid CSKY ray/orientation count");
            }
            for (const auto orientation : std::array{"N", "NE", "E", "SE", "S", "SW", "W", "NW"})
            {
                if (text() != orientation)
                {
                    throw std::runtime_error("invalid CSKY orientation order");
                }
            }
            const auto count = reader.ReadUInt32();
            if (count == 0U || count > 4096U)
            {
                throw std::runtime_error("invalid CSKY cell count");
            }
            SkyExposure result;
            for (std::uint32_t index = 0U; index < count; ++index)
            {
                const auto id = util::Intern(text());
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (!std::isfinite(reader.ReadSingle()))
                    {
                        throw std::runtime_error("invalid CSKY listener origin");
                    }
                }
                const auto samples = reader.ReadUInt32();
                // Writer MAX_SAMPLES is a spacing target, not a wire cap. Centre + per-box
                // grids include 22 points in the authored rear yard; this is metadata only.
                if (samples == 0U || samples > 4096U)
                {
                    throw std::runtime_error("invalid CSKY sample count");
                }
                if (!result.cells_.emplace(id, fraction()).second)
                {
                    throw std::runtime_error("duplicate CSKY cell");
                }
                for (int direction = 0; direction < 8; ++direction)
                {
                    (void)fraction();
                }
            }
            if (!reader.ReadBytes(1).empty())
            {
                throw std::runtime_error("trailing CSKY bytes");
            }
            return result;
        }
        catch (const std::exception& error)
        {
            return util::Err(util::ErrorCode::InvalidData, error.what(), std::string(name));
        }
    }

    util::Result<SkyExposure> SkyExposure::ReadFromTitle(std::string_view path, std::string_view worldHash)
    {
        try
        {
            auto stream = Microsoft::Xna::Framework::TitleContainer::OpenStream(std::string(path));
            if (stream == nullptr)
            {
                return util::Err(util::ErrorCode::NotFound, "missing CSKY", std::string(path));
            }
            return Read(*stream, path, worldHash);
        }
        catch (const std::exception& error)
        {
            return util::Err(util::ErrorCode::NotFound, error.what(), std::string(path));
        }
    }

    float SkyExposure::At(util::Id cell) const noexcept
    {
        const auto found = cells_.find(cell);
        return found == cells_.end() ? 0.0F : found->second;
    }
} // namespace cnahouse::audio
