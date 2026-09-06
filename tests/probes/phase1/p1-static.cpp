// HOUSE-00070 / HOUSE-00071 / HOUSE-00073 -- static glTF -> cna-content -> .cnb -> Model.
//
// What is measured, and why it is measured this way:
//
//  * bounds, orientation and UVs are checked by reading the compiled vertex and index buffers
//    back with the plain XNA `GetData` overloads and comparing them, element by element, against
//    the numbers `p1-make-gltf.py` wrote into the source .glb. "It rendered" is not evidence that
//    a position survived unchanged; a byte-level comparison against the authored value is.
//  * winding is checked by rendering the same model under all three cull modes into a
//    RenderTarget2D and counting covered pixels. That answers HOUSE-00071 numerically instead of
//    by looking at a screenshot.
//  * orientation is checked a second time in screen space: the ink bounding box of the render is
//    compared against the analytic projection of the authored box, which catches an axis remap
//    that a symmetric readback comparison could miss.
#include "p1-common.hpp"
#include "p1-fixtures/p1-static-expected.h"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kRt = 256;

    // The layout `CNA.ModelProcessor` was MEASURED to emit for a glTF primitive carrying
    // POSITION/NORMAL/TEXCOORD_0. It is NOT `VertexPositionNormalTexture`: a Vector4 TANGENT the
    // source never authored sits between the normal and the texture coordinate, and the stride is 48
    // rather than 40. Naming the struct after the measurement is the point -- a probe that assumed a
    // built-in type read every vertex after the first from the wrong offset.
    struct ModelVertex
    {
        Vector3 Position;
        Vector3 Normal;
        Vector4 Tangent;
        Vector2 TextureCoordinate;
    };

    static_assert(sizeof(ModelVertex) == 48, "the measured ModelProcessor stride");

    const char* CullName(CullMode m)
    {
        switch (m)
        {
            case CullMode::None:
                return "CullNone";
            case CullMode::CullClockwiseFace:
                return "CullClockwise";
            case CullMode::CullCounterClockwiseFace:
                return "CullCounterClockwise";
        }
        return "?";
    }

    struct Ink
    {
        int count = 0;
        int minX = kRt, minY = kRt, maxX = -1, maxY = -1;
    };

    Ink Measure(const std::vector<Color>& px)
    {
        Ink ink;
        for (int y = 0; y < kRt; ++y)
        {
            for (int x = 0; x < kRt; ++x)
            {
                const Color& c = px[static_cast<size_t>(y) * kRt + x];
                // background is pure black; anything the model wrote is not
                if (c.getRProperty() || c.getGProperty() || c.getBProperty())
                {
                    ++ink.count;
                    if (x < ink.minX)
                    {
                        ink.minX = x;
                    }
                    if (y < ink.minY)
                    {
                        ink.minY = y;
                    }
                    if (x > ink.maxX)
                    {
                        ink.maxX = x;
                    }
                    if (y > ink.maxY)
                    {
                        ink.maxY = y;
                    }
                }
            }
        }
        return ink;
    }

    class StaticProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            // NOTE: CNA's `Load<T>` returns T *by value*, so `Model` is a value type here where XNA
            // 4.0's is a reference type. The probe keeps the instance alive for its whole lifetime.
            Model model = content.Load<Model>("P1Static");

            // ---- structure -----------------------------------------------------------------------
            const auto& meshes = model.getMeshesProperty();
            const auto& bones = model.getBonesProperty();
            r.note("mesh count", std::to_string(meshes.getCountProperty()));
            r.note("bone count", std::to_string(bones.getCountProperty()));
            for (int i = 0; i < bones.getCountProperty(); ++i)
            {
                const ModelBone* b = bones[i];
                r.note(("bone[" + std::to_string(i) + "]").c_str(),
                       b->getNameProperty() + " parent=" +
                           (b->getParentProperty() ? b->getParentProperty()->getNameProperty() : "<none>") +
                           " children=" + std::to_string(b->getChildrenProperty().getCountProperty()));
            }
            r.check("exactly one mesh",
                    meshes.getCountProperty() == 1,
                    "got " + std::to_string(meshes.getCountProperty()));
            r.check("Root is non-null", model.getRootProperty() != nullptr);

            ModelMesh* mesh = meshes[0];
            const auto& parts = mesh->getMeshPartsProperty();
            r.note("mesh name", mesh->getNameProperty());
            r.check("exactly one mesh part",
                    parts.getCountProperty() == 1,
                    "got " + std::to_string(parts.getCountProperty()));
            if (parts.getCountProperty() != 1)
            {
                return r.finish("p1-static");
            }
            ModelMeshPart* part = parts[0];

            r.check("NumVertices matches the source .glb",
                    part->getNumVerticesProperty() == kExpectedVertexCount,
                    std::to_string(part->getNumVerticesProperty()) + " vs " +
                        std::to_string(kExpectedVertexCount));
            r.check("PrimitiveCount matches the source .glb",
                    part->getPrimitiveCountProperty() == kExpectedIndexCount / 3,
                    std::to_string(part->getPrimitiveCountProperty()) + " vs " +
                        std::to_string(kExpectedIndexCount / 3));

            // ---- vertex declaration --------------------------------------------------------------
            VertexBuffer* vb = part->getVertexBufferProperty();
            IndexBuffer* ib = part->getIndexBufferProperty();
            r.check("VertexBuffer is present", vb != nullptr);
            r.check("IndexBuffer is present", ib != nullptr);
            if (vb == nullptr || ib == nullptr)
            {
                return r.finish("p1-static");
            }

            const VertexDeclaration& decl = vb->getVertexDeclarationProperty();
            std::string layout = "stride=" + std::to_string(decl.getVertexStrideProperty());
            for (const VertexElement& e : decl.GetVertexElements())
            {
                layout += " {off=" + std::to_string(e.getOffsetProperty()) +
                          " usage=" + std::to_string(static_cast<int>(e.getVertexElementUsageProperty())) +
                          " fmt=" + std::to_string(static_cast<int>(e.getVertexElementFormatProperty())) +
                          " idx=" + std::to_string(e.getUsageIndexProperty()) + "}";
            }
            r.note("vertex declaration", layout);
            r.note("VertexPositionNormalTexture stride, for comparison",
                   std::to_string(sizeof(VertexPositionNormalTexture)));
            // The claim under test is not "the layout is some particular built-in type" -- it is that
            // the layout the processor emits is the one this probe decodes with, element for element.
            const auto& elems = decl.GetVertexElements();
            auto hasElement = [&](VertexElementUsage usage, VertexElementFormat fmt, int off)
            {
                for (size_t i = 0; i < elems.size(); ++i)
                {
                    if (elems[i].getVertexElementUsageProperty() == usage &&
                        elems[i].getVertexElementFormatProperty() == fmt &&
                        elems[i].getOffsetProperty() == off)
                    {
                        return true;
                    }
                }
                return false;
            };
            r.check("declaration matches the decoded struct, element for element",
                    decl.getVertexStrideProperty() == static_cast<int>(sizeof(ModelVertex)) &&
                        elems.size() == 4 &&
                        hasElement(VertexElementUsage::Position, VertexElementFormat::Vector3, 0) &&
                        hasElement(VertexElementUsage::Normal, VertexElementFormat::Vector3, 12) &&
                        hasElement(VertexElementUsage::Tangent, VertexElementFormat::Vector4, 24) &&
                        hasElement(VertexElementUsage::TextureCoordinate, VertexElementFormat::Vector2, 40),
                    "stride " + std::to_string(decl.getVertexStrideProperty()) + ", " +
                        std::to_string(elems.size()) + " elements");
            r.check("the emitted layout is NOT VertexPositionNormalTexture",
                    decl.getVertexStrideProperty() != static_cast<int>(sizeof(VertexPositionNormalTexture)),
                    "a built-in XNA vertex type cannot be used to read a Model back");

            // ---- vertex readback vs the authored numbers -------------------------------------------
            std::vector<ModelVertex> verts(static_cast<size_t>(part->getNumVerticesProperty()));
            vb->GetData(verts.data(), static_cast<int>(verts.size()));

            int posBad = 0, nrmBad = 0, uvBad = 0;
            std::string firstBad;
            for (size_t i = 0; i < verts.size() && i < static_cast<size_t>(kExpectedVertexCount); ++i)
            {
                const Vector3& p = verts[i].Position;
                const Vector3& n = verts[i].Normal;
                const Vector2& t = verts[i].TextureCoordinate;
                const bool pOk =
                    p.X == kExpectedPos[i][0] && p.Y == kExpectedPos[i][1] && p.Z == kExpectedPos[i][2];
                const bool nOk = p1::near(n.X, kExpectedNrm[i][0]) && p1::near(n.Y, kExpectedNrm[i][1]) &&
                                 p1::near(n.Z, kExpectedNrm[i][2]);
                const bool tOk = t.X == kExpectedUv[i][0] && t.Y == kExpectedUv[i][1];
                if (!pOk)
                {
                    ++posBad;
                }
                if (!nOk)
                {
                    ++nrmBad;
                }
                if (!tOk)
                {
                    ++uvBad;
                }
                if ((!pOk || !nOk || !tOk) && firstBad.empty())
                {
                    char buf[320];
                    std::snprintf(buf,
                                  sizeof buf,
                                  "v%zu pos(%g,%g,%g) want(%g,%g,%g) nrm(%g,%g,%g) want(%g,%g,%g) "
                                  "uv(%g,%g) want(%g,%g)",
                                  i,
                                  p.X,
                                  p.Y,
                                  p.Z,
                                  kExpectedPos[i][0],
                                  kExpectedPos[i][1],
                                  kExpectedPos[i][2],
                                  n.X,
                                  n.Y,
                                  n.Z,
                                  kExpectedNrm[i][0],
                                  kExpectedNrm[i][1],
                                  kExpectedNrm[i][2],
                                  t.X,
                                  t.Y,
                                  kExpectedUv[i][0],
                                  kExpectedUv[i][1]);
                    firstBad = buf;
                }
            }
            r.check("positions pass through bit-exact (no axis remap or flip)",
                    posBad == 0,
                    std::to_string(posBad) + " of " + std::to_string(verts.size()) + " differ; " + firstBad);
            r.check("normals pass through unchanged", nrmBad == 0, std::to_string(nrmBad) + " differ");
            r.check("UVs pass through bit-exact (no V flip)", uvBad == 0, std::to_string(uvBad) + " differ");

            // ---- index readback --------------------------------------------------------------------
            r.note("index element size",
                   ib->getIndexElementSizeProperty() == IndexElementSize::SixteenBits ? "16-bit" : "32-bit");
            r.note("index count", std::to_string(ib->getIndexCountProperty()));
            int idxBad = -1;
            if (ib->getIndexElementSizeProperty() == IndexElementSize::SixteenBits &&
                ib->getIndexCountProperty() == kExpectedIndexCount)
            {
                std::vector<std::uint16_t> idx(static_cast<size_t>(kExpectedIndexCount));
                ib->GetData(idx.data(), static_cast<int>(idx.size()));
                idxBad = 0;
                for (size_t i = 0; i < idx.size(); ++i)
                {
                    if (idx[i] != kExpectedIdx[i])
                    {
                        ++idxBad;
                    }
                }
            }
            r.check("indices are the source order, unreversed",
                    idxBad == 0,
                    idxBad < 0 ? "not comparable" : std::to_string(idxBad) + " differ");

            // ---- bounds ----------------------------------------------------------------------------
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
            for (const auto& v : verts)
            {
                const float c[3] = {v.Position.X, v.Position.Y, v.Position.Z};
                for (int k = 0; k < 3; ++k)
                {
                    lo[k] = std::fmin(lo[k], c[k]);
                    hi[k] = std::fmax(hi[k], c[k]);
                }
            }
            char bb[160];
            std::snprintf(
                bb, sizeof bb, "min(%g,%g,%g) max(%g,%g,%g)", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
            r.check("vertex-buffer bounds equal the authored bounds",
                    lo[0] == kExpectedMin[0] && lo[1] == kExpectedMin[1] && lo[2] == kExpectedMin[2] &&
                        hi[0] == kExpectedMax[0] && hi[1] == kExpectedMax[1] && hi[2] == kExpectedMax[2],
                    bb);

            // HOUSE-00073: is ModelMesh::BoundingSphere populated from .cnb, or degenerate?
            const BoundingSphere sphere = mesh->getBoundingSphereProperty();
            const float cx = 0.5f * (kExpectedMin[0] + kExpectedMax[0]);
            const float cy = 0.5f * (kExpectedMin[1] + kExpectedMax[1]);
            const float cz = 0.5f * (kExpectedMin[2] + kExpectedMax[2]);
            const float halfDiag = std::sqrt((kExpectedMax[0] - cx) * (kExpectedMax[0] - cx) +
                                             (kExpectedMax[1] - cy) * (kExpectedMax[1] - cy) +
                                             (kExpectedMax[2] - cz) * (kExpectedMax[2] - cz));
            char sb[220];
            std::snprintf(sb,
                          sizeof sb,
                          "centre(%g,%g,%g) r=%g; box centre(%g,%g,%g) half-diagonal=%g",
                          sphere.Center.X,
                          sphere.Center.Y,
                          sphere.Center.Z,
                          sphere.Radius,
                          cx,
                          cy,
                          cz,
                          halfDiag);
            r.check("ModelMesh::BoundingSphere is non-degenerate", sphere.Radius > 0.0f, sb);
            r.check(
                "BoundingSphere contains every vertex",
                [&]
                {
                    for (const auto& v : verts)
                    {
                        if (Vector3::Distance(v.Position, sphere.Center) > sphere.Radius + 1e-3f)
                        {
                            return false;
                        }
                    }
                    return true;
                }(),
                sb);

            // ---- winding, measured under all three cull modes ---------------------------------------
            // Camera on +Z looking down -Z. The measured surface is the OPEN single-sided quad, not the
            // box: a closed solid shows a face under either cull mode -- the near one, or the far one
            // through it -- so its silhouette is identical and it measures nothing. On an open quad the
            // wrong cull mode renders zero pixels.
            Model quad = content.Load<Model>("P1Quad");
            ModelMeshPart* qpart = quad.getMeshesProperty()[0]->getMeshPartsProperty()[0];
            VertexBuffer* qvb = qpart->getVertexBufferProperty();
            IndexBuffer* qib = qpart->getIndexBufferProperty();
            r.check("the quad fixture loaded with one triangle pair",
                    qpart->getPrimitiveCountProperty() == 2,
                    std::to_string(qpart->getPrimitiveCountProperty()) + " primitives");

            RenderTarget2D rt(gd, kRt, kRt, false, SurfaceFormat::Color, DepthFormat::Depth24);
            BasicEffect fx(gd);
            fx.setLightingEnabledProperty(false);
            fx.setTextureEnabledProperty(false);
            fx.setVertexColorEnabledProperty(false);
            fx.setDiffuseColorProperty(Vector3(1.0f, 1.0f, 1.0f));
            fx.setWorldProperty(Matrix::getIdentityProperty());
            // centred on the quad's own centre, so the analytic screen-space inverse below is exact
            const float qcx = 0.5f * (kQuadMin[0] + kQuadMax[0]);
            const float qcy = 0.5f * (kQuadMin[1] + kQuadMax[1]);
            fx.setViewProperty(
                Matrix::CreateLookAt(Vector3(qcx, qcy, 20.0f), Vector3(qcx, qcy, 0.0f), Vector3::Up));
            // A symmetric orthographic frame the probe can invert analytically.
            const float kHalf = 8.0f;
            fx.setProjectionProperty(Matrix::CreateOrthographic(2.0f * kHalf, 2.0f * kHalf, 0.1f, 100.0f));

            std::vector<Color> px(static_cast<size_t>(kRt) * kRt);
            Ink inkFor[3];
            const CullMode modes[3] = {
                CullMode::None, CullMode::CullClockwiseFace, CullMode::CullCounterClockwiseFace};
            for (int m = 0; m < 3; ++m)
            {
                RasterizerState rs;
                rs.setCullModeProperty(modes[m]);
                gd.SetRenderTarget(&rt);
                gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                gd.setRasterizerStateProperty(rs);
                gd.setDepthStencilStateProperty(DepthStencilState::Default);
                gd.SetVertexBuffer(qvb);
                gd.setIndicesProperty(qib);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p].Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList,
                                             qpart->getVertexOffsetProperty(),
                                             0,
                                             qpart->getNumVerticesProperty(),
                                             qpart->getStartIndexProperty(),
                                             qpart->getPrimitiveCountProperty());
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(px.data(), static_cast<int>(px.size()));
                inkFor[m] = ::Measure(px);
                char msg[200];
                std::snprintf(msg,
                              sizeof msg,
                              "%d px, bbox x[%d,%d] y[%d,%d]",
                              inkFor[m].count,
                              inkFor[m].minX,
                              inkFor[m].maxX,
                              inkFor[m].minY,
                              inkFor[m].maxY);
                r.note(CullName(modes[m]), msg);
            }

            const bool cwDraws = inkFor[1].count > 0;
            const bool ccwDraws = inkFor[2].count > 0;
            r.check("exactly one cull mode renders the front face",
                    cwDraws != ccwDraws,
                    std::string("CullClockwise=") + std::to_string(inkFor[1].count) +
                        " CullCounterClockwise=" + std::to_string(inkFor[2].count));
            r.check("the correct cull mode for glTF winding is CullClockwise",
                    cwDraws && !ccwDraws,
                    cwDraws ? "CullClockwise draws" : "CullCounterClockwise draws -- §5 claim is wrong");
            r.check("CullNone covers at least as much as the front-face mode",
                    inkFor[0].count >= inkFor[1].count && inkFor[0].count >= inkFor[2].count,
                    std::to_string(inkFor[0].count));

            // ---- orientation in screen space ---------------------------------------------------------
            // With the ortho frame above, world x maps to column (x-cx)/kHalf * (kRt/2) + kRt/2 and
            // world y maps to row kRt/2 - (y-cy)/kHalf * (kRt/2). Y up on screen is what makes the
            // expected minY come from the box's MAX y -- an inverted import would swap them.
            auto col = [&](float x) { return (x - qcx) / kHalf * (kRt * 0.5f) + kRt * 0.5f; };
            auto row = [&](float y) { return kRt * 0.5f - (y - qcy) / kHalf * (kRt * 0.5f); };
            const float wantMinX = col(kQuadMin[0]);
            const float wantMaxX = col(kQuadMax[0]);
            const float wantMinY = row(kQuadMax[1]); // world +Y is screen -Y
            const float wantMaxY = row(kQuadMin[1]);
            const Ink& ink = inkFor[0];
            char ob[240];
            std::snprintf(ob,
                          sizeof ob,
                          "measured x[%d,%d] y[%d,%d] vs analytic x[%.1f,%.1f] y[%.1f,%.1f]",
                          ink.minX,
                          ink.maxX,
                          ink.minY,
                          ink.maxY,
                          wantMinX,
                          wantMaxX,
                          wantMinY,
                          wantMaxY);
            // one pixel of rasterisation slack on each edge
            r.check("screen-space extents match the analytic projection (+Y is up)",
                    std::fabs(ink.minX - wantMinX) <= 1.5f && std::fabs(ink.maxX - (wantMaxX - 1)) <= 1.5f &&
                        std::fabs(ink.minY - wantMinY) <= 1.5f &&
                        std::fabs(ink.maxY - (wantMaxY - 1)) <= 1.5f,
                    ob);

            return r.finish("p1-static");
        }
    };

} // namespace

P1_MAIN(StaticProbe, "p1-static")
