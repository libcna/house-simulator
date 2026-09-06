// HOUSE-00074 -- skinned glTF -> .cnb -> Model, and whether a PROJECT-OWNED sidecar can bind to it.
//
// This settles R-16. The question is not "does a skinned model load" but "can `cna-house` recover
// the joint mapping its own `.chanim` needs **without** reading `Model::Tag`, without
// `getSkinsEXTProperty()`, and without any CNAEXT convenience call". Nothing in this file touches
// any of those.
//
// The decisive measurement: the fixture binds every vertex to a joint the generator chose, so the
// blend index in the compiled buffer can be tested against BOTH hypotheses at once --
//   (a) it indexes `Model::Bones` directly, or
//   (b) it indexes the skin's own joint list, 0..N-1 in the order glTF declared them.
// HOUSE-00072 found CNA inserts a synthetic `Root` bone, so those two differ by at least one and
// the fixture tells them apart on the first run.
#include "p1-common.hpp"
#include "p1-fixtures/p1-static-expected.h"

#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace
{

    // Decodes one element out of the raw vertex bytes according to the measured declaration.
    struct Decoder
    {
        const std::vector<unsigned char>* bytes = nullptr;
        int stride = 0;

        [[nodiscard]] const unsigned char* at(int vertex, int offset) const
        {
            return bytes->data() + static_cast<size_t>(vertex) * static_cast<size_t>(stride) + offset;
        }

        [[nodiscard]] float f32(int vertex, int offset) const
        {
            float v;
            std::memcpy(&v, at(vertex, offset), sizeof v);
            return v;
        }

        // Blend indices arrive as one of the small integer formats; decode all of them rather than
        // guessing, and report which one was actually there.
        [[nodiscard]] bool indices4(int vertex, int offset, VertexElementFormat fmt, int out[4]) const
        {
            const unsigned char* p = at(vertex, offset);
            switch (fmt)
            {
                case VertexElementFormat::Byte4:
                case VertexElementFormat::Color:
                    for (int k = 0; k < 4; ++k)
                    {
                        out[k] = p[k];
                    }
                    return true;
                case VertexElementFormat::Short4:
                case VertexElementFormat::NormalizedShort4:
                {
                    std::uint16_t s[4];
                    std::memcpy(s, p, sizeof s);
                    for (int k = 0; k < 4; ++k)
                    {
                        out[k] = s[k];
                    }
                    return true;
                }
                case VertexElementFormat::Vector4:
                {
                    float v[4];
                    std::memcpy(v, p, sizeof v);
                    for (int k = 0; k < 4; ++k)
                    {
                        out[k] = static_cast<int>(v[k] + 0.5f);
                    }
                    return true;
                }
                default:
                    return false;
            }
        }

        [[nodiscard]] bool weights4(int vertex, int offset, VertexElementFormat fmt, float out[4]) const
        {
            const unsigned char* p = at(vertex, offset);
            switch (fmt)
            {
                case VertexElementFormat::Vector4:
                    std::memcpy(out, p, 4 * sizeof(float));
                    return true;
                case VertexElementFormat::Color:
                case VertexElementFormat::Byte4:
                    for (int k = 0; k < 4; ++k)
                    {
                        out[k] = static_cast<float>(p[k]) / 255.0f;
                    }
                    return true;
                default:
                    return false;
            }
        }
    };

    class SkinProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            Model model = content.Load<Model>("P1Skin");
            const auto& bones = model.getBonesProperty();
            const auto& meshes = model.getMeshesProperty();
            r.note("bone count", std::to_string(bones.getCountProperty()));
            r.note("mesh count", std::to_string(meshes.getCountProperty()));
            for (int i = 0; i < bones.getCountProperty(); ++i)
            {
                r.note(("bone[" + std::to_string(i) + "]").c_str(),
                       bones[i]->getNameProperty() + " parent=" +
                           (bones[i]->getParentProperty() ? bones[i]->getParentProperty()->getNameProperty()
                                                          : "<none>"));
            }

            // ---- acceptance (1): every glTF skin joint has a same-named Model::Bones entry ----------
            int jointBone[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
            int named = 0;
            for (int j = 0; j < kSkinJointCount; ++j)
            {
                ModelBone* b = bones[std::string(kSkinJointName[j])];
                if (b != nullptr)
                {
                    jointBone[j] = b->getIndexProperty();
                    ++named;
                }
                r.check((std::string("joint ") + kSkinJointName[j] + " has a same-named bone").c_str(),
                        b != nullptr,
                        b ? ("Bones[" + std::to_string(b->getIndexProperty()) + "]") : "absent");
            }
            r.check("all skin joints are name-resolvable in Model::Bones",
                    named == kSkinJointCount,
                    std::to_string(named) + "/" + std::to_string(kSkinJointCount));

            // ---- acceptance (3): parents and transforms agree with the source hierarchy -------------
            for (int j = 0; j < kSkinJointCount && named == kSkinJointCount; ++j)
            {
                ModelBone* b = bones[std::string(kSkinJointName[j])];
                const std::string got =
                    b->getParentProperty() ? b->getParentProperty()->getNameProperty() : "<none>";
                const std::string want = kSkinJointParent[j] < 0
                                             ? std::string("P1SkinRoot")
                                             : std::string(kSkinJointName[kSkinJointParent[j]]);
                r.check((std::string(kSkinJointName[j]) + ": parent link").c_str(),
                        got == want,
                        got + " want " + want);
                // authored local translations: J0 (0,0,0), J1 and J2 (0,2,0)
                const Matrix& m = b->getTransformProperty();
                const float wantY = (j == 0) ? 0.0f : 2.0f;
                r.check((std::string(kSkinJointName[j]) + ": local translation").c_str(),
                        p1::near(m.M41, 0.0f) && p1::near(m.M42, wantY) && p1::near(m.M43, 0.0f),
                        "(" + std::to_string(m.M41) + "," + std::to_string(m.M42) + "," +
                            std::to_string(m.M43) + ") want (0," + std::to_string(wantY) + ",0)");
            }

            // ---- the vertex layout, and what the blend indices actually mean --------------------------
            r.check("exactly one skinned mesh",
                    meshes.getCountProperty() == 1,
                    std::to_string(meshes.getCountProperty()));
            if (meshes.getCountProperty() < 1)
            {
                return r.finish("p1-skin");
            }
            ModelMeshPart* part = meshes[0]->getMeshPartsProperty()[0];
            VertexBuffer* vb = part->getVertexBufferProperty();
            const VertexDeclaration& decl = vb->getVertexDeclarationProperty();
            r.note("vertex declaration", p1::DescribeDeclaration(decl));

            VertexElementFormat jf{}, wf{}, pf{};
            int jo = 0, wo = 0, po = 0;
            const bool hasJ = p1::FindElement(decl, VertexElementUsage::BlendIndices, 0, jf, jo);
            const bool hasW = p1::FindElement(decl, VertexElementUsage::BlendWeight, 0, wf, wo);
            const bool hasP = p1::FindElement(decl, VertexElementUsage::Position, 0, pf, po);
            r.check("the declaration carries BlendIndices",
                    hasJ,
                    hasJ ? ("offset " + std::to_string(jo)) : "absent -- skinning did not survive");
            r.check("the declaration carries BlendWeight",
                    hasW,
                    hasW ? ("offset " + std::to_string(wo)) : "absent");
            if (!hasJ || !hasW || !hasP)
            {
                return r.finish("p1-skin");
            }

            std::vector<unsigned char> raw;
            const int count = part->getNumVerticesProperty();
            r.check("vertex count matches the source .glb",
                    count == kSkinVertexCount,
                    std::to_string(count) + " vs " + std::to_string(kSkinVertexCount));
            r.check("the vertex buffer could be read back at its measured stride",
                    p1::ReadVertexBytes(vb, count, decl.getVertexStrideProperty(), raw),
                    "stride " + std::to_string(decl.getVertexStrideProperty()));
            if (raw.empty())
            {
                return r.finish("p1-skin");
            }
            Decoder dec{&raw, decl.getVertexStrideProperty()};

            // Vertices may be reordered by the processor, so each one is matched to its authored
            // counterpart by POSITION -- never by array index.
            int matched = 0, boneHyp = 0, skinHyp = 0, weightOk = 0, decoded = 0;
            std::string firstMismatch;
            for (int v = 0; v < count; ++v)
            {
                const float x = dec.f32(v, po), y = dec.f32(v, po + 4), z = dec.f32(v, po + 8);
                int src = -1;
                for (int k = 0; k < kSkinVertexCount; ++k)
                {
                    // the generator's own layout: vertex k sits at (-0.5 + col, row, 0)
                    const float sx = -0.5f + static_cast<float>(k % 2);
                    const float sy = static_cast<float>(k / 2);
                    if (p1::near(x, sx) && p1::near(y, sy) && p1::near(z, 0.0f))
                    {
                        src = k;
                        break;
                    }
                }
                if (src < 0)
                {
                    if (firstMismatch.empty())
                    {
                        char b[128];
                        std::snprintf(
                            b, sizeof b, "v%d at (%g,%g,%g) matches no authored vertex", v, x, y, z);
                        firstMismatch = b;
                    }
                    continue;
                }
                ++matched;

                int ji[4];
                float wv[4];
                if (!dec.indices4(v, jo, jf, ji) || !dec.weights4(v, wo, wf, wv))
                {
                    continue;
                }
                ++decoded;

                // Weights are order-insensitive here only in the sense that a 50/50 blend is symmetric;
                // compare as a multiset keyed by the joint the index resolves to under each hypothesis.
                bool bOk = true, sOk = true, wOk = true;
                for (int k = 0; k < 4; ++k)
                {
                    const float want = kSkinVertexWeight[src][k];
                    if (!p1::near(wv[k], want, 2e-3f))
                    {
                        wOk = false;
                    }
                    if (want <= 0.0f)
                    {
                        continue;
                    }
                    const int authored = kSkinVertexJoint[src][k];
                    if (ji[k] != jointBone[authored])
                    {
                        bOk = false;
                    } // hypothesis (a)
                    if (ji[k] != authored)
                    {
                        sOk = false;
                    } // hypothesis (b)
                }
                if (bOk)
                {
                    ++boneHyp;
                }
                if (sOk)
                {
                    ++skinHyp;
                }
                if (wOk)
                {
                    ++weightOk;
                }
            }
            r.check("every compiled vertex matches an authored position",
                    matched == count,
                    std::to_string(matched) + "/" + std::to_string(count) + " " + firstMismatch);
            r.check("blend indices and weights decoded in their declared formats",
                    decoded == matched,
                    std::to_string(decoded) + "/" + std::to_string(matched));
            r.check("blend weights equal the authored weights",
                    weightOk == matched,
                    std::to_string(weightOk) + "/" + std::to_string(matched));

            char hyp[200];
            std::snprintf(hyp,
                          sizeof hyp,
                          "indexes Model::Bones on %d/%d vertices; indexes the skin joint list on %d/%d",
                          boneHyp,
                          matched,
                          skinHyp,
                          matched);
            r.note("blend-index meaning", hyp);
            r.check("the blend index resolves under exactly one hypothesis",
                    (boneHyp == matched) != (skinHyp == matched),
                    hyp);

            const bool indexesBones = (boneHyp == matched);
            r.note("VERDICT",
                   indexesBones ? "blend indices are Model::Bones indices -- a sidecar binds by bone index"
                                : "blend indices are SKIN-LOCAL -- a sidecar MUST carry an explicit "
                                  "joint-name -> bone-index map");

            // ---- acceptance (2): the mapping is recoverable and stable, WITHOUT Model::Tag -----------
            // Recoverable: the joint order is exactly the order that reproduces the authored binding.
            // The probe reconstructs it from names alone, which is all a sidecar would ever carry.
            std::vector<int> recovered(static_cast<size_t>(kSkinJointCount), -1);
            for (int j = 0; j < kSkinJointCount; ++j)
            {
                ModelBone* b = bones[std::string(kSkinJointName[j])];
                recovered[static_cast<size_t>(j)] = b ? b->getIndexProperty() : -1;
            }
            std::string map;
            for (int j = 0; j < kSkinJointCount; ++j)
            {
                map += std::string(j ? " " : "") + kSkinJointName[j] + "->" +
                       std::to_string(recovered[static_cast<size_t>(j)]);
            }
            r.note("name -> bone-index map a sidecar would carry", map);
            r.check(
                "the map is total and injective",
                [&]
                {
                    for (size_t a = 0; a < recovered.size(); ++a)
                    {
                        if (recovered[a] < 0)
                        {
                            return false;
                        }
                        for (size_t b = a + 1; b < recovered.size(); ++b)
                        {
                            if (recovered[a] == recovered[b])
                            {
                                return false;
                            }
                        }
                    }
                    return true;
                }(),
                map);

            return r.finish("p1-skin");
        }
    };

} // namespace

P1_MAIN(SkinProbe, "p1-skin")
