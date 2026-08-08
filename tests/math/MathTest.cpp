#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "math/Projection.hpp"
#include "math/Quaternion.hpp"
#include "math/Transform.hpp"
#include "math/Types.hpp"

// This target links neither ThorVG nor the renderer, so the conventions below
// are provable without a graphics backend.

namespace {

using Catch::Approx;
using namespace rubiks::math;

constexpr float kPi = 3.14159265358979323846f;
constexpr double kDefaultMargin = 1e-5;
constexpr double kInverseProjectionMargin = 1e-4;

/** Requires a Vec3 to match component-wise within an absolute margin. */
void require_vec3(const Vec3& actual, float x, float y, float z,
                  double margin = kDefaultMargin)
{
    REQUIRE(actual.x == Approx(x).margin(margin));
    REQUIRE(actual.y == Approx(y).margin(margin));
    REQUIRE(actual.z == Approx(z).margin(margin));
}

/** Requires a Vec4 to match component-wise within an absolute margin. */
void require_vec4(const Vec4& actual, float x, float y, float z, float w,
                  double margin = kDefaultMargin)
{
    REQUIRE(actual.x == Approx(x).margin(margin));
    REQUIRE(actual.y == Approx(y).margin(margin));
    REQUIRE(actual.z == Approx(z).margin(margin));
    REQUIRE(actual.w == Approx(w).margin(margin));
}

}  // namespace

TEST_CASE("matrices are column-major and multiply column vectors")
{
    // Columns as written: the fourth one carries the translation.
    const Mat4 translation{
        Vec4{1.0f, 0.0f, 0.0f, 0.0f},
        Vec4{0.0f, 1.0f, 0.0f, 0.0f},
        Vec4{0.0f, 0.0f, 1.0f, 0.0f},
        Vec4{7.0f, 8.0f, 9.0f, 1.0f},
    };

    SECTION("storage exposes columns, not rows")
    {
        require_vec4(translation.w, 7.0f, 8.0f, 9.0f, 1.0f);
    }

    SECTION("apply treats the vector as a column")
    {
        require_vec4(apply_point(translation, Vec3{1.0f, 2.0f, 3.0f}), 8.0f,
                     10.0f, 12.0f, 1.0f);
    }

    SECTION("multiply(a, b) applies b first")
    {
        const Mat4 scale{
            Vec4{2.0f, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, 2.0f, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, 2.0f, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f},
        };

        // Scale then translate leaves the translation untouched.
        require_vec4(apply_point(multiply(translation, scale),
                                 Vec3{1.0f, 1.0f, 1.0f}),
                     9.0f, 10.0f, 11.0f, 1.0f);

        // Translate then scale multiplies the translation as well.
        require_vec4(apply_point(multiply(scale, translation),
                                 Vec3{1.0f, 1.0f, 1.0f}),
                     16.0f, 18.0f, 20.0f, 1.0f);
    }
}

TEST_CASE("look_at puts the camera at the origin looking down -Z")
{
    const Mat4 view =
        look_at(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
                Vec3{0.0f, 1.0f, 0.0f});

    SECTION("the eye maps to the origin")
    {
        require_vec4(apply_point(view, Vec3{0.0f, 0.0f, 5.0f}), 0.0f, 0.0f,
                     0.0f, 1.0f);
    }

    SECTION("the target sits on -Z at the eye distance")
    {
        require_vec4(apply_point(view, Vec3{0.0f, 0.0f, 0.0f}), 0.0f, 0.0f,
                     -5.0f, 1.0f);
    }

    SECTION("world axes keep their handedness")
    {
        require_vec4(apply_point(view, Vec3{1.0f, 2.0f, 3.0f}), 1.0f, 2.0f,
                     -2.0f, 1.0f);
    }

    SECTION("a diagonal eye still lands the target on -Z")
    {
        const Mat4 diagonal =
            look_at(Vec3{3.0f, 3.0f, 3.0f}, Vec3{0.0f, 0.0f, 0.0f},
                    Vec3{0.0f, 1.0f, 0.0f});

        // Distance from (3, 3, 3) to the origin is 3 * sqrt(3).
        require_vec4(apply_point(diagonal, Vec3{0.0f, 0.0f, 0.0f}), 0.0f, 0.0f,
                     -3.0f * std::sqrt(3.0f), 1.0f);
    }
}

TEST_CASE("perspective maps the frustum onto the NDC cube")
{
    // 90 degrees vertical, square aspect: the frustum half-extent equals the
    // depth, so a point at (1, 1, -1) lands exactly on the NDC corner.
    const Mat4 projection = perspective(kPi / 2.0f, 1.0f, 1.0f, 3.0f);

    SECTION("clip w is the positive view-space depth")
    {
        const Vec4 clip = apply_point(projection, Vec3{0.0f, 0.0f, -2.0f});
        REQUIRE(clip.w == Approx(2.0f).margin(1e-5));
    }

    SECTION("the near plane maps to -1 and the far plane to +1")
    {
        const Vec4 near_clip = apply_point(projection, Vec3{0.0f, 0.0f, -1.0f});
        REQUIRE(near_clip.z / near_clip.w == Approx(-1.0f).margin(1e-5));

        const Vec4 far_clip = apply_point(projection, Vec3{0.0f, 0.0f, -3.0f});
        REQUIRE(far_clip.z / far_clip.w == Approx(1.0f).margin(1e-5));
    }

    SECTION("the frustum corner maps to the NDC corner with +Y up")
    {
        const Vec4 clip = apply_point(projection, Vec3{1.0f, 1.0f, -1.0f});
        REQUIRE(clip.x / clip.w == Approx(1.0f).margin(1e-5));
        REQUIRE(clip.y / clip.w == Approx(1.0f).margin(1e-5));
    }

    SECTION("aspect widens the horizontal extent only")
    {
        const Mat4 wide = perspective(kPi / 2.0f, 2.0f, 1.0f, 3.0f);
        const Vec4 clip = apply_point(wide, Vec3{2.0f, 1.0f, -1.0f});
        REQUIRE(clip.x / clip.w == Approx(1.0f).margin(1e-5));
        REQUIRE(clip.y / clip.w == Approx(1.0f).margin(1e-5));
    }
}

TEST_CASE("inverse undoes a projection times a view")
{
    // The combination pointer picking has to invert to turn a point on the
    // screen back into a ray through the scene.
    const Mat4 view =
        look_at(Vec3{3.0f, 4.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
                Vec3{0.0f, 1.0f, 0.0f});
    const Mat4 projection = perspective(kPi / 3.0f, 1.5f, 0.1f, 100.0f);
    const Mat4 combined = multiply(projection, view);

    SECTION("the product with the original is the identity")
    {
        const Mat4 product = multiply(inverse(combined), combined);
        const Mat4 expected = identity();

        // A wider margin than the rest of this file: a projection spanning
        // near 0.1 to far 100 is poorly conditioned in z, so single precision
        // leaves residue in the fifth decimal.
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                REQUIRE(product[column][row] ==
                        Approx(expected[column][row]).margin(1e-4));
            }
        }
    }

    SECTION("a projected point comes back where it started")
    {
        const Vec3 point{0.25f, -0.5f, 0.75f};
        const Vec4 clip = apply_point(combined, point);
        const Vec4 ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w, 1.0f};

        // Qualified: unqualified apply() would find linalg's own elementwise
        // apply through argument-dependent lookup.
        const Vec4 restored = rubiks::math::apply(inverse(combined), ndc);
        require_vec3(Vec3{restored.x / restored.w, restored.y / restored.w,
                          restored.z / restored.w},
                     point.x, point.y, point.z, kInverseProjectionMargin);
    }
}

TEST_CASE("quaternion rotation agrees with its matrix")
{
    const Quaternion turn =
        quaternion_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    const Vec3 point{1.0f, 0.0f, 0.0f};

    SECTION("a quarter turn about +Z sends +X to +Y")
    {
        require_vec3(quaternion_rotate(turn, point), 0.0f, 1.0f, 0.0f);
    }

    SECTION("the matrix form produces the same point")
    {
        const Vec4 rotated = apply_point(quaternion_to_matrix(turn), point);
        require_vec4(rotated, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    SECTION("composition applies the right-hand rotation first")
    {
        const Quaternion about_x =
            quaternion_from_axis_angle(Vec3{1.0f, 0.0f, 0.0f}, kPi / 2.0f);

        // About X first sends +Y to +Z; about Z then leaves +Z alone.
        const Quaternion combined = quaternion_multiply(turn, about_x);
        require_vec3(quaternion_rotate(combined, Vec3{0.0f, 1.0f, 0.0f}), 0.0f,
                     0.0f, 1.0f);
    }

    SECTION("the identity rotation leaves points alone")
    {
        require_vec3(quaternion_rotate(quaternion_identity(), point), 1.0f,
                     0.0f, 0.0f);
    }
}

TEST_CASE("Transform composes as translation, then rotation, then scale")
{
    Transform transform;
    transform.translation = Vec3{10.0f, 0.0f, 0.0f};
    transform.rotation =
        quaternion_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, kPi / 2.0f);
    transform.scale = 2.0f;

    // (1, 0, 0) scales to (2, 0, 0), rotates to (0, 2, 0), moves to (10, 2, 0).
    require_vec4(apply_point(transform.to_matrix(), Vec3{1.0f, 0.0f, 0.0f}),
                 10.0f, 2.0f, 0.0f, 1.0f);

    SECTION("a default Transform is the identity")
    {
        const Transform identity_transform;
        require_vec4(
            apply_point(identity_transform.to_matrix(), Vec3{1.0f, 2.0f, 3.0f}),
            1.0f, 2.0f, 3.0f, 1.0f);
    }
}
