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

void require_vec3(const Vec3& actual, float x, float y, float z)
{
    REQUIRE(actual.x == Approx(x).margin(1e-5));
    REQUIRE(actual.y == Approx(y).margin(1e-5));
    REQUIRE(actual.z == Approx(z).margin(1e-5));
}

void require_vec4(const Vec4& actual, float x, float y, float z, float w)
{
    REQUIRE(actual.x == Approx(x).margin(1e-5));
    REQUIRE(actual.y == Approx(y).margin(1e-5));
    REQUIRE(actual.z == Approx(z).margin(1e-5));
    REQUIRE(actual.w == Approx(w).margin(1e-5));
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
