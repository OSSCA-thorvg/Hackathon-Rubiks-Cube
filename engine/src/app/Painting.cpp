#include "app/Application.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/ApplicationState.hpp"
#include "cube/Assembly.hpp"
#include "cube/Cube.hpp"
#include "cube/Face.hpp"
#include "graphics/2d/net/NetGeometry.hpp"
#include "graphics/frame/FrameInputs.hpp"
#include "interaction/NetPicking.hpp"

namespace rubiks::app {

using namespace detail;

namespace {

/**
 * The cell a point on one surface is over, when that surface draws the net
 * and the net holds the point.
 */
[[nodiscard]] std::optional<interaction::NetPick> net_cell_on(std::uint32_t id,
                                                              float x,
                                                              float y) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    if (!live_surface(id)) return std::nullopt;

    const Surface& surface = state->surfaces[id];
    if ((drawn_scenes(surface) & kSceneNet) == 0) return std::nullopt;
    return interaction::pick_net(x, y, surface.placement.net, size_of_cube());
}

/**
 * Lays the brush on one cell of the draft.
 *
 * Where a press and a key both end up, so the two colour a cell the same way:
 * they differ only in how they found it.
 */
[[nodiscard]] bool paint_cell(const interaction::NetPick& pick) noexcept
{
    if (!cube::face_from(static_cast<int>(pick.face))) return false;
    if (pick.row < 0 || pick.row >= size_of_cube() ||
        pick.col < 0 || pick.col >= size_of_cube()) return false;
    static_cast<void>(state->paint->model.set_color(
        pick.face, pick.row, pick.col, state->paint->brush));
    return true;
}

/** Lays the brush on every cell of one face of the draft. */
bool fill_face(cube::Face face) noexcept
{
    static_cast<void>(state->paint->model.fill_face(face, state->paint->brush));
    return true;
}

bool paint_on(std::uint32_t id, float x, float y) noexcept
{
    if (!state || !state->paint) return false;

    const auto pick = net_cell_on(id, x, y);
    if (!pick) return false;
    return paint_cell(*pick);
}

bool fill_on(std::uint32_t id, float x, float y) noexcept
{
    if (!state || !state->paint) return false;

    const auto pick = net_cell_on(id, x, y);
    if (!pick) return false;
    return fill_face(pick->face);
}

}  // namespace

namespace detail {

/**
 * Drops a draft that has stopped describing the cube it was taken from.
 *
 * A draft is a colouring of one cube of one size, so every command that puts a
 * different cube there leaves it describing nothing. Each of those comes
 * through here rather than each of them remembering to, and the person is told
 * by the draft closing rather than by finding it quietly wrong.
 */
void discard_painting() noexcept
{
    state->paint.reset();
}

/**
 * What a press does while a draft is open: it colours the cell under it, or
 * the whole face when the draft is filling, and a press that lands on the net
 * begins a stroke on the surface it was made on.
 */
bool paint_pointer_down(std::uint32_t id, float x, float y) noexcept
{
    state->paint->stroking = state->paint->filling ? fill_on(id, x, y)
                                                   : paint_on(id, x, y);
    if (state->paint->stroking) state->gesture_surface = id;
    return state->paint->stroking;
}

/**
 * A drag across the net colours every cell it crosses, which is how a face
 * is copied off a real one without lifting the finger between squares -- on
 * the surface the stroke began on, whose pixels the moves are in.
 */
void paint_pointer_move(float x, float y) noexcept
{
    if (state->paint->stroking) {
        static_cast<void>(paint_on(state->gesture_surface, x, y));
    }
}

/** Ends a stroke, whether the press was lifted or taken away. */
void paint_pointer_up() noexcept
{
    state->paint->stroking = false;
}

/**
 * What the net draws over the cube: the draft's colours, the cells the last
 * refusal blamed and, while it is shown, the keyboard's place. Nothing without
 * a draft.
 */
std::optional<graphics::NetPainting> net_painting() noexcept
{
    if (!state->paint) return std::nullopt;

    std::optional<cube::FaceCell> cursor;
    if (state->paint->cursor_shown) cursor = state->paint->cursor;
    const cube::ColorView colors = state->paint->model.view().flat();
    return graphics::NetPainting{
        std::vector<cube::FaceColor>(colors.begin(), colors.end()),
        state->paint->model.reading().blamed, cursor};
}

}  // namespace detail

bool begin_painting() noexcept
{
    if (!state) return false;

    // A draft taken of a cube in the middle of a turn would be a copy of a
    // moment nobody chose, so a sequence has to finish or be stopped first.
    if (is_busy()) return false;

    // Already open. Reopening would throw away what has been coloured, and
    // asking twice is not a way of asking for that.
    if (state->paint) return false;

    // A gesture still in hand is let go of first. From here on a press on the
    // net colours and a release ends the colouring, so a drag left half made
    // would have nothing to finish it -- and would still be sitting there when
    // the draft was put away.
    state->interaction.cancel();

    state->paint.emplace(state->cube_state);
    return true;
}

void cancel_painting() noexcept
{
    if (!state) return;
    discard_painting();
}

bool is_painting() noexcept
{
    return state && state->paint.has_value();
}

bool set_brush(cube::FaceColor colour) noexcept
{
    if (!state || !state->paint) return false;
    if (!cube::face_color_from(static_cast<int>(colour))) return false;
    state->paint->brush = colour;
    return true;
}

cube::FaceColor brush() noexcept
{
    if (!state || !state->paint) return kDefaultBrush;
    return state->paint->brush;
}

bool set_filling(bool whole_face) noexcept
{
    if (!state || !state->paint) return false;
    state->paint->filling = whole_face;
    return true;
}

bool is_filling() noexcept
{
    return state && state->paint && state->paint->filling;
}

bool paint_cursor_step(int columns, int rows) noexcept
{
    if (!state || !state->paint) return false;
    auto& draft = *state->paint;
    const int size = size_of_cube();

    // The first step places it rather than moves it: in the middle of the
    // front face, which is the one somebody copying their cube looks at first.
    if (!draft.cursor) {
        draft.cursor = interaction::NetPick{cube::Face::Front, size / 2, size / 2};
        return true;
    }

    // Counted across the whole cross, so a step off one face's edge lands on
    // the face drawn beside it -- which the net draws there because it is the
    // face that shares that edge on the cube.
    const auto block = graphics::net_block(draft.cursor->face);
    const int column = block.column * size + draft.cursor->col + columns;
    const int row = block.row * size + draft.cursor->row + rows;
    if (column < 0 || row < 0) return false;

    const auto face = graphics::net_face_at(column / size, row / size);
    if (!face) return false;

    draft.cursor = interaction::NetPick{*face, row % size, column % size};
    return true;
}

bool paint_at_cursor() noexcept
{
    if (!state || !state->paint || !state->paint->cursor) return false;

    const auto& at = *state->paint->cursor;
    return state->paint->filling ? fill_face(at.face) : paint_cell(at);
}

void set_paint_cursor_shown(bool shown) noexcept
{
    if (state && state->paint) state->paint->cursor_shown = shown;
}

std::optional<PaintCursor> paint_cursor() noexcept
{
    if (!state || !state->paint || !state->paint->cursor) return std::nullopt;

    const interaction::NetPick& at = *state->paint->cursor;
    return PaintCursor{at, state->paint->model.face(at.face).at(at.row, at.col)};
}

int painted_count(cube::FaceColor colour) noexcept
{
    if (!state || !state->paint) return 0;

    return static_cast<int>(state->paint->model.count(colour));
}

bool apply_painting() noexcept
{
    if (!state || !state->paint) return false;

    // Validation is requested here and cached until the next draft edit.
    std::optional<cube::Cube> built = state->paint->model.assemble();
    if (!built) return false;

    // Past every refusal. What arrives is a starting position and not a move,
    // so the record goes the way it goes for a change of size -- and a rewind
    // afterwards comes back to this cube rather than to a solved one, which is
    // what a person who painted their own cube would expect it to mean.
    //
    // The draft's stickers are handed over as where the session began, which
    // is the one thing about the draft that outlives it.
    std::vector<cube::FaceColor> origin =
        cube::painting_of(state->paint->model.view());
    install_cube(std::move(*built), std::move(origin));
    return true;
}

cube::PaintFault painting_fault() noexcept
{
    if (!state || !state->paint) return cube::PaintFault::None;
    return state->paint->model.reading().fault;
}

}  // namespace rubiks::app
