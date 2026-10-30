#include "app/Application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "app/ApplicationState.hpp"
#include "cube/Cube.hpp"
#include "graphics/3d/Light.hpp"
#include "graphics/CanvasTheme.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"

namespace rubiks::app {

using namespace detail;

namespace {


/**
 * The bounds a speed multiplier is held inside.
 *
 * Four times either way. Faster than that and a quarter turn is over inside
 * two frames, so a sequence stops being an animation and becomes a slideshow;
 * slower and a scramble outlasts anyone's patience for watching it.
 */
constexpr float kMinSpeedScale = 0.25f;
constexpr float kMaxSpeedScale = 4.0f;

}  // namespace

bool set_cube_size(int size) noexcept
{
    if (!state || !valid_cube_size(size)) return false;

    // Already this size, so nothing to do and nothing to throw away. The same
    // answer the view commands give, and for the same reason: asking for what
    // is there is not a command.
    if (size == size_of_cube()) return true;

    // A reset onto a cube of the new size: the record, the draft and whatever
    // was playing go the way they do for a reset, and nothing else does.
    install_cube(cube::Cube(size));
    return true;
}

int cube_size() noexcept
{
    return state ? size_of_cube() : 0;
}

bool set_view_mode(graphics::ViewMode mode) noexcept
{
    if (!state || !graphics::view_mode_from(static_cast<int>(mode))) {
        return false;
    }
    if (mode == state->view_mode) return true;

    // Preserve every orbit step produced before the command, then end only
    // gestures that still depend on the old viewport. Snaps survive cancel.
    drain_orbit();
    state->interaction.cancel();
    state->view_mode = mode;
    relayout_all();
    return true;
}

graphics::ViewMode view_mode() noexcept
{
    return state ? state->view_mode : kDefaultViewMode;
}

bool set_flat_style(graphics::FlatStyle style) noexcept
{
    if (!state || !graphics::flat_style_from(static_cast<int>(style))) {
        return false;
    }
    if (style == state->flat_style) return true;

    // A drag holds a rectangle that is about to move or vanish, so it goes the
    // same way a resize sends it. A snap only animates an angle and carries on.
    state->interaction.cancel();
    state->flat_style = style;
    relayout_all();
    return true;
}

graphics::FlatStyle flat_style() noexcept
{
    return state ? state->flat_style : kDefaultFlatStyle;
}

bool set_palette(graphics::Palette palette) noexcept
{
    if (!state || !graphics::palette_from(static_cast<int>(palette))) {
        return false;
    }

    // No cancel and no relayout: the palette is read where a sticker becomes
    // pixels and nowhere else, so a drag keeps its grip and a snap keeps its
    // angle. The change shows on the next frame that is drawn anyway.
    state->palette = palette;
    return true;
}

graphics::Palette palette() noexcept
{
    return state ? state->palette : kDefaultPalette;
}

std::uintptr_t lighting_buffer(std::uint32_t count) noexcept
{
    if (!state) return 0;
    if (!graphics::Lighting::valid_count(count)) return 0;

    state->lighting_values.assign(count, 0.0f);
    return reinterpret_cast<std::uintptr_t>(state->lighting_values.data());
}

bool set_lighting(std::uint32_t count) noexcept
{
    if (!state) return false;
    if (count != state->lighting_values.size()) return false;

    // Read into a copy and swap only when the whole list is good, so a
    // refused list leaves the lights exactly as they were.
    graphics::Lighting next = state->lighting;
    if (!next.from_values(state->lighting_values.data(), count)) return false;
    state->lighting = std::move(next);
    return true;
}

std::uint32_t lighting_count() noexcept
{
    if (!state) return 0;
    return static_cast<std::uint32_t>(state->lighting.value_count());
}

std::uintptr_t lighting_values(std::uint32_t count) noexcept
{
    if (!state) return 0;
    if (count != state->lighting.value_count()) return 0;

    state->lighting_values.assign(count, 0.0f);
    state->lighting.to_values(state->lighting_values.data());
    return reinterpret_cast<std::uintptr_t>(state->lighting_values.data());
}

bool set_canvas_theme(graphics::CanvasTheme theme) noexcept
{
    if (!state || !graphics::canvas_theme_from(static_cast<int>(theme))) {
        return false;
    }

    // Both, in one place: the field is what a surface sized later is cleared
    // to and the renderer is what a frame reads, and a refused value has
    // already left above, so neither can be written without the other.
    //
    // The ground is the renderer's rather than the scene's, so no frame's
    // inputs describe it: every target holds the old one until it is drawn
    // again, which is what forgetting its last frame says.
    state->canvas_theme = theme;
    for (auto& surface : state->surfaces) {
        if (surface.renderer) {
            surface.renderer->set_background(graphics::canvas_background(theme));
        }
        surface.drawn.reset();
    }
    return true;
}

bool set_speed_scale(float scale) noexcept
{
    if (!state) return false;

    // Not-a-number and infinity are refused rather than clamped: they are not
    // a speed someone asked for and got trimmed, they are not a speed at all,
    // and a duration made from either stops an animation from ever ending.
    if (!std::isfinite(scale)) return false;

    state->speed_scale = std::clamp(scale, kMinSpeedScale, kMaxSpeedScale);

    // A turn already running keeps the duration it started with. The value is
    // read where a move begins, so nothing in flight is re-timed halfway and
    // no frame jumps under a slider being dragged.
    return true;
}

float speed_scale() noexcept
{
    return state ? state->speed_scale : kDefaultSpeedScale;
}

}  // namespace rubiks::app
