// HOUSE-00076 -- one skin per runtime `.glb`, and the attachment record that binds it.
//
// The task's premise needed one correction, which the first run produced and which is recorded
// rather than worked around: **there is no loadable unsplit source to compare against.**
// `CNA.ModelProcessor` REFUSES a multi-skin glTF outright --
//
//     Process (CNA.ModelProcessor): glTF produced 2 Model documents; set ModelProcessor bool
//     parameter 'generateChildAssets' to true to publish the deterministic multi-Model output set.
//
// -- so "the split parts render identically to the unsplit source" cannot mean what it says. The
// measurement that carries the same weight, and that this probe makes, is that the TWO independent
// split routes agree pixel for pixel:
//
//   route 1  the pipeline's own per-skin split (`generateChildAssets=true`), which publishes the
//            lexicographically first group as the primary asset and the rest as named children;
//   route 2  `p1-split-skins.py`, the project-owned offline splitter, one `.glb` per skin.
//
// Both are driven from ONE pose of ONE shared skeleton, evaluated once, which is the property that
// actually matters: the parts must reassemble. `getSkinsEXTProperty()` is never called and is
// never needed.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/GameServiceContainer.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"

#include "System/IO/File.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonElement.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kRt = 256;

    // The attachment record, exactly as a `.chanim` sidecar will carry it: read from JSON at load time
    // with `System::Text::Json`, never inferred from a CNA query.
    struct Attachment
    {
        std::string part;
        std::string skeletonRoot;
        std::string attachmentBone;
        std::vector<std::string> joints;
        bool valid = false;
    };

    Attachment ReadAttachment(const std::string& path)
    {
        Attachment a;
        const std::string text = System::IO::File::ReadAllText(path);
        auto doc = System::Text::Json::JsonDocument::Parse(text);
        const auto root = doc->getRootElementProperty();
        a.part = root.GetProperty("part").GetString();
        a.skeletonRoot = root.GetProperty("skeletonRoot").GetString();
        a.attachmentBone = root.GetProperty("attachmentBone").GetString();
        for (const auto& j : root.GetProperty("joints").EnumerateArray())
        {
            a.joints.push_back(j.GetString());
        }
        a.valid = !a.joints.empty();
        return a;
    }

    struct Piece
    {
        Model model;
        ModelMeshPart* part = nullptr;
        std::vector<std::string> joints; // skin-local order, by name
        std::vector<Matrix> inverseBind;
    };

} // namespace

namespace
{

    class SkinSplitProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();
            Content::ContentManager& content = getContentProperty();
            // A second manager over the pipeline-split output tree. It needs the Game's service
            // provider: a ContentManager built with a null provider throws
            // "no GraphicsDevice is available from the service provider" at the first Load<Model>.
            Content::ContentManager childContent(&getServicesProperty());

            content.setRootDirectoryProperty("build-probe/p1-content");
            childContent.setRootDirectoryProperty("build-probe/p1-content-child");

            // ---- the attachment records, read back from the files the splitter wrote ---------------
            const Attachment attA = ReadAttachment("build-probe/p1-fixtures/P1PartA.attach.json");
            const Attachment attB = ReadAttachment("build-probe/p1-fixtures/P1PartB.attach.json");
            r.check("attachment record A round-trips through System::Text::Json",
                    attA.valid,
                    attA.part + " skeleton=" + attA.skeletonRoot + " bone=" + attA.attachmentBone +
                        " joints=" + std::to_string(attA.joints.size()));
            r.check("attachment record B round-trips through System::Text::Json",
                    attB.valid,
                    attB.part + " skeleton=" + attB.skeletonRoot + " bone=" + attB.attachmentBone +
                        " joints=" + std::to_string(attB.joints.size()));
            r.check("record A names the authored joints in skin order",
                    attA.joints.size() == 2 && attA.joints[0] == "P1AJ0" && attA.joints[1] == "P1AJ1",
                    attA.joints.empty() ? "" : attA.joints[0] + "," + attA.joints[1]);
            r.check("record B names the authored joints in skin order",
                    attB.joints.size() == 2 && attB.joints[0] == "P1BJ0" && attB.joints[1] == "P1BJ1",
                    attB.joints.empty() ? "" : attB.joints[0] + "," + attB.joints[1]);
            r.check("both parts declare the same shared skeleton root",
                    attA.skeletonRoot == attB.skeletonRoot && attA.skeletonRoot == "P1TwoRoot",
                    attA.skeletonRoot + " / " + attB.skeletonRoot);
            r.check("each part records its own attachment bone",
                    attA.attachmentBone == "P1AJ0" && attB.attachmentBone == "P1BJ0",
                    attA.attachmentBone + " / " + attB.attachmentBone);

            // ---- load both routes -------------------------------------------------------------------
            auto makePiece = [&](Content::ContentManager& cm,
                                 const std::string& asset,
                                 const std::vector<std::string>& joints,
                                 float rootX)
            {
                auto piece = std::make_shared<Piece>();
                piece->model = cm.Load<Model>(asset);
                piece->part = piece->model.getMeshesProperty()[0]->getMeshPartsProperty()[0];
                piece->joints = joints;
                // The inverse bind matrices are the project's, from the source asset, computed offline:
                // joint 0's world bind is (rootX, 0, 0) and joint 1's is (rootX, 2, 0).
                piece->inverseBind.push_back(Matrix::CreateTranslation(Vector3(-rootX, 0.0f, 0.0f)));
                piece->inverseBind.push_back(Matrix::CreateTranslation(Vector3(-rootX, -2.0f, 0.0f)));
                return piece;
            };

            std::vector<std::shared_ptr<Piece>> offline, pipeline;
            std::string loadError;
            try
            {
                offline.push_back(makePiece(content, "P1PartA", attA.joints, -2.0f));
                offline.push_back(makePiece(content, "P1PartB", attB.joints, 2.0f));
                pipeline.push_back(makePiece(childContent, "P1TwoSkin", attA.joints, -2.0f));
                pipeline.push_back(makePiece(childContent, "P1TwoSkin_P1SkinB", attB.joints, 2.0f));
            }
            catch (const std::exception& e)
            {
                loadError = e.what();
            }
            r.check("both split routes produced loadable single-skin Models", loadError.empty(), loadError);
            if (!loadError.empty())
            {
                return r.finish("p1-skinsplit");
            }

            for (size_t i = 0; i < offline.size(); ++i)
            {
                r.check((offline[i]->joints[0] + ": the offline part has exactly one mesh").c_str(),
                        offline[i]->model.getMeshesProperty().getCountProperty() == 1,
                        std::to_string(offline[i]->model.getMeshesProperty().getCountProperty()));
                r.check((pipeline[i]->joints[0] + ": the pipeline part has exactly one mesh").c_str(),
                        pipeline[i]->model.getMeshesProperty().getCountProperty() == 1,
                        std::to_string(pipeline[i]->model.getMeshesProperty().getCountProperty()));
            }

            // ---- one pose of one shared skeleton, evaluated once ------------------------------------
            // Both parts of both routes are driven from the SAME joint-name -> local-matrix table, which
            // is the property "the parts reassemble on a shared skeleton" actually asserts.
            const Quaternion bend =
                Quaternion::CreateFromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), MathHelper::PiOver4);
            auto poseOf = [&](const std::string& joint) -> Matrix
            {
                if (joint == "P1AJ0")
                {
                    return Matrix::CreateTranslation(Vector3(-2.0f, 0.0f, 0.0f));
                }
                if (joint == "P1BJ0")
                {
                    return Matrix::CreateTranslation(Vector3(2.0f, 0.0f, 0.0f));
                }
                // both second joints bend, so a route that mixed up its parts would move the wrong ribbon
                return Matrix::CreateFromQuaternion(bend) *
                       Matrix::CreateTranslation(Vector3(0.0f, 2.0f, 0.0f));
            };

            auto paletteFor = [&](const Piece& p)
            {
                std::vector<Matrix> pal;
                for (size_t j = 0; j < p.joints.size(); ++j)
                {
                    // absolute = local * absolute(parent); joint 0's parent is the identity skeleton root
                    Matrix absolute = poseOf(p.joints[j]);
                    if (j > 0)
                    {
                        absolute = absolute * poseOf(p.joints[0]);
                    }
                    pal.push_back(p.inverseBind[j] * absolute);
                }
                return pal;
            };

            RenderTarget2D rt(gd, kRt, kRt, false, SurfaceFormat::Color, DepthFormat::Depth24);
            SkinnedEffect fx(gd);
            fx.setWeightsPerVertexProperty(4);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(
                Matrix::CreateLookAt(Vector3(0.0f, 2.0f, 12.0f), Vector3(0.0f, 2.0f, 0.0f), Vector3::Up));
            fx.setProjectionProperty(Matrix::CreateOrthographic(14.0f, 14.0f, 0.1f, 100.0f));
            fx.setAmbientLightColorProperty(Vector3::One);
            fx.setDiffuseColorProperty(Vector3::One);

            auto renderRoute = [&](const std::vector<std::shared_ptr<Piece>>& pieces, std::vector<Color>& out)
            {
                gd.SetRenderTarget(&rt);
                gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
                gd.setDepthStencilStateProperty(DepthStencilState::Default);
                for (const auto& p : pieces)
                {
                    std::vector<Matrix> full(static_cast<size_t>(SkinnedEffect::MaxBones),
                                             Matrix::getIdentityProperty());
                    const std::vector<Matrix> pal = paletteFor(*p);
                    for (size_t j = 0; j < pal.size(); ++j)
                    {
                        full[j] = pal[j];
                    }
                    fx.SetBoneTransforms(full);
                    gd.SetVertexBuffer(p->part->getVertexBufferProperty());
                    gd.setIndicesProperty(p->part->getIndexBufferProperty());
                    EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                    for (int q = 0; q < passes.getCountProperty(); ++q)
                    {
                        passes[q]->Apply();
                        gd.DrawIndexedPrimitives(PrimitiveType::TriangleList,
                                                 p->part->getVertexOffsetProperty(),
                                                 0,
                                                 p->part->getNumVerticesProperty(),
                                                 p->part->getStartIndexProperty(),
                                                 p->part->getPrimitiveCountProperty());
                    }
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(out.data(), static_cast<int>(out.size()));
            };

            std::vector<Color> pxOffline(static_cast<size_t>(kRt) * kRt);
            std::vector<Color> pxPipeline(pxOffline.size());
            renderRoute(offline, pxOffline);
            renderRoute(pipeline, pxPipeline);

            int lit = 0, differ = 0, leftLit = 0, rightLit = 0;
            for (int y = 0; y < kRt; ++y)
            {
                for (int x = 0; x < kRt; ++x)
                {
                    const size_t i = static_cast<size_t>(y) * kRt + x;
                    const bool on = pxOffline[i].getRProperty() || pxOffline[i].getGProperty() ||
                                    pxOffline[i].getBProperty();
                    if (on)
                    {
                        ++lit;
                        if (x < kRt / 2)
                        {
                            ++leftLit;
                        }
                        else
                        {
                            ++rightLit;
                        }
                    }
                    if (pxOffline[i].getRProperty() != pxPipeline[i].getRProperty() ||
                        pxOffline[i].getGProperty() != pxPipeline[i].getGProperty() ||
                        pxOffline[i].getBProperty() != pxPipeline[i].getBProperty())
                    {
                        ++differ;
                    }
                }
            }
            r.note("offline-split route coverage",
                   std::to_string(lit) + " px (" + std::to_string(leftLit) + " left, " +
                       std::to_string(rightLit) + " right)");
            r.check("both parts actually drew",
                    leftLit > 100 && rightLit > 100,
                    std::to_string(leftLit) + " / " + std::to_string(rightLit));
            r.check("the pipeline split and the offline split are pixel-identical",
                    differ == 0,
                    std::to_string(differ) + " differing pixels");

            return r.finish("p1-skinsplit");
        }
    };

} // namespace

P1_MAIN(SkinSplitProbe, "p1-skinsplit")
