// HOUSE-00075 -- animate the skinned fixture through a HAND-WRITTEN clip evaluator and
// `SkinnedEffect::SetBoneTransforms`, and confirm the deformation is real.
// HOUSE-00077 -- the `SkinnedEffect` bone-count limit: is 72 accepted and 73 rejected?
//
// The evaluator here is the shape `cnahouse::anim` will have: a clip is joint tracks of TRS
// keyframes, sampled and composed into local matrices, walked up the parent chain into absolute
// matrices, and multiplied by inverse bind matrices the PROJECT owns. Nothing reads `Model::Tag`,
// no `getSkinsEXTProperty()` is called, and no CNAEXT convenience appears -- which is the whole
// point of R-16.
//
// "Visible deformation" is not asserted from the absence of an exception. Two poses are rendered
// into a render target, read back, and compared: the moved silhouette must differ from the bind
// pose in a large fraction of pixels AND its bounding box must have moved in the direction the
// rotation predicts.
#include "p1-common.hpp"
#include "p1-fixtures/p1-static-expected.h"

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

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    constexpr int kRt = 256;

    // --- the project-owned clip evaluator, in miniature -------------------------------------------
    // One track per joint; each keyframe is a TRS at a time. This is deliberately the same shape as
    // the `.chanim` HOUSE-00166 will define, so that what is measured here is what will ship.
    struct Key
    {
        float time;
        Vector3 translation;
        Quaternion rotation;
        Vector3 scale;
    };

    struct Track
    {
        std::string joint;
        std::vector<Key> keys;
    };

    struct Clip
    {
        float duration = 0.0f;
        std::vector<Track> tracks;
    };

    Matrix Compose(const Vector3& t, const Quaternion& q, const Vector3& s)
    {
        // XNA's row-vector order: scale, then rotate, then translate.
        return Matrix::CreateScale(s) * Matrix::CreateFromQuaternion(q) * Matrix::CreateTranslation(t);
    }

    Matrix Sample(const Track& track, float time)
    {
        if (track.keys.empty())
        {
            return Matrix::getIdentityProperty();
        }
        if (time <= track.keys.front().time)
        {
            const Key& k = track.keys.front();
            return Compose(k.translation, k.rotation, k.scale);
        }
        if (time >= track.keys.back().time)
        {
            const Key& k = track.keys.back();
            return Compose(k.translation, k.rotation, k.scale);
        }
        for (size_t i = 1; i < track.keys.size(); ++i)
        {
            if (time <= track.keys[i].time)
            {
                const Key& a = track.keys[i - 1];
                const Key& b = track.keys[i];
                const float span = b.time - a.time;
                const float u = span > 0.0f ? (time - a.time) / span : 0.0f;
                return Compose(Vector3::Lerp(a.translation, b.translation, u),
                               Quaternion::Slerp(a.rotation, b.rotation, u),
                               Vector3::Lerp(a.scale, b.scale, u));
            }
        }
        return Matrix::getIdentityProperty();
    }

    struct Ink
    {
        int count = 0;
        int minX = kRt, minY = kRt, maxX = -1, maxY = -1;
    };

    Ink InkOf(const std::vector<Color>& px)
    {
        Ink ink;
        for (int y = 0; y < kRt; ++y)
        {
            for (int x = 0; x < kRt; ++x)
            {
                const Color& c = px[static_cast<size_t>(y) * kRt + x];
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

    class SkinAnimProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            GraphicsDevice& gd = getGraphicsDeviceProperty();
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            Model model = content.Load<Model>("P1Skin");
            const auto& bones = model.getBonesProperty();
            ModelMeshPart* part = model.getMeshesProperty()[0]->getMeshPartsProperty()[0];
            VertexBuffer* vb = part->getVertexBufferProperty();
            IndexBuffer* ib = part->getIndexBufferProperty();

            // The sidecar's contribution: the joint order and the inverse bind matrices. Both come
            // from the source .glb offline -- here, from the same generator constants the fixture was
            // built with -- never from a CNA extension query.
            int jointBone[8];
            Matrix inverseBind[8];
            bool resolved = true;
            for (int j = 0; j < kSkinJointCount; ++j)
            {
                ModelBone* b = bones[std::string(kSkinJointName[j])];
                if (b == nullptr)
                {
                    resolved = false;
                    break;
                }
                jointBone[j] = b->getIndexProperty();
                // authored world bind translation of joint j is (0, 2j, 0)
                inverseBind[j] =
                    Matrix::CreateTranslation(Vector3(0.0f, -2.0f * static_cast<float>(j), 0.0f));
            }
            r.check("the sidecar's joint name -> bone index map resolved", resolved);
            if (!resolved)
            {
                return r.finish("p1-skinanim");
            }

            // --- the clip: hold the bind pose at t=0, a 90-degree bend at J1 by t=1 ------------------
            Clip clip;
            clip.duration = 1.0f;
            for (int j = 0; j < kSkinJointCount; ++j)
            {
                Track t;
                t.joint = kSkinJointName[j];
                const Vector3 tr(0.0f, j == 0 ? 0.0f : 2.0f, 0.0f);
                const Quaternion bend =
                    (j == 1) ? Quaternion::CreateFromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), MathHelper::PiOver2)
                             : Quaternion::Identity;
                t.keys.push_back({0.0f, tr, Quaternion::Identity, Vector3::One});
                t.keys.push_back({1.0f, tr, bend, Vector3::One});
                clip.tracks.push_back(t);
            }

            // Evaluating the clip into the skinning palette. This is the whole runtime path.
            auto palette = [&](float time)
            {
                std::vector<Matrix> localOfBone(static_cast<size_t>(bones.getCountProperty()),
                                                Matrix::getIdentityProperty());
                for (int i = 0; i < bones.getCountProperty(); ++i)
                {
                    localOfBone[static_cast<size_t>(i)] = bones[i]->getTransformProperty();
                }
                for (size_t t = 0; t < clip.tracks.size(); ++t)
                {
                    ModelBone* b = bones[clip.tracks[t].joint];
                    if (b != nullptr)
                    {
                        localOfBone[static_cast<size_t>(b->getIndexProperty())] =
                            Sample(clip.tracks[t], time);
                    }
                }
                std::vector<Matrix> absolute(localOfBone.size(), Matrix::getIdentityProperty());
                for (int i = 0; i < bones.getCountProperty(); ++i)
                {
                    Matrix m = localOfBone[static_cast<size_t>(i)];
                    const ModelBone* p = bones[i]->getParentProperty();
                    while (p != nullptr)
                    {
                        m = m * localOfBone[static_cast<size_t>(p->getIndexProperty())];
                        p = p->getParentProperty();
                    }
                    absolute[static_cast<size_t>(i)] = m;
                }
                // The palette is indexed by SKIN-LOCAL joint index -- HOUSE-00074's measured verdict.
                std::vector<Matrix> pal(static_cast<size_t>(kSkinJointCount));
                for (int j = 0; j < kSkinJointCount; ++j)
                {
                    pal[static_cast<size_t>(j)] =
                        inverseBind[j] * absolute[static_cast<size_t>(jointBone[j])];
                }
                return pal;
            };

            const std::vector<Matrix> bind = palette(0.0f);
            const std::vector<Matrix> bent = palette(1.0f);

            r.check(
                "the bind-pose palette is the identity for every joint",
                [&]
                {
                    for (size_t j = 0; j < bind.size(); ++j)
                    {
                        const Matrix& m = bind[j];
                        if (!p1::near(m.M11, 1.0f, 1e-4f) || !p1::near(m.M22, 1.0f, 1e-4f) ||
                            !p1::near(m.M33, 1.0f, 1e-4f) || !p1::near(m.M41, 0.0f, 1e-4f) ||
                            !p1::near(m.M42, 0.0f, 1e-4f) || !p1::near(m.M43, 0.0f, 1e-4f))
                        {
                            return false;
                        }
                    }
                    return true;
                }(),
                "an authored bind pose must skin to identity, or the inverse bind matrices are wrong");

            // Analytic check of the bent palette before a single pixel is drawn: J2's skinning matrix
            // must carry the tip from (0,4,0) to (-2,2,0) -- a 90-degree turn about (0,2,0).
            const Vector3 tip =
                Vector3::Transform(Vector3(0.0f, 4.0f, 0.0f), bent[static_cast<size_t>(kSkinJointCount - 1)]);
            char tipMsg[120];
            std::snprintf(tipMsg, sizeof tipMsg, "(%g,%g,%g) want (-2,2,0)", tip.X, tip.Y, tip.Z);
            r.check("the evaluated palette moves the tip where the rotation says",
                    p1::near(tip.X, -2.0f, 1e-3f) && p1::near(tip.Y, 2.0f, 1e-3f) &&
                        p1::near(tip.Z, 0.0f, 1e-3f),
                    tipMsg);

            // --- render both poses -------------------------------------------------------------------
            RenderTarget2D rt(gd, kRt, kRt, false, SurfaceFormat::Color, DepthFormat::Depth24);
            SkinnedEffect fx(gd);
            fx.setWeightsPerVertexProperty(4);
            fx.setWorldProperty(Matrix::getIdentityProperty());
            fx.setViewProperty(
                Matrix::CreateLookAt(Vector3(0.0f, 2.0f, 12.0f), Vector3(0.0f, 2.0f, 0.0f), Vector3::Up));
            fx.setProjectionProperty(Matrix::CreateOrthographic(12.0f, 12.0f, 0.1f, 100.0f));
            fx.setDiffuseColorProperty(Vector3::One);
            // MEASURED: SkinnedEffect refuses `LightingEnabled = false` -- it throws
            // "SkinnedEffect does not support setting LightingEnabled to false.", exactly as XNA 4.0's
            // does. A flat unlit skinned draw is therefore impossible; the probe uses a full ambient
            // term instead, which is the only way to get pose-independent coverage out of it.
            {
                bool refused = false;
                std::string what;
                try
                {
                    fx.setLightingEnabledProperty(false);
                }
                catch (const std::exception& e)
                {
                    refused = true;
                    what = e.what();
                }
                r.check("SkinnedEffect refuses LightingEnabled = false, as XNA 4.0 does",
                        refused,
                        refused ? ("threw: " + what) : "it accepted false");
            }
            fx.setAmbientLightColorProperty(Vector3::One);

            std::vector<Color> pxBind(static_cast<size_t>(kRt) * kRt);
            std::vector<Color> pxBent(pxBind.size());

            auto render = [&](const std::vector<Matrix>& pal, std::vector<Color>& out)
            {
                // SkinnedEffect's palette is fixed-length; pad to MaxBones with identities, which is
                // what a game with fewer joints than the cap must do anyway.
                std::vector<Matrix> full(static_cast<size_t>(SkinnedEffect::MaxBones),
                                         Matrix::getIdentityProperty());
                for (size_t j = 0; j < pal.size(); ++j)
                {
                    full[j] = pal[j];
                }
                fx.SetBoneTransforms(full);
                gd.SetRenderTarget(&rt);
                gd.Clear(ClearOptions::Target | ClearOptions::DepthBuffer, Color::Black, 1.0f, 0);
                gd.setRasterizerStateProperty(RasterizerState::CullNone);
                gd.setDepthStencilStateProperty(DepthStencilState::Default);
                gd.SetVertexBuffer(vb);
                gd.setIndicesProperty(ib);
                EffectPassCollection& passes = fx.getCurrentTechniqueProperty()->getPassesProperty();
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    passes[p]->Apply();
                    gd.DrawIndexedPrimitives(PrimitiveType::TriangleList,
                                             part->getVertexOffsetProperty(),
                                             0,
                                             part->getNumVerticesProperty(),
                                             part->getStartIndexProperty(),
                                             part->getPrimitiveCountProperty());
                }
                gd.SetRenderTarget(nullptr);
                rt.GetData(out.data(), static_cast<int>(out.size()));
            };

            render(bind, pxBind);
            render(bent, pxBent);
            const Ink a = InkOf(pxBind);
            const Ink b = InkOf(pxBent);
            char am[160], bm[160];
            std::snprintf(
                am, sizeof am, "%d px, bbox x[%d,%d] y[%d,%d]", a.count, a.minX, a.maxX, a.minY, a.maxY);
            std::snprintf(
                bm, sizeof bm, "%d px, bbox x[%d,%d] y[%d,%d]", b.count, b.minX, b.maxX, b.minY, b.maxY);
            r.note("bind pose", am);
            r.note("bent pose", bm);

            r.check("the bind pose renders at all", a.count > 0, am);
            int differing = 0;
            for (size_t i = 0; i < pxBind.size(); ++i)
            {
                if (pxBind[i].getRProperty() != pxBent[i].getRProperty() ||
                    pxBind[i].getGProperty() != pxBent[i].getGProperty() ||
                    pxBind[i].getBProperty() != pxBent[i].getBProperty())
                {
                    ++differing;
                }
            }
            r.check("the two poses differ in a large fraction of covered pixels",
                    a.count > 0 && differing >= a.count / 2,
                    std::to_string(differing) + " differing px against a bind-pose coverage of " +
                        std::to_string(a.count));

            // The bend is about +Z by +90 degrees, which carries the ribbon's upper half toward -X:
            // the bent silhouette must extend further left than the bind pose does.
            r.check("the deformation goes the way the rotation says",
                    b.minX < a.minX - 4,
                    "bent minX " + std::to_string(b.minX) + " vs bind minX " + std::to_string(a.minX));
            // ... and it must be shorter, because the top half is now horizontal
            r.check("the bent pose is shorter than the bind pose",
                    b.minY > a.minY + 4,
                    "bent minY " + std::to_string(b.minY) + " vs bind minY " + std::to_string(a.minY));

            // --- HOUSE-00077: the bone-count limit ----------------------------------------------------
            r.check("SkinnedEffect::MaxBones is 72",
                    SkinnedEffect::MaxBones == 72,
                    std::to_string(SkinnedEffect::MaxBones));
            bool at72 = true;
            std::string what72;
            try
            {
                std::vector<Matrix> m72(72, Matrix::getIdentityProperty());
                fx.SetBoneTransforms(m72);
            }
            catch (const std::exception& e)
            {
                at72 = false;
                what72 = e.what();
            }
            r.check("SetBoneTransforms accepts exactly 72", at72, what72);

            bool threw73 = false;
            std::string what73;
            try
            {
                std::vector<Matrix> m73(73, Matrix::getIdentityProperty());
                fx.SetBoneTransforms(m73);
            }
            catch (const std::exception& e)
            {
                threw73 = true;
                what73 = e.what();
            }
            r.check("SetBoneTransforms rejects 73",
                    threw73,
                    threw73 ? ("threw: " + what73) : "it accepted 73 -- the cap is not enforced");

            return r.finish("p1-skinanim");
        }
    };

} // namespace

P1_MAIN(SkinAnimProbe, "p1-skinanim")
