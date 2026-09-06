// SPDX-License-Identifier: MIT
#include "cnahouse/debug/Screenshot.hpp"

#include <chrono>
#include <format>

#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::debug
{

    util::Result<void> Screenshot::Save(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                        Microsoft::Xna::Framework::Graphics::RenderTarget2D& source,
                                        std::string_view path)
    {
        const int width = source.getWidthProperty();
        const int height = source.getHeightProperty();
        if (width <= 0 || height <= 0)
        {
            return util::Err(util::ErrorCode::InvalidArgument,
                             std::format("the source is {}x{}, which has no pixels", width, height),
                             std::string(path));
        }

        try
        {
            // MEASURED: `SaveAsPng` on a render target throws *"no CPU-side pixel data available"* --
            // its pixels are on the GPU and the const save path has no shadow copy to write. So the
            // frame is read back and re-uploaded through the two calls the phase-1 probes proved
            // (`HOUSE-00083` read a 2048² target back bit-exactly; `HOUSE-00078` uploaded with
            // `SetData`). Two copies of one frame, once, at the moment a file was asked for.
            std::vector<Microsoft::Xna::Framework::Color> pixels(static_cast<std::size_t>(width) *
                                                                 static_cast<std::size_t>(height));
            source.GetData(pixels.data(), static_cast<int>(pixels.size()));

            Microsoft::Xna::Framework::Graphics::Texture2D staging(device, width, height);
            staging.SetData(pixels.data(), static_cast<int>(pixels.size()));

            // `SaveAsPng(const std::string&)` is CNAEXT; the stream overload is plain XNA 4.0, so the
            // file is opened here and handed to that. `System::IO` rather than `std::filesystem`, per
            // `cna-house.md` §8.3.
            System::IO::FileStream stream(
                std::string(path), System::IO::FileMode::Create, System::IO::FileAccess::Write);
            staging.SaveAsPng(&stream, width, height);
            stream.Flush();
        }
        catch (const std::exception& e)
        {
            // The XNA content and IO boundary throws; every failure this project reports is a
            // `Result`, so the conversion happens here rather than escaping to the caller
            // (`docs/conventions.md` §5.2).
            return util::Err(util::ErrorCode::IoFailure, e.what(), std::string(path));
        }

        util::Log::Info(util::LogCat::Debug, "screenshot written: {} ({}x{})", path, width, height);
        return {};
    }

    std::string Screenshot::TimestampedName(std::string_view directory, std::string_view prefix)
    {
        // Seconds resolution is not enough: pressing the key twice in a second is exactly what someone
        // does when trying to catch a transient, so the millisecond is part of the name.
        const auto now = std::chrono::system_clock::now();
        const auto epoch = now.time_since_epoch();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch - seconds);

        const std::time_t stamp = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &stamp);
#else
        localtime_r(&stamp, &local);
#endif

        const std::string separator = directory.empty() || directory.back() == '/' ? "" : "/";
        return std::format("{}{}{}-{:04}{:02}{:02}-{:02}{:02}{:02}-{:03}.png",
                           directory,
                           separator,
                           prefix,
                           local.tm_year + 1900,
                           local.tm_mon + 1,
                           local.tm_mday,
                           local.tm_hour,
                           local.tm_min,
                           local.tm_sec,
                           static_cast<int>(millis.count()));
    }

} // namespace cnahouse::debug
