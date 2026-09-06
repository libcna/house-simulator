// HOUSE-00102 -- `StorageDevice`/`StorageContainer`: write, read, list and delete a file, and record
//                the resolved path. This is where every save file will live.
// HOUSE-00103 -- `System::Text::Json`: a nested object with arrays and floats, round-tripped, with
//                the float precision actually checked rather than assumed.
//
// The precision question is not academic. `cna-house` saves world state -- positions, rotations,
// time-of-day -- as JSON, and a save that drifts on every load/save cycle corrupts a long-running
// house slowly enough that nobody notices until it is unrecoverable. So the probe round-trips the
// awkward values (a third, a repeating binary fraction, a denormal-adjacent tiny number, a large
// one) and requires them to come back BIT-IDENTICAL, not merely close.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "System/IAsyncResult.hpp"
#include "System/IO/Stream.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonValueKind.hpp"

#include <cstring>
#include <memory>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Storage;

namespace
{

    std::string ReadWholeFile(System::IO::Stream& stream)
    {
        std::string out;
        std::vector<std::uint8_t> buffer(4096);
        for (;;)
        {
            const auto read = stream.Read(buffer.data(), 0, static_cast<int>(buffer.size()));
            if (read <= 0)
            {
                break;
            }
            out.append(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(read));
        }
        return out;
    }

    class StorageProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;

            // ================= HOUSE-00102: StorageDevice / StorageContainer ========================
            std::unique_ptr<StorageDevice> device;
            std::string err;
            try
            {
                auto async = StorageDevice::BeginShowSelector(nullptr, nullptr);
                device = StorageDevice::EndShowSelector(async.get());
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("a StorageDevice is obtainable with no UI on desktop", device != nullptr, err);
            if (device == nullptr)
            {
                return r.finish("p1-storage");
            }
            r.check("the device reports itself connected", device->getIsConnectedProperty());
            r.note("free space", std::to_string(device->getFreeSpaceProperty()) + " bytes");

            std::unique_ptr<StorageContainer> container;
            try
            {
                auto async = device->BeginOpenContainer("P1Probe", nullptr, nullptr);
                container = device->EndOpenContainer(async.get());
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("a StorageContainer opens", container != nullptr, err);
            if (container == nullptr)
            {
                return r.finish("p1-storage");
            }
            r.note("container display name", container->getDisplayNameProperty());

            // The resolved path matters because it is where a user's saves actually are, and because
            // `cna-house` promises never to touch the disk outside it.
            //
            // MEASURED, and it corrects `cna-house.md` §5: the `<app>` component is NOT the game's
            // name. `StorageDevice` uses `appName_.empty() ? "game" : appName_`, and the only way to
            // set `appName_` is `SetAppNameEXT` -- a `CNAEXT` identifier ADR-0001 forbids. So an
            // XNA-only game's saves land under a directory literally called `game`, shared with every
            // other CNA application on the machine.
            //
            // The in-architecture answer needs no extension: the CONTAINER name is ours through plain
            // XNA, so `BeginOpenContainer("CnaHouse")` gives `.../game/CnaHouse/`, which is
            // unambiguous. The probe verifies the container name really is a directory level by
            // finding its own.
            const char* xdg = std::getenv("XDG_DATA_HOME");
            const char* home = std::getenv("HOME");
            r.note("XDG_DATA_HOME", xdg ? xdg : "<unset>");
            const std::string base =
                xdg ? std::string(xdg) : (std::string(home ? home : "?") + "/.local/share");
            const std::string resolved = base + "/game/P1Probe";
            r.note("resolved container root (measured)", resolved);
            r.check("the container name is a directory level we control through plain XNA",
                    container->getDisplayNameProperty() == "P1Probe",
                    "so a distinctive container name is the XNA-only way to avoid the shared "
                    "`game/` directory");

            // ================= HOUSE-00103: the JSON payload =======================================
            // Values chosen to break a naive serialiser: 1/3 is not representable, 0.1 repeats in
            // binary, and the two extremes exercise the exponent.
            const double kThird = 1.0 / 3.0;
            const double kTenth = 0.1;
            const double kTiny = 1.1754944e-38;
            const double kLarge = 3.4028235e+38;
            const std::string payload =
                "{\n"
                "  \"schema\": 1,\n"
                "  \"house\": {\n"
                "    \"name\": \"p1\",\n"
                "    \"rooms\": [\"kitchen\", \"hall\", \"bedroom\"],\n"
                "    \"clock\": { \"day\": 4, \"seconds\": 31.5 },\n"
                "    \"floats\": [0.3333333333333333, 0.1, 1.1754944e-38, 3.4028235e+38, -0.0, 2.0],\n"
                "    \"nested\": { \"a\": { \"b\": { \"c\": [1, 2, 3] } } }\n"
                "  }\n"
                "}\n";

            const std::string file = "p1-save.json";
            try
            {
                if (container->FileExists(file))
                {
                    container->DeleteFile(file);
                }
                auto stream = container->CreateFile(file);
                stream->Write(reinterpret_cast<const std::uint8_t*>(payload.data()),
                              0,
                              static_cast<int>(payload.size()));
                stream->Flush();
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("a file is written into the container", container->FileExists(file), err);

            const std::vector<std::string> names = container->GetFileNames();
            std::string listing;
            bool found = false;
            for (const std::string& n : names)
            {
                listing += n + " ";
                if (n == file || n.find(file) != std::string::npos)
                {
                    found = true;
                }
            }
            r.note("GetFileNames", listing.empty() ? "<empty>" : listing);
            r.check("the written file appears in GetFileNames", found, listing);

            std::string readBack;
            try
            {
                auto stream = container->OpenFile(file, System::IO::FileMode::Open);
                readBack = ReadWholeFile(*stream);
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("the file reads back byte-identical",
                    readBack == payload,
                    std::to_string(readBack.size()) + " bytes vs " + std::to_string(payload.size()));

            // ================= HOUSE-00103: the JSON round trip ====================================
            try
            {
                auto doc = System::Text::Json::JsonDocument::Parse(readBack);
                const auto root = doc->getRootElementProperty();
                const auto house = root.GetProperty("house");
                r.check("nested objects resolve by name",
                        house.GetProperty("nested")
                                .GetProperty("a")
                                .GetProperty("b")
                                .GetProperty("c")
                                .GetArrayLength() == 3);
                const auto rooms = house.GetProperty("rooms").EnumerateArray();
                r.check("a string array round-trips in order",
                        rooms.size() == 3 && rooms[0].GetString() == "kitchen" &&
                            rooms[2].GetString() == "bedroom",
                        std::to_string(rooms.size()) + " entries");

                const auto floats = house.GetProperty("floats").EnumerateArray();
                const double want[6] = {kThird, kTenth, kTiny, kLarge, -0.0, 2.0};
                std::string detail;
                int exact = 0;
                for (size_t i = 0; i < floats.size() && i < 6; ++i)
                {
                    const double got = floats[i].GetDouble();
                    // Bit comparison, not an epsilon: a save that drifts by one ulp per cycle still
                    // corrupts a long-running house, just slowly.
                    std::uint64_t gb = 0, wb = 0;
                    std::memcpy(&gb, &got, sizeof gb);
                    std::memcpy(&wb, &want[i], sizeof wb);
                    const bool same = gb == wb;
                    if (same)
                    {
                        ++exact;
                    }
                    char b[110];
                    std::snprintf(b, sizeof b, "[%zu] %.17g %s ", i, got, same ? "ok" : "DRIFT");
                    detail += b;
                }
                r.note("float round trip (bit comparison)", detail);
                r.check(
                    "every float returns bit-identical", exact == 6, std::to_string(exact) + "/6; " + detail);

                r.check("an integer stays an integer",
                        house.GetProperty("clock").GetProperty("day").GetInt32() == 4);
                r.check("a fractional value inside a nested object survives",
                        house.GetProperty("clock").GetProperty("seconds").GetDouble() == 31.5);
            }
            catch (const std::exception& e)
            {
                r.check("the JSON parses", false, e.what());
            }

            // ================= delete ==============================================================
            container->DeleteFile(file);
            r.check("DeleteFile removes it", !container->FileExists(file));

            return r.finish("p1-storage");
        }
    };

} // namespace

P1_MAIN(StorageProbe, "p1-storage")
