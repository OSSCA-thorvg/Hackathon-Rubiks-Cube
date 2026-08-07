#pragma once

#include "math/Quaternion.hpp"
#include "math/Types.hpp"

namespace rubiks::math {

/**
 * Rigid transform with uniform scale.
 *
 * to_matrix() composes as translation * rotation * scale, so a point is
 * scaled first, then rotated, then moved.
 */
struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quaternion rotation = quaternion_identity();
    float scale = 1.0f;

    [[nodiscard]] Mat4 to_matrix() const noexcept
    {
        Mat4 matrix = quaternion_to_matrix(rotation);
        matrix.x *= scale;
        matrix.y *= scale;
        matrix.z *= scale;
        matrix.w = Vec4{translation, 1.0f};
        return matrix;
    }
};

}  // namespace rubiks::math
