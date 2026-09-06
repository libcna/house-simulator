// HOUSE-00104 -- the XNA math the room/portal system depends on, compared against ANALYTIC answers.
//
// `BoundingFrustum::GetCorners`, `BoundingFrustum::Intersects(BoundingBox)`,
// `BoundingBox::CreateFromPoints` and `Ray::Intersects` are the four calls phase 9 is built on. A
// portal system that trusts a frustum test which is subtly wrong produces rooms that pop, and the
// bug is nearly impossible to find later -- so each is checked against a number computed by hand
// here, in a frame chosen so the expected answers are exact rather than approximate.
//
// The frustum is deliberately ORTHOGRAPHIC and axis-aligned: its eight corners are then exactly the
// corners of a box whose extents are the projection parameters, with no perspective divide to
// introduce tolerance, so `GetCorners` is checkable to 1e-4 rather than "about right".
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/ContainmentType.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"

#include <optional>

using namespace Microsoft::Xna::Framework;

namespace
{

    std::string V(const Vector3& v)
    {
        char b[64];
        std::snprintf(b, sizeof b, "(%g,%g,%g)", v.X, v.Y, v.Z);
        return b;
    }

    class MathProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;

            // ================= BoundingBox::CreateFromPoints ========================================
            {
                // Deliberately unordered and with duplicates, since a naive implementation that assumed
                // the first point was the minimum would still pass on a sorted list.
                const std::vector<Vector3> points = {
                    Vector3(3.0f, -1.0f, 7.0f),
                    Vector3(-2.0f, 5.0f, -4.0f),
                    Vector3(3.0f, -1.0f, 7.0f),
                    Vector3(0.0f, 0.0f, 0.0f),
                    Vector3(-2.0f, 2.0f, 9.0f),
                    Vector3(1.5f, 5.0f, -4.0f),
                };
                const BoundingBox box = BoundingBox::CreateFromPoints(points);
                r.check("CreateFromPoints finds the true minimum",
                        box.Min.X == -2.0f && box.Min.Y == -1.0f && box.Min.Z == -4.0f,
                        V(box.Min));
                r.check("CreateFromPoints finds the true maximum",
                        box.Max.X == 3.0f && box.Max.Y == 5.0f && box.Max.Z == 9.0f,
                        V(box.Max));
                r.check("every source point is contained",
                        [&]
                        {
                            for (const Vector3& p : points)
                            {
                                if (box.Contains(p) == ContainmentType::Disjoint)
                                {
                                    return false;
                                }
                            }
                            return true;
                        }());
            }

            // ================= BoundingBox intersection ==============================================
            {
                const BoundingBox a(Vector3(0.0f, 0.0f, 0.0f), Vector3(2.0f, 2.0f, 2.0f));
                const BoundingBox inside(Vector3(0.5f, 0.5f, 0.5f), Vector3(1.5f, 1.5f, 1.5f));
                const BoundingBox overlapping(Vector3(1.0f, 1.0f, 1.0f), Vector3(3.0f, 3.0f, 3.0f));
                const BoundingBox touching(Vector3(2.0f, 0.0f, 0.0f), Vector3(4.0f, 2.0f, 2.0f));
                const BoundingBox apart(Vector3(3.0f, 3.0f, 3.0f), Vector3(4.0f, 4.0f, 4.0f));
                r.check("a fully contained box reports Contains",
                        a.Contains(inside) == ContainmentType::Contains,
                        std::to_string(static_cast<int>(a.Contains(inside))));
                r.check("a partially overlapping box reports Intersects",
                        a.Contains(overlapping) == ContainmentType::Intersects,
                        std::to_string(static_cast<int>(a.Contains(overlapping))));
                r.check("a face-touching box counts as intersecting, not disjoint",
                        a.Intersects(touching),
                        "boxes sharing exactly the plane x = 2");
                r.check("a separated box is disjoint", !a.Intersects(apart));
            }

            // ================= Ray::Intersects =======================================================
            {
                const BoundingBox box(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
                // straight at the box from 5 units away: the near face is at z = 1, so t = 4 exactly
                const Ray hit(Vector3(0.0f, 0.0f, 5.0f), Vector3(0.0f, 0.0f, -1.0f));
                std::optional<float> t = hit.Intersects(box);
                r.check("a ray aimed at the box hits it", t.has_value());
                r.check("the hit distance is the analytic one (4.0)",
                        t.has_value() && p1::near(*t, 4.0f, 1e-4f),
                        t.has_value() ? std::to_string(*t) : "no hit");

                const Ray miss(Vector3(0.0f, 5.0f, 5.0f), Vector3(0.0f, 0.0f, -1.0f));
                r.check("a ray passing above the box misses it", !miss.Intersects(box).has_value());

                // A ray STARTING inside must report 0, which is the case a door-probe gets wrong.
                const Ray inside(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
                std::optional<float> ti = inside.Intersects(box);
                r.check("a ray starting inside the box reports distance 0",
                        ti.has_value() && p1::near(*ti, 0.0f, 1e-5f),
                        ti.has_value() ? std::to_string(*ti) : "no hit");

                // A ray pointing AWAY must miss, even though the infinite line would hit.
                const Ray away(Vector3(0.0f, 0.0f, 5.0f), Vector3(0.0f, 0.0f, 1.0f));
                r.check("a ray pointing away misses, though its line would hit",
                        !away.Intersects(box).has_value());

                const BoundingSphere sphere(Vector3(0.0f, 0.0f, 0.0f), 2.0f);
                std::optional<float> ts = hit.Intersects(sphere);
                r.check("Ray::Intersects(BoundingSphere) gives the analytic distance (3.0)",
                        ts.has_value() && p1::near(*ts, 3.0f, 1e-4f),
                        ts.has_value() ? std::to_string(*ts) : "no hit");

                const Plane plane(Vector3(0.0f, 1.0f, 0.0f), 0.0f); // the y = 0 plane
                const Ray down(Vector3(0.0f, 3.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f));
                std::optional<float> tp = down.Intersects(plane);
                r.check("Ray::Intersects(Plane) gives the analytic distance (3.0)",
                        tp.has_value() && p1::near(*tp, 3.0f, 1e-4f),
                        tp.has_value() ? std::to_string(*tp) : "no hit");
            }

            // ================= BoundingFrustum =======================================================
            {
                // An orthographic frustum, axis-aligned, camera at the origin looking down -Z:
                // width 4, height 2, near 1, far 11. Its corners are therefore exactly
                // x in {-2, 2}, y in {-1, 1}, z in {-1, -11} -- with no perspective divide anywhere,
                // so every expected number below is exact.
                const Matrix view =
                    Matrix::CreateLookAt(Vector3::Zero, Vector3(0.0f, 0.0f, -1.0f), Vector3::Up);
                const Matrix projection = Matrix::CreateOrthographic(4.0f, 2.0f, 1.0f, 11.0f);
                const BoundingFrustum frustum(view * projection);

                const std::vector<Vector3> corners = frustum.GetCorners();
                r.check("GetCorners returns 8 corners", corners.size() == 8, std::to_string(corners.size()));
                float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
                for (const Vector3& c : corners)
                {
                    const float v[3] = {c.X, c.Y, c.Z};
                    for (int k = 0; k < 3; ++k)
                    {
                        lo[k] = std::fmin(lo[k], v[k]);
                        hi[k] = std::fmax(hi[k], v[k]);
                    }
                }
                char cb[180];
                std::snprintf(
                    cb, sizeof cb, "x[%g,%g] y[%g,%g] z[%g,%g]", lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
                r.note("frustum corner extents", cb);
                r.check("the corner extents are the analytic ones",
                        p1::near(lo[0], -2.0f, 1e-4f) && p1::near(hi[0], 2.0f, 1e-4f) &&
                            p1::near(lo[1], -1.0f, 1e-4f) && p1::near(hi[1], 1.0f, 1e-4f) &&
                            p1::near(lo[2], -11.0f, 1e-3f) && p1::near(hi[2], -1.0f, 1e-3f),
                        cb);

                // The four cases a portal system actually asks about.
                const BoundingBox insideBox(Vector3(-0.5f, -0.5f, -5.0f), Vector3(0.5f, 0.5f, -4.0f));
                const BoundingBox straddling(Vector3(1.0f, -0.5f, -5.0f), Vector3(5.0f, 0.5f, -4.0f));
                const BoundingBox behind(Vector3(-0.5f, -0.5f, 2.0f), Vector3(0.5f, 0.5f, 3.0f));
                const BoundingBox beyondFar(Vector3(-0.5f, -0.5f, -20.0f), Vector3(0.5f, 0.5f, -15.0f));
                r.check("a box fully inside the frustum reports Contains",
                        frustum.Contains(insideBox) == ContainmentType::Contains,
                        std::to_string(static_cast<int>(frustum.Contains(insideBox))));
                r.check("a box crossing a side plane reports Intersects",
                        frustum.Contains(straddling) == ContainmentType::Intersects,
                        std::to_string(static_cast<int>(frustum.Contains(straddling))));
                r.check("a box behind the camera is Disjoint",
                        frustum.Contains(behind) == ContainmentType::Disjoint,
                        std::to_string(static_cast<int>(frustum.Contains(behind))));
                r.check("a box beyond the far plane is Disjoint",
                        frustum.Contains(beyondFar) == ContainmentType::Disjoint,
                        std::to_string(static_cast<int>(frustum.Contains(beyondFar))));
                r.check("Intersects(BoundingBox) agrees with Contains on every one of the four",
                        frustum.Intersects(insideBox) && frustum.Intersects(straddling) &&
                            !frustum.Intersects(behind) && !frustum.Intersects(beyondFar));

                // A sphere exactly touching the right plane: the case an epsilon error flips.
                const BoundingSphere grazing(Vector3(3.0f, 0.0f, -5.0f), 1.0f);
                r.check("a sphere tangent to a side plane counts as intersecting",
                        frustum.Intersects(grazing),
                        "centre x = 3, radius 1, plane at x = 2");
            }

            // ================= Matrix and Quaternion sanity ==========================================
            {
                const Matrix m = Matrix::CreateRotationY(MathHelper::PiOver2);
                const Vector3 v = Vector3::Transform(Vector3(0.0f, 0.0f, -1.0f), m);
                r.check("a +90 degree turn about +Y carries -Z to -X",
                        p1::near(v.X, -1.0f, 1e-5f) && p1::near(v.Y, 0.0f, 1e-5f) &&
                            p1::near(v.Z, 0.0f, 1e-5f),
                        V(v));
                const Quaternion q =
                    Quaternion::CreateFromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), MathHelper::PiOver2);
                const Vector3 vq =
                    Vector3::Transform(Vector3(0.0f, 0.0f, -1.0f), Matrix::CreateFromQuaternion(q));
                r.check("the quaternion route agrees with the matrix route",
                        p1::near(vq.X, v.X, 1e-5f) && p1::near(vq.Y, v.Y, 1e-5f) &&
                            p1::near(vq.Z, v.Z, 1e-5f),
                        V(vq));
            }

            return r.finish("p1-math");
        }
    };

} // namespace

P1_MAIN(MathProbe, "p1-math")
