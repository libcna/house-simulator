// Shared scaffolding for the cna-house phase-1 capability probes.
//
// XNA-only, exactly as ADR-0001 requires of runtime code: no `CNA/` include, no `CNA::`
// reference, no `*EXT*` identifier, no native graphics call. A probe that needed one of those
// would be measuring something the project may not use, which is not a useful measurement.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace p1
{

    // A probe reports every check it made, then a single verdict line the caller greps for. Checks
    // are never silently skipped: a check that could not run is recorded as such.
    class Report
    {
    public:
        void check(const char* name, bool ok, const std::string& detail = {})
        {
            ++total_;
            if (ok)
            {
                ++passed_;
            }
            else
            {
                failed_.push_back(name);
            }
            std::printf("  [%s] %-46s %s\n", ok ? "ok" : "FAIL", name, detail.c_str());
        }

        void note(const char* name, const std::string& detail)
        {
            std::printf("  [--] %-46s %s\n", name, detail.c_str());
        }

        [[nodiscard]] bool ok() const
        {
            return failed_.empty() && total_ > 0;
        }

        int finish(const char* probe)
        {
            std::printf("%s: %d/%d checks passed\n", probe, passed_, total_);
            if (!failed_.empty())
            {
                std::printf("%s: failing checks:", probe);
                for (const auto& f : failed_)
                {
                    std::printf(" %s", f.c_str());
                }
                std::printf("\n");
            }
            std::printf("%s: %s\n", probe, ok() ? "PASS" : "FAIL");
            return ok() ? 0 : 1;
        }

    private:
        int total_ = 0;
        int passed_ = 0;
        std::vector<std::string> failed_;
    };

    inline bool near(float a, float b, float eps = 1e-5f)
    {
        return std::fabs(a - b) <= eps;
    }

    // The probes run their measurement once, from a real frame, then exit. `Game::Run()` is the only
    // lifetime CNA offers on desktop, so a probe is a Game whose Draw does the work and then calls
    // Exit() -- which is what an application would do too.
    class ProbeGame : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit ProbeGame(int width = 640, int height = 360)
            : gdm_(this)
        {
            gdm_.setPreferredBackBufferWidthProperty(width);
            gdm_.setPreferredBackBufferHeightProperty(height);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
        }

        [[nodiscard]] int exitCode() const
        {
            return code_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (done_)
            {
                return;
            }
            done_ = true;
            try
            {
                code_ = Measure();
            }
            catch (const std::exception& e)
            {
                std::printf("  [FAIL] probe threw: %s\n", e.what());
                code_ = 2;
            }
            catch (...)
            {
                std::printf("  [FAIL] probe threw a non-std exception\n");
                code_ = 2;
            }
            Exit();
        }

        // The measurement. Runs once, inside a frame, with a live device.
        virtual int Measure() = 0;

        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;

    private:
        bool done_ = false;
        int code_ = 3; // 3 == Measure() never ran
    };

    // --- reading a vertex buffer back whatever its layout is ---------------------------------------
    //
    // `VertexBuffer::GetData<T>` derives the stride from `sizeof(T)`, so reading a buffer whose layout
    // is not known at compile time needs a struct of exactly the measured stride. Dispatching on the
    // stride to a fixed set of byte-blob structs is ugly but honest: the alternative is to assume a
    // built-in XNA vertex type, which is exactly the assumption that made the first version of
    // `p1-static` read every vertex after the first from the wrong offset.
    template<int N>
    struct RawVertex
    {
        unsigned char b[N];
    };

    // clang-format off
    // A macro-generated switch: one case per plausible stride, because `GetData<T>` derives the
    // stride from `sizeof(T)` and the stride is not known until it is measured. clang-format folds
    // the trailing `default` onto the last case label and mangles the continuation backslashes, so
    // the whole function is exempted rather than contorted to satisfy it.
    inline bool ReadVertexBytes(Microsoft::Xna::Framework::Graphics::VertexBuffer* vb, int count,
                                int stride, std::vector<unsigned char>& out)
    {
        out.assign(static_cast<size_t>(count) * static_cast<size_t>(stride), 0);
#define P1_STRIDE_CASE(N)                                          \
    case N: {                                                      \
        std::vector<RawVertex<N>> tmp(static_cast<size_t>(count)); \
        vb->GetData(tmp.data(), count);                            \
        std::memcpy(out.data(), tmp.data(), out.size());           \
        return true;                                               \
    }
        switch (stride) {
            P1_STRIDE_CASE(20) P1_STRIDE_CASE(24) P1_STRIDE_CASE(28) P1_STRIDE_CASE(32)
            P1_STRIDE_CASE(36) P1_STRIDE_CASE(40) P1_STRIDE_CASE(44) P1_STRIDE_CASE(48)
            P1_STRIDE_CASE(52) P1_STRIDE_CASE(56) P1_STRIDE_CASE(60) P1_STRIDE_CASE(64)
            P1_STRIDE_CASE(68) P1_STRIDE_CASE(72) P1_STRIDE_CASE(76) P1_STRIDE_CASE(80)
            default: return false;
        }
#undef P1_STRIDE_CASE
    }

    // clang-format on

    // Locates one element of a vertex declaration by usage and usage index. Returns false when the
    // declaration does not carry it -- which is itself a measurement, never an assertion.
    inline bool FindElement(const Microsoft::Xna::Framework::Graphics::VertexDeclaration& decl,
                            Microsoft::Xna::Framework::Graphics::VertexElementUsage usage,
                            int usageIndex,
                            Microsoft::Xna::Framework::Graphics::VertexElementFormat& fmtOut,
                            int& offsetOut)
    {
        const auto& elems = decl.GetVertexElements();
        for (size_t i = 0; i < elems.size(); ++i)
        {
            if (elems[i].getVertexElementUsageProperty() == usage &&
                elems[i].getUsageIndexProperty() == usageIndex)
            {
                fmtOut = elems[i].getVertexElementFormatProperty();
                offsetOut = elems[i].getOffsetProperty();
                return true;
            }
        }
        return false;
    }

    inline std::string DescribeDeclaration(const Microsoft::Xna::Framework::Graphics::VertexDeclaration& decl)
    {
        static const char* kUsage[] = {"Position",
                                       "Color",
                                       "TexCoord",
                                       "Normal",
                                       "Binormal",
                                       "Tangent",
                                       "BlendIndices",
                                       "BlendWeight",
                                       "Depth",
                                       "Fog",
                                       "PointSize",
                                       "Sample",
                                       "TessellateFactor"};
        static const char* kFormat[] = {"Single",
                                        "Vector2",
                                        "Vector3",
                                        "Vector4",
                                        "Color",
                                        "Byte4",
                                        "Short2",
                                        "Short4",
                                        "NormalizedShort2",
                                        "NormalizedShort4",
                                        "HalfVector2",
                                        "HalfVector4"};
        std::string s = "stride=" + std::to_string(decl.getVertexStrideProperty());
        const auto& elems = decl.GetVertexElements();
        for (size_t i = 0; i < elems.size(); ++i)
        {
            const int u = static_cast<int>(elems[i].getVertexElementUsageProperty());
            const int f = static_cast<int>(elems[i].getVertexElementFormatProperty());
            s += " " + std::string(u >= 0 && u < 13 ? kUsage[u] : "?") +
                 (elems[i].getUsageIndexProperty() ? std::to_string(elems[i].getUsageIndexProperty()) : "") +
                 "@" + std::to_string(elems[i].getOffsetProperty()) + ":" +
                 std::string(f >= 0 && f < 12 ? kFormat[f] : "?");
        }
        return s;
    }

} // namespace p1

// Every probe's `main` is the same three lines; a probe that forgot to propagate its verdict to
// the process exit code would silently "pass" in a script.
#define P1_MAIN(GameType, probeName)                                                                         \
    int main()                                                                                               \
    {                                                                                                        \
        std::printf("%s: start\n", probeName);                                                               \
        GameType game;                                                                                       \
        game.Run();                                                                                          \
        std::printf("%s: exit=%d\n", probeName, game.exitCode());                                            \
        return game.exitCode();                                                                              \
    }
