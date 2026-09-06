// HOUSE-00072 -- a real multi-bone hierarchy from .cnb: Bones, ParentBone, Root and
// CopyAbsoluteBoneTransformsTo, each compared against a number computed offline rather than
// against another CNA call.
//
// The fixture has depth three, a sibling branch, a 90-degree rotation that does not commute with
// its translation, a non-uniform scale, and one node authored as an explicit glTF `matrix` -- the
// single place CNA's importer converts a layout at all. Any of the classic errors (a transpose, a
// reversed multiplication order, a parent link read from the wrong node) moves a number this probe
// checks by name.
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

    void Row(const Matrix& m, float out[16])
    {
        out[0] = m.M11;
        out[1] = m.M12;
        out[2] = m.M13;
        out[3] = m.M14;
        out[4] = m.M21;
        out[5] = m.M22;
        out[6] = m.M23;
        out[7] = m.M24;
        out[8] = m.M31;
        out[9] = m.M32;
        out[10] = m.M33;
        out[11] = m.M34;
        out[12] = m.M41;
        out[13] = m.M42;
        out[14] = m.M43;
        out[15] = m.M44;
    }

    std::string Fmt(const float m[16])
    {
        char b[400];
        std::snprintf(b,
                      sizeof b,
                      "[%g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g]",
                      m[0],
                      m[1],
                      m[2],
                      m[3],
                      m[4],
                      m[5],
                      m[6],
                      m[7],
                      m[8],
                      m[9],
                      m[10],
                      m[11],
                      m[12],
                      m[13],
                      m[14],
                      m[15]);
        return b;
    }

    // A comparison that also reports HOW it differs, because "a transpose" and "a wrong order" need
    // different fixes and a bare pass/fail hides which one happened.
    bool Same(const float a[16], const float b[16], float eps = 2e-5f)
    {
        for (int i = 0; i < 16; ++i)
        {
            if (std::fabs(a[i] - b[i]) > eps)
            {
                return false;
            }
        }
        return true;
    }

    bool IsTransposeOf(const float a[16], const float b[16], float eps = 2e-5f)
    {
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                if (std::fabs(a[4 * r + c] - b[4 * c + r]) > eps)
                {
                    return false;
                }
            }
        }
        return true;
    }

    class HierProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            Model model = content.Load<Model>("P1Hier");
            const auto& bones = model.getBonesProperty();
            const auto& meshes = model.getMeshesProperty();
            r.note("bone count", std::to_string(bones.getCountProperty()));
            r.note("mesh count", std::to_string(meshes.getCountProperty()));

            for (int i = 0; i < bones.getCountProperty(); ++i)
            {
                const ModelBone* b = bones[i];
                float m[16];
                Row(b->getTransformProperty(), m);
                r.note(("bone[" + std::to_string(i) + "]").c_str(),
                       b->getNameProperty() + " index=" + std::to_string(b->getIndexProperty()) + " parent=" +
                           (b->getParentProperty() ? b->getParentProperty()->getNameProperty() : "<none>") +
                           " children=" + std::to_string(b->getChildrenProperty().getCountProperty()) + " " +
                           Fmt(m));
            }

            // ---- Root ------------------------------------------------------------------------------
            ModelBone* root = model.getRootProperty();
            r.check("Root is non-null", root != nullptr);
            if (root == nullptr)
            {
                return r.finish("p1-hier");
            }
            r.check("Root has no parent", root->getParentProperty() == nullptr, root->getNameProperty());
            r.check("every bone reaches Root by following Parent",
                    [&]
                    {
                        for (int i = 0; i < bones.getCountProperty(); ++i)
                        {
                            const ModelBone* b = bones[i];
                            int guard = 0;
                            while (b->getParentProperty() != nullptr && guard++ < 64)
                            {
                                b = b->getParentProperty();
                            }
                            if (b != root)
                            {
                                return false;
                            }
                        }
                        return true;
                    }());
            r.check("Bones[i]->Index equals i",
                    [&]
                    {
                        for (int i = 0; i < bones.getCountProperty(); ++i)
                        {
                            if (bones[i]->getIndexProperty() != i)
                            {
                                return false;
                            }
                        }
                        return true;
                    }());

            // ---- every authored node became a bone, with the authored parent -------------------------
            int found = 0;
            for (int n = 0; n < kHierCount; ++n)
            {
                ModelBone* b = bones[std::string(kHierName[n])];
                if (b == nullptr)
                {
                    r.check(kHierName[n], false, "no bone of this name");
                    continue;
                }
                ++found;
                const std::string wantParent =
                    kHierParent[n] < 0 ? root->getNameProperty() : std::string(kHierName[kHierParent[n]]);
                const std::string gotParent =
                    b->getParentProperty() ? b->getParentProperty()->getNameProperty() : "<none>";
                r.check((std::string(kHierName[n]) + ": parent link").c_str(),
                        gotParent == wantParent,
                        gotParent + " want " + wantParent);

                float got[16];
                Row(b->getTransformProperty(), got);
                const bool same = Same(got, kHierLocal[n]);
                r.check((std::string(kHierName[n]) + ": local Transform").c_str(),
                        same,
                        same ? ""
                             : (IsTransposeOf(got, kHierLocal[n])
                                    ? "TRANSPOSED: " + Fmt(got) + " want " + Fmt(kHierLocal[n])
                                    : Fmt(got) + " want " + Fmt(kHierLocal[n])));
            }
            r.check("all four authored nodes survived the import",
                    found == kHierCount,
                    std::to_string(found) + "/" + std::to_string(kHierCount));

            // ---- CopyAbsoluteBoneTransformsTo --------------------------------------------------------
            // XNA 4.0's CopyAbsoluteBoneTransformsTo requires a destination already at least
            // Bones.Count long and throws otherwise; CNA enforces the same contract. Measured, not
            // assumed: the unsized call throws with the parameter name as its message.
            std::vector<Matrix> unsized;
            bool unsizedThrew = false;
            std::string unsizedWhat;
            try
            {
                model.CopyAbsoluteBoneTransformsTo(unsized);
            }
            catch (const std::exception& e)
            {
                unsizedThrew = true;
                unsizedWhat = e.what();
            }
            r.check("an undersized destination is rejected, not silently grown",
                    unsizedThrew,
                    unsizedThrew ? ("threw: " + unsizedWhat) : "it resized itself");

            std::vector<Matrix> absolute(static_cast<size_t>(bones.getCountProperty()));
            model.CopyAbsoluteBoneTransformsTo(absolute);
            r.check("CopyAbsoluteBoneTransformsTo filled one matrix per bone",
                    static_cast<int>(absolute.size()) == bones.getCountProperty(),
                    std::to_string(absolute.size()) + " vs " + std::to_string(bones.getCountProperty()));

            for (int n = 0; n < kHierCount && static_cast<int>(absolute.size()) == bones.getCountProperty();
                 ++n)
            {
                ModelBone* b = bones[std::string(kHierName[n])];
                if (b == nullptr)
                {
                    continue;
                }
                float got[16];
                Row(absolute[static_cast<size_t>(b->getIndexProperty())], got);
                const bool same = Same(got, kHierAbsolute[n]);
                std::string how;
                if (!same)
                {
                    how = IsTransposeOf(got, kHierAbsolute[n]) ? "TRANSPOSED: " : "";
                    how += Fmt(got) + " want " + Fmt(kHierAbsolute[n]);
                }
                r.check((std::string(kHierName[n]) + ": absolute transform").c_str(), same, how);
            }

            // ---- CopyBoneTransformsTo / From round trip ----------------------------------------------
            std::vector<Matrix> locals(static_cast<size_t>(bones.getCountProperty()));
            model.CopyBoneTransformsTo(locals);
            r.check("CopyBoneTransformsTo agrees with Bones[i]->Transform",
                    [&]
                    {
                        if (static_cast<int>(locals.size()) != bones.getCountProperty())
                        {
                            return false;
                        }
                        for (int i = 0; i < bones.getCountProperty(); ++i)
                        {
                            float a[16], c[16];
                            Row(locals[static_cast<size_t>(i)], a);
                            Row(bones[i]->getTransformProperty(), c);
                            if (!Same(a, c))
                            {
                                return false;
                            }
                        }
                        return true;
                    }());
            model.CopyBoneTransformsFrom(locals);
            std::vector<Matrix> absolute2(static_cast<size_t>(bones.getCountProperty()));
            model.CopyAbsoluteBoneTransformsTo(absolute2);
            r.check("a Transforms From/To round trip changes nothing",
                    [&]
                    {
                        if (absolute.size() != absolute2.size())
                        {
                            return false;
                        }
                        for (size_t i = 0; i < absolute.size(); ++i)
                        {
                            float a[16], c[16];
                            Row(absolute[i], a);
                            Row(absolute2[i], c);
                            if (!Same(a, c))
                            {
                                return false;
                            }
                        }
                        return true;
                    }());

            // ---- meshes are attached to the bones that carried them ----------------------------------
            for (int i = 0; i < meshes.getCountProperty(); ++i)
            {
                ModelMesh* mesh = meshes[i];
                r.note(("mesh[" + std::to_string(i) + "]").c_str(),
                       mesh->getNameProperty() + " parentBone=" +
                           (mesh->getParentBoneProperty() ? mesh->getParentBoneProperty()->getNameProperty()
                                                          : "<none>"));
            }
            r.check("every mesh has a ParentBone",
                    [&]
                    {
                        for (int i = 0; i < meshes.getCountProperty(); ++i)
                        {
                            if (meshes[i]->getParentBoneProperty() == nullptr)
                            {
                                return false;
                            }
                        }
                        return meshes.getCountProperty() > 0;
                    }());

            return r.finish("p1-hier");
        }
    };

} // namespace

P1_MAIN(HierProbe, "p1-hier")
