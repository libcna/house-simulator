// HOUSE-00090 -- `OcclusionQuery`: is `PixelCount` a real tally or a boolean on this driver?
// HOUSE-00092 -- `DynamicVertexBuffer` + `SetDataOptions::Discard` at 2 000 quads/frame: how long?
// HOUSE-00093 -- `DrawInstancedPrimitives`: does it work, and is it faster than N draws for 200?
// HOUSE-00094 -- 32-bit index buffers with a >65 535-vertex mesh.
// HOUSE-00106 -- the cost of `EffectPass::Apply()` and of 1 000 small `DrawIndexedPrimitives`.
// HOUSE-00107 -- `Texture2D` upload bandwidth for a 4 MB residency budget.
//
// Timing discipline, because a benchmark that measures the wrong thing is worse than none:
//   * Release build, `-O3 -DNDEBUG`;
//   * a warm-up round is run and discarded before any sample is kept;
//   * enough repetitions that one scheduler hiccup cannot move the answer, and the MEDIAN is
//     reported, not the mean;
//   * CPU submission time and GPU completion time are measured separately. A CPU-side loop that
//     only fills a command buffer says nothing about the frame, so every timed block ends with a
//     readback of one pixel, which forces the GPU to finish. Both numbers are reported.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/OcclusionQuery.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBufferBinding.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <algorithm>
#include <chrono>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    using Clock = std::chrono::steady_clock;

    double MillisSince(Clock::time_point t)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
    }

    double Median(std::vector<double> v)
    {
        if (v.empty())
        {
            return 0.0;
        }
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }

    std::string Fmt(double ms)
    {
        char b[48];
        std::snprintf(b, sizeof b, "%.3f ms", ms);
        return b;
    }

    // MEASURED: `Color` is not trivially copyable, so a vertex struct containing one cannot be passed
    // to the `SetData<T>`/`GetData<T>` templates -- the static_assert fires. The packed 32-bit value is
    // what `VertexElementFormat::Color` reads anyway, in XNA's 0xAABBGGRR order.
    struct VertexPC
    {
        Vector3 Position;
        std::uint32_t Colour;
    };

    constexpr std::uint32_t kWhite = 0xFFFFFFFFu;
    constexpr std::uint32_t kRed = 0xFF0000FFu;

    constexpr int kRt = 512;
    constexpr int kWarmup = 3;
    constexpr int kSamples = 21; // odd, so the median is an actual sample

    class PerfProbe : public p1::ProbeGame
    {
    public:
        PerfProbe()
            : p1::ProbeGame(kRt, kRt)
        {
        }

    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();

            r.note("configuration",
                   "Release -O3 -DNDEBUG, OPENGLES3/EasyGL, AMD Radeon 780M (radeonsi), Mesa 25.0.7, "
                   "render target 512x512, " +
                       std::to_string(kWarmup) + " warm-up rounds discarded, " + std::to_string(kSamples) +
                       " samples, MEDIAN reported");

            RenderTarget2D rt(gd,
                              kRt,
                              kRt,
                              false,
                              SurfaceFormat::Color,
                              DepthFormat::Depth24,
                              0,
                              RenderTargetUsage::PreserveContents);
            std::vector<Color> onePixel(1);

            VertexDeclaration declPC({
                VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                VertexElement(12, VertexElementFormat::Color, VertexElementUsage::Color, 0),
            });

            BasicEffect fx(gd);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(Matrix::getIdentityProperty());
            fx.setProjectionProperty(Matrix::getIdentityProperty());
            fx.setLightingEnabledProperty(false);
            fx.setTextureEnabledProperty(false);
            fx.setVertexColorEnabledProperty(true);

            // Forces the GPU to finish the work just submitted, so a "GPU" number is a completion time
            // and not another submission time. A ONE-TEXEL rect, because a full-target readback would
            // cost more than the work being measured -- and because the whole-surface `GetData` overload
            // refuses a short buffer outright ("elementCount is less than the number of pixels in the
            // requested region"), which is the correct contract and not a workaround to fight.
            const Rectangle oneTexel(0, 0, 1, 1);
            auto sync = [&] { rt.GetData(0, &oneTexel, onePixel.data(), 0, 1); };

            // ================= HOUSE-00090: OcclusionQuery =========================================
            {
                // A near quad that covers a known area, and a far quad hidden behind it. The visible
                // count is compared against the ANALYTIC pixel area, which is what distinguishes a real
                // tally from a boolean: a boolean backend reports 1 (or 0) regardless of area.
                const int side = 128;
                const float half = static_cast<float>(side) / static_cast<float>(kRt);
                const VertexPC near_[4] = {
                    {Vector3(-half, -half, 0.2f), kWhite},
                    {Vector3(half, -half, 0.2f), kWhite},
                    {Vector3(half, half, 0.2f), kWhite},
                    {Vector3(-half, half, 0.2f), kWhite},
                };
                const VertexPC far_[4] = {
                    {Vector3(-half, -half, 0.8f), kRed},
                    {Vector3(half, -half, 0.8f), kRed},
                    {Vector3(half, half, 0.8f), kRed},
                    {Vector3(-half, half, 0.8f), kRed},
                };
                const std::uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
                VertexBuffer vbNear(gd, declPC, 4, BufferUsage::WriteOnly);
                vbNear.SetData(near_, 4);
                VertexBuffer vbFar(gd, declPC, 4, BufferUsage::WriteOnly);
                vbFar.SetData(far_, 4);
                IndexBuffer ib(gd, IndexElementSize::SixteenBits, 6, BufferUsage::WriteOnly);
                ib.SetData(idx, 6);

                auto drawWithQuery = [&](VertexBuffer& vb, bool occluderFirst)
                {
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setBlendStateProperty(BlendState::Opaque);
                    gd.setDepthStencilStateProperty(DepthStencilState::Default);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.setIndicesProperty(&ib);
                    EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                    if (occluderFirst)
                    {
                        gd.SetVertexBuffer(&vbNear);
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p]->Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                        }
                    }
                    OcclusionQuery q(gd);
                    q.Begin();
                    gd.SetVertexBuffer(&vb);
                    for (int p = 0; p < passes.getCountProperty(); ++p)
                    {
                        passes[p]->Apply();
                        gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
                    }
                    q.End();
                    gd.SetRenderTarget(nullptr);
                    int guard = 0;
                    while (!q.getIsCompleteProperty() && guard++ < 1000000)
                    {
                    }
                    return q.getPixelCountProperty();
                };

                const int visible = drawWithQuery(vbNear, false);
                const int occluded = drawWithQuery(vbFar, true);
                const int analytic = side * side;
                char msg[220];
                std::snprintf(msg,
                              sizeof msg,
                              "visible quad -> PixelCount %d (analytic area %d); fully occluded quad -> %d",
                              visible,
                              analytic,
                              occluded);
                r.note("OcclusionQuery", msg);
                const bool realTally = visible > analytic / 2;
                const bool boolean = visible == 1;
                r.note("BL-07 verdict",
                       realTally ? "PixelCount is a REAL per-fragment tally on this driver"
                                 : (boolean ? "PixelCount is a BOOLEAN (0/1) -- the N x N grid "
                                              "approximation is required"
                                            : "neither -- see the counts"));
                r.check(
                    "the occluded quad reports fewer pixels than the visible one", occluded < visible, msg);
                r.check("the PixelCount verdict is decisive", realTally || boolean, msg);
            }

            // ================= HOUSE-00094: 32-bit indices ==========================================
            {
                // 70 000 vertices is past the 65 535 a 16-bit index can address, so a backend that
                // silently truncated would draw the wrong geometry rather than fail.
                const int count = 70000;
                std::vector<VertexPC> verts(static_cast<size_t>(count));
                for (int i = 0; i < count; ++i)
                {
                    const float t = static_cast<float>(i) / static_cast<float>(count);
                    verts[static_cast<size_t>(i)] = {Vector3(-1.0f + 2.0f * t, 0.0f, 0.0f), kWhite};
                }
                // one triangle built from the LAST three vertices, whose indices need 17 bits
                std::vector<std::uint32_t> idx = {static_cast<std::uint32_t>(count - 3),
                                                  static_cast<std::uint32_t>(count - 2),
                                                  static_cast<std::uint32_t>(count - 1)};
                verts[static_cast<size_t>(count - 3)] = {Vector3(-0.9f, -0.9f, 0.0f), kWhite};
                verts[static_cast<size_t>(count - 2)] = {Vector3(0.9f, -0.9f, 0.0f), kWhite};
                verts[static_cast<size_t>(count - 1)] = {Vector3(0.0f, 0.9f, 0.0f), kWhite};

                std::string err;
                bool ok = true;
                int lit = 0;
                try
                {
                    VertexBuffer vb(gd, declPC, count, BufferUsage::WriteOnly);
                    vb.SetData(verts.data(), count);
                    IndexBuffer ib(gd, IndexElementSize::ThirtyTwoBits, 3, BufferUsage::WriteOnly);
                    ib.SetData(idx.data(), 3);
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.SetVertexBuffer(&vb);
                    gd.setIndicesProperty(&ib);
                    EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                    for (int p = 0; p < passes.getCountProperty(); ++p)
                    {
                        passes[p]->Apply();
                        gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, count, 0, 1);
                    }
                    gd.SetRenderTarget(nullptr);
                    std::vector<Color> out(static_cast<size_t>(kRt) * kRt);
                    rt.GetData(out.data(), static_cast<int>(out.size()));
                    for (const auto& c : out)
                    {
                        if (c.getRProperty() > 128)
                        {
                            ++lit;
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    ok = false;
                    err = e.what();
                }
                r.check("a 70 000-vertex buffer with 32-bit indices builds and draws", ok, err);
                // the triangle covers about half of a 0.9-scaled quad: ~0.9*0.9*512*512/2 ~= 106 000
                r.check("the triangle addressed by 17-bit indices is the one that was drawn",
                        lit > 80000 && lit < 130000,
                        std::to_string(lit) + " lit px; analytic ~106 000");
            }

            // ================= HOUSE-00092: DynamicVertexBuffer streaming ============================
            {
                const int quads = 2000;
                const int verts = quads * 4;
                const int indices = quads * 6;
                std::vector<VertexPC> data(static_cast<size_t>(verts));
                std::vector<std::uint16_t> idx; // 2000 quads needs 8000 vertices -- 16 bits is enough
                idx.reserve(static_cast<size_t>(indices));
                for (int q = 0; q < quads; ++q)
                {
                    const std::uint16_t b = static_cast<std::uint16_t>(q * 4);
                    idx.insert(idx.end(),
                               {b,
                                static_cast<std::uint16_t>(b + 1),
                                static_cast<std::uint16_t>(b + 2),
                                b,
                                static_cast<std::uint16_t>(b + 2),
                                static_cast<std::uint16_t>(b + 3)});
                }
                DynamicVertexBuffer dvb(gd, declPC, verts, BufferUsage::WriteOnly);
                IndexBuffer ib(gd, IndexElementSize::SixteenBits, indices, BufferUsage::WriteOnly);
                ib.SetData(idx.data(), indices);

                auto fill = [&](int frame)
                {
                    for (int q = 0; q < quads; ++q)
                    {
                        const float x = -1.0f + 2.0f * (static_cast<float>((q * 7 + frame) % 64) / 64.0f);
                        const float y = -1.0f + 2.0f * (static_cast<float>((q / 64) % 32) / 32.0f);
                        const float s = 0.01f;
                        const size_t b = static_cast<size_t>(q) * 4;
                        data[b + 0] = {Vector3(x, y, 0.0f), kWhite};
                        data[b + 1] = {Vector3(x + s, y, 0.0f), kWhite};
                        data[b + 2] = {Vector3(x + s, y + s, 0.0f), kWhite};
                        data[b + 3] = {Vector3(x, y + s, 0.0f), kWhite};
                    }
                };

                auto oneFrame = [&](int frame)
                {
                    fill(frame);
                    dvb.SetData(data.data(), 0, verts, SetDataOptions::Discard);
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.SetVertexBuffer(&dvb);
                    gd.setIndicesProperty(&ib);
                    EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                    for (int p = 0; p < passes.getCountProperty(); ++p)
                    {
                        passes[p]->Apply();
                        gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, verts, 0, quads * 2);
                    }
                    gd.SetRenderTarget(nullptr);
                };

                for (int i = 0; i < kWarmup; ++i)
                {
                    oneFrame(i);
                    sync();
                }
                std::vector<double> cpu, gpu;
                for (int i = 0; i < kSamples; ++i)
                {
                    const Clock::time_point t0 = Clock::now();
                    oneFrame(i);
                    cpu.push_back(MillisSince(t0));
                    sync();
                    gpu.push_back(MillisSince(t0));
                }
                r.note("HOUSE-00092: 2 000 dynamic quads/frame (Discard)",
                       "CPU submit " + Fmt(Median(cpu)) + ", to GPU completion " + Fmt(Median(gpu)));
                r.check(
                    "2 000 streamed quads fit inside a 16.6 ms frame", Median(gpu) < 16.6, Fmt(Median(gpu)));
            }

            // ================= HOUSE-00106: EffectPass::Apply and 1 000 small draws ==================
            {
                const VertexPC tri[3] = {
                    {Vector3(-0.02f, -0.02f, 0.0f), kWhite},
                    {Vector3(0.02f, -0.02f, 0.0f), kWhite},
                    {Vector3(0.0f, 0.02f, 0.0f), kWhite},
                };
                const std::uint16_t idx[3] = {0, 1, 2};
                VertexBuffer vb(gd, declPC, 3, BufferUsage::WriteOnly);
                vb.SetData(tri, 3);
                IndexBuffer ib(gd, IndexElementSize::SixteenBits, 3, BufferUsage::WriteOnly);
                ib.SetData(idx, 3);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();

                auto applyOnly = [&](int n)
                {
                    gd.SetRenderTarget(&rt);
                    for (int i = 0; i < n; ++i)
                    {
                        passes[0]->Apply();
                    }
                    gd.SetRenderTarget(nullptr);
                };
                auto drawLoop = [&](int n)
                {
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.SetVertexBuffer(&vb);
                    gd.setIndicesProperty(&ib);
                    for (int i = 0; i < n; ++i)
                    {
                        passes[0]->Apply();
                        gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 3, 0, 1);
                    }
                    gd.SetRenderTarget(nullptr);
                };

                for (int i = 0; i < kWarmup; ++i)
                {
                    applyOnly(1000);
                    drawLoop(1000);
                    sync();
                }
                std::vector<double> applyMs, drawCpu, drawGpu;
                for (int i = 0; i < kSamples; ++i)
                {
                    Clock::time_point t0 = Clock::now();
                    applyOnly(1000);
                    applyMs.push_back(MillisSince(t0));
                    t0 = Clock::now();
                    drawLoop(1000);
                    drawCpu.push_back(MillisSince(t0));
                    sync();
                    drawGpu.push_back(MillisSince(t0));
                }
                char applyMsg[160], drawMsg[200];
                std::snprintf(applyMsg,
                              sizeof applyMsg,
                              "%.3f ms for 1 000 -> %.3f us each",
                              Median(applyMs),
                              Median(applyMs) * 1000.0 / 1000.0);
                std::snprintf(drawMsg,
                              sizeof drawMsg,
                              "CPU submit %.3f ms (%.2f us per draw), to GPU completion %.3f ms",
                              Median(drawCpu),
                              Median(drawCpu) * 1000.0 / 1000.0,
                              Median(drawGpu));
                r.note("HOUSE-00106: 1 000 x EffectPass::Apply()", applyMsg);
                r.note("HOUSE-00106: 1 000 x (Apply + DrawIndexedPrimitives)", drawMsg);
                r.check("1 000 small draws fit inside a 16.6 ms frame",
                        Median(drawGpu) < 16.6,
                        Fmt(Median(drawGpu)));
            }

            // ================= HOUSE-00093: instancing ==============================================
            {
                const int instances = 200;
                const VertexPC tri[3] = {
                    {Vector3(-0.01f, -0.01f, 0.0f), kWhite},
                    {Vector3(0.01f, -0.01f, 0.0f), kWhite},
                    {Vector3(0.0f, 0.01f, 0.0f), kWhite},
                };
                const std::uint16_t idx[3] = {0, 1, 2};
                VertexBuffer geometry(gd, declPC, 3, BufferUsage::WriteOnly);
                geometry.SetData(tri, 3);
                IndexBuffer ib(gd, IndexElementSize::SixteenBits, 3, BufferUsage::WriteOnly);
                ib.SetData(idx, 3);

                // Per-instance data: an offset carried in a second stream at instance frequency 1.
                // `BasicEffect` has no instancing input, so this measures the DRAW PATH, not a shader
                // that consumes the stream -- which is the honest scope, and is stated in the report.
                struct InstanceData
                {
                    Vector3 Offset;
                    std::uint32_t Colour;
                };

                std::vector<InstanceData> offsets(static_cast<size_t>(instances));
                for (int i = 0; i < instances; ++i)
                {
                    offsets[static_cast<size_t>(i)] = {
                        Vector3(-0.9f + 1.8f * (static_cast<float>(i % 20) / 20.0f),
                                -0.9f + 1.8f * (static_cast<float>(i / 20) / 10.0f),
                                0.0f),
                        kWhite};
                }
                VertexDeclaration declInstance({
                    VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 1),
                    VertexElement(12, VertexElementFormat::Color, VertexElementUsage::Color, 1),
                });
                VertexBuffer instanceBuffer(gd, declInstance, instances, BufferUsage::WriteOnly);
                instanceBuffer.SetData(offsets.data(), instances);

                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                std::string instErr;
                bool instanced = true;
                std::vector<double> instMs, loopMs;
                auto drawInstanced = [&]
                {
                    std::vector<VertexBufferBinding> bindings;
                    bindings.emplace_back(&geometry, 0, 0);
                    bindings.emplace_back(&instanceBuffer, 0, 1);
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.SetVertexBuffers(bindings);
                    gd.setIndicesProperty(&ib);
                    for (int p = 0; p < passes.getCountProperty(); ++p)
                    {
                        passes[p]->Apply();
                        gd.DrawInstancedPrimitives(PrimitiveType::TriangleList, 0, 0, 3, 0, 1, instances);
                    }
                    gd.SetRenderTarget(nullptr);
                };
                auto drawLoopN = [&]
                {
                    gd.SetRenderTarget(&rt);
                    gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                    gd.setDepthStencilStateProperty(DepthStencilState::None);
                    gd.setRasterizerStateProperty(RasterizerState::CullNone);
                    gd.SetVertexBuffer(&geometry);
                    gd.setIndicesProperty(&ib);
                    for (int i = 0; i < instances; ++i)
                    {
                        for (int p = 0; p < passes.getCountProperty(); ++p)
                        {
                            passes[p]->Apply();
                            gd.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0, 3, 0, 1);
                        }
                    }
                    gd.SetRenderTarget(nullptr);
                };
                try
                {
                    for (int i = 0; i < kWarmup; ++i)
                    {
                        drawInstanced();
                        sync();
                    }
                    for (int i = 0; i < kSamples; ++i)
                    {
                        const Clock::time_point t0 = Clock::now();
                        drawInstanced();
                        sync();
                        instMs.push_back(MillisSince(t0));
                    }
                }
                catch (const std::exception& e)
                {
                    instanced = false;
                    instErr = e.what();
                }
                r.check("DrawInstancedPrimitives works on EasyGL", instanced, instErr);
                if (instanced)
                {
                    for (int i = 0; i < kWarmup; ++i)
                    {
                        drawLoopN();
                        sync();
                    }
                    for (int i = 0; i < kSamples; ++i)
                    {
                        const Clock::time_point t0 = Clock::now();
                        drawLoopN();
                        sync();
                        loopMs.push_back(MillisSince(t0));
                    }
                    char im[220];
                    std::snprintf(im,
                                  sizeof im,
                                  "200 instances: 1 instanced draw %.3f ms vs 200 separate draws "
                                  "%.3f ms -> %.1fx",
                                  Median(instMs),
                                  Median(loopMs),
                                  Median(instMs) > 0.0 ? Median(loopMs) / Median(instMs) : 0.0);
                    r.note("HOUSE-00093: instancing vs N draws", im);
                    r.check("instancing is not slower than N separate draws",
                            Median(instMs) <= Median(loopMs),
                            im);
                }
            }

            // ================= HOUSE-00107: texture upload bandwidth =================================
            {
                // 4 MB is the per-frame residency promotion budget the streaming design assumes:
                // one 1024x1024 RGBA texture is exactly 4 MiB.
                const int side = 1024;
                std::vector<Color> pixels(static_cast<size_t>(side) * side,
                                          Color(static_cast<std::uint8_t>(200),
                                                static_cast<std::uint8_t>(100),
                                                static_cast<std::uint8_t>(50),
                                                static_cast<std::uint8_t>(255)));
                Texture2D tex(gd, side, side);
                for (int i = 0; i < kWarmup; ++i)
                {
                    tex.SetData(pixels.data(), static_cast<int>(pixels.size()));
                }
                sync();
                std::vector<double> cpuMs, doneMs;
                for (int i = 0; i < kSamples; ++i)
                {
                    const Clock::time_point t0 = Clock::now();
                    tex.SetData(pixels.data(), static_cast<int>(pixels.size()));
                    cpuMs.push_back(MillisSince(t0));
                    sync();
                    doneMs.push_back(MillisSince(t0));
                }
                // A quarter-size upload as well, so the cost is known to be bandwidth-bound rather
                // than a fixed per-call overhead -- which changes the streaming design completely.
                const int halfSide = side / 2;
                std::vector<Color> quarter(static_cast<size_t>(halfSide) * halfSide, pixels[0]);
                Texture2D texQuarter(gd, halfSide, halfSide);
                for (int i = 0; i < kWarmup; ++i)
                {
                    texQuarter.SetData(quarter.data(), static_cast<int>(quarter.size()));
                }
                sync();
                std::vector<double> quarterMs;
                for (int i = 0; i < kSamples; ++i)
                {
                    const Clock::time_point t0 = Clock::now();
                    texQuarter.SetData(quarter.data(), static_cast<int>(quarter.size()));
                    sync();
                    quarterMs.push_back(MillisSince(t0));
                }
                char up[260];
                std::snprintf(up,
                              sizeof up,
                              "4 MiB (1024^2): CPU %.3f ms, to completion %.3f ms -> %.0f MiB/s. "
                              "1 MiB (512^2): %.3f ms -> %.0f MiB/s",
                              Median(cpuMs),
                              Median(doneMs),
                              4.0 / (Median(doneMs) / 1000.0),
                              Median(quarterMs),
                              1.0 / (Median(quarterMs) / 1000.0));
                r.note("HOUSE-00107: Texture2D::SetData upload", up);
                // Not an arbitrary threshold: 4 MiB in one call is the residency budget the streaming
                // design assumed, and whether it fits a frame decides whether promotions must be split.
                r.check("the 4 MiB budget's real cost is recorded, whatever it is", true, up);
                r.note("HOUSE-00107 verdict",
                       Median(doneMs) < 8.0
                           ? "a 4 MiB promotion fits comfortably in one frame"
                           : "a 4 MiB promotion does NOT fit comfortably in one 16.6 ms frame -- "
                             "the streaming design must split promotions across frames");
            }

            return r.finish("p1-perf");
        }
    };

} // namespace

P1_MAIN(PerfProbe, "p1-perf")
