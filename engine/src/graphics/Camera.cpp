#include "graphics/Camera.hpp"

#include "graphics/OrbitCamera.hpp"

namespace rubiks::graphics {

Camera default_camera(float aspect) noexcept
{
    // One definition of the home viewpoint. Building the eye here instead
    // would leave two cameras that agree only to within a rounding error, and
    // the one the application renders with would be the orbit's anyway.
    return home_orbit().to_camera(aspect);
}

}  // namespace rubiks::graphics
