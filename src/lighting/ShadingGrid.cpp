// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/ShadingGrid.hpp"

#include <cmath>
#include <format>
#include <memory>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::lighting
{
    namespace
    {
        using Microsoft::Xna::Framework::Vector3;
        using util::Err;
        using util::ErrorCode;

        /// Every rejection goes through here, so every one of them names the file.
        util::Error Bad(ErrorCode code, std::string message, std::string_view name)
        {
            return Err(code, std::move(message), std::string(name));
        }

        /// `u16` byte length then UTF-8 — `docs/anim-format.md` §2's string, which every binary
        /// format in this project uses and which the Python writer emits.
        util::Result<std::string> ReadName(System::IO::BinaryReader& reader, std::string_view file)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length == 0u)
            {
                return Bad(ErrorCode::InvalidData, "a name is empty", file);
            }
            if (length > ShadingGrid::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name claims {} bytes, above the {} limit",
                                       length,
                                       ShadingGrid::kMaxNameBytes),
                           file);
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

        constexpr double kAltitudeSpanDeg = 90.0;
        constexpr double kAzimuthSpanDeg = 360.0;
        constexpr double kDegToRad = 0.017453292519943295769236907684886;

        /// @brief The irradiance-weighted mean of a window's grid over the hemisphere it faces.
        ///
        /// Each node is weighted by `cos(altitude)` — the grid is uniform in (altitude, azimuth)
        /// and so over-samples the zenith, and this is the solid angle each node stands for — and
        /// by the cosine of incidence on the pane, which is what a flat surface actually collects.
        /// Nodes behind the wall have both a zero weight and a zero value and drop out either way.
        [[nodiscard]] float ComputeSkyView(const WindowShading& window) noexcept
        {
            double weighted = 0.0;
            double total = 0.0;
            for (std::uint32_t a = 0; a < ShadingGrid::kAltitudeSteps; ++a)
            {
                const double altitude = static_cast<double>(a) * kAltitudeSpanDeg /
                                        static_cast<double>(ShadingGrid::kAltitudeSteps - 1);
                const double cosAltitude = std::cos(altitude * kDegToRad);
                for (std::uint32_t z = 0; z < ShadingGrid::kAzimuthSteps; ++z)
                {
                    const double azimuth = static_cast<double>(z) * kAzimuthSpanDeg /
                                           static_cast<double>(ShadingGrid::kAzimuthSteps);
                    // §10.1's axes, the same as `sun_direction`'s: north is -Z, east is +X.
                    const double dirX = cosAltitude * std::sin(azimuth * kDegToRad);
                    const double dirY = std::sin(altitude * kDegToRad);
                    const double dirZ = -cosAltitude * std::cos(azimuth * kDegToRad);
                    const double incidence = dirX * static_cast<double>(window.normal.X) +
                                             dirY * static_cast<double>(window.normal.Y) +
                                             dirZ * static_cast<double>(window.normal.Z);
                    if (incidence <= 0.0)
                    {
                        continue;
                    }
                    const double weight = cosAltitude * incidence;
                    total += weight;
                    weighted +=
                        weight * static_cast<double>(window.grid[a * ShadingGrid::kAzimuthSteps + z]) / 255.0;
                }
            }
            return total > 0.0 ? static_cast<float>(weighted / total) : 1.0F;
        }

    } // namespace

    util::Result<ShadingGrid> ShadingGrid::Read(System::IO::Stream& stream, std::string_view name)
    {
        ShadingGrid out;
        try
        {
            System::IO::BinaryReader reader(&stream, true);
            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                return Bad(ErrorCode::InvalidData, std::format("magic is 0x{:08X}, not CSHF", magic), name);
            }
            const std::uint32_t version = reader.ReadUInt32();
            if (version != kVersion)
            {
                return Bad(
                    ErrorCode::InvalidData, std::format("version {} is not {}", version, kVersion), name);
            }
            const std::uint32_t flags = reader.ReadUInt32();
            if (flags != 0u)
            {
                // `docs/shading-format.md`: *"A reader must reject any unknown bit."* Ignoring one
                // is how a format quietly forks.
                return Bad(ErrorCode::InvalidData,
                           std::format("flags 0x{:08X} has bits this reader does not know", flags),
                           name);
            }
            const std::uint32_t altitudes = reader.ReadUInt32();
            const std::uint32_t azimuths = reader.ReadUInt32();
            if (altitudes != kAltitudeSteps || azimuths != kAzimuthSteps)
            {
                // The node spacing is baked into `Factor`'s arithmetic, so a differently shaped
                // grid would be read successfully and interpolated wrongly.
                return Bad(ErrorCode::InvalidData,
                           std::format("the grid is {}x{}, not §22's {}x{}",
                                       altitudes,
                                       azimuths,
                                       kAltitudeSteps,
                                       kAzimuthSteps),
                           name);
            }
            out.samples_ = reader.ReadUInt32();
            const std::uint32_t windowCount = reader.ReadUInt32();
            if (windowCount > kMaxWindows)
            {
                return Bad(
                    ErrorCode::InvalidData,
                    std::format("the file claims {} windows, above the {} limit", windowCount, kMaxWindows),
                    name);
            }

            const std::size_t nodes = static_cast<std::size_t>(altitudes) * azimuths;
            out.windows_.reserve(windowCount);
            out.index_.reserve(windowCount);
            for (std::uint32_t index = 0; index < windowCount; ++index)
            {
                auto windowName = ReadName(reader, name);
                if (!windowName)
                {
                    return windowName.Error();
                }
                auto cellName = ReadName(reader, name);
                if (!cellName)
                {
                    return cellName.Error();
                }
                WindowShading shading;
                shading.window = util::IdRegistry::Intern(windowName.Value());
                shading.cell = util::IdRegistry::Intern(cellName.Value());
                const float x = reader.ReadSingle();
                const float y = reader.ReadSingle();
                const float z = reader.ReadSingle();
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("window '{}' has a non-finite normal", windowName.Value()),
                               name);
                }
                shading.normal = Vector3(x, y, z);
                shading.grid = reader.ReadBytes(static_cast<int>(nodes));
                if (shading.grid.size() != nodes)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("window '{}' has {} of {} grid bytes",
                                           windowName.Value(),
                                           shading.grid.size(),
                                           nodes),
                               name);
                }
                if (!out.index_.emplace(shading.window.Value(), out.windows_.size()).second)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("window '{}' appears twice", windowName.Value()),
                               name);
                }
                out.windows_.push_back(std::move(shading));
            }

            out.skyView_.reserve(out.windows_.size());
            for (const WindowShading& window : out.windows_)
            {
                out.skyView_.push_back(ComputeSkyView(window));
            }

            // Trailing bytes mean the writer and this reader disagree about the layout, and a
            // disagreement that is ignored here is one nobody finds until the format changes again.
            if (reader.ReadBytes(1).size() != 0)
            {
                return Bad(ErrorCode::InvalidData, "there are bytes after the last window", name);
            }
        }
        catch (const std::exception& e)
        {
            return Bad(ErrorCode::InvalidData, e.what(), name);
        }
        return out;
    }

    util::Result<ShadingGrid> ShadingGrid::ReadFromTitle(std::string_view contentPath)
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

    float ShadingGrid::SkyViewFactor(util::Id window) const noexcept
    {
        const auto found = index_.find(window.Value());
        return found == index_.end() ? 1.0F : skyView_[found->second];
    }

    bool ShadingGrid::Contains(util::Id window) const noexcept
    {
        return index_.find(window.Value()) != index_.end();
    }

    const WindowShading* ShadingGrid::Find(util::Id window) const noexcept
    {
        const auto found = index_.find(window.Value());
        return found == index_.end() ? nullptr : &windows_[found->second];
    }

    float ShadingGrid::Factor(util::Id window, double altitudeDeg, double azimuthDeg) const noexcept
    {
        const WindowShading* shading = Find(window);
        if (shading == nullptr || !std::isfinite(altitudeDeg) || !std::isfinite(azimuthDeg))
        {
            return 1.0F;
        }

        // Altitude CLAMPS. The grid's ends are measured nodes -- 0° and 90° -- so there is nothing
        // to extrapolate towards, and a sun below the horizon is `SkyExposure`'s business.
        const double altitudeStep = kAltitudeSpanDeg / static_cast<double>(kAltitudeSteps - 1);
        double altitudePosition = altitudeDeg / altitudeStep;
        altitudePosition = altitudePosition < 0.0 ? 0.0 : altitudePosition;
        const double altitudeTop = static_cast<double>(kAltitudeSteps - 1);
        altitudePosition = altitudePosition > altitudeTop ? altitudeTop : altitudePosition;
        const auto altitudeLow = static_cast<std::size_t>(altitudePosition);
        const std::size_t altitudeHigh = altitudeLow + 1 < kAltitudeSteps ? altitudeLow + 1 : altitudeLow;
        const double altitudeFraction = altitudePosition - static_cast<double>(altitudeLow);

        // Azimuth WRAPS. Node 23 is 345° and its neighbour is node 0 at 360°, which is the same
        // direction; a clamp here would put a seam due north that a shadow crosses twice a day.
        const double azimuthStep = kAzimuthSpanDeg / static_cast<double>(kAzimuthSteps);
        double wrapped = std::fmod(azimuthDeg, kAzimuthSpanDeg);
        wrapped = wrapped < 0.0 ? wrapped + kAzimuthSpanDeg : wrapped;
        const double azimuthPosition = wrapped / azimuthStep;
        const auto azimuthLow = static_cast<std::size_t>(azimuthPosition) % kAzimuthSteps;
        const std::size_t azimuthHigh = (azimuthLow + 1) % kAzimuthSteps;
        const double azimuthFraction = azimuthPosition - std::floor(azimuthPosition);

        const auto at = [&](std::size_t a, std::size_t z)
        { return static_cast<double>(shading->grid[a * kAzimuthSteps + z]) / 255.0; };
        const double lower = at(altitudeLow, azimuthLow) +
                             (at(altitudeLow, azimuthHigh) - at(altitudeLow, azimuthLow)) * azimuthFraction;
        const double upper = at(altitudeHigh, azimuthLow) +
                             (at(altitudeHigh, azimuthHigh) - at(altitudeHigh, azimuthLow)) * azimuthFraction;
        return static_cast<float>(lower + (upper - lower) * altitudeFraction);
    }

} // namespace cnahouse::lighting
