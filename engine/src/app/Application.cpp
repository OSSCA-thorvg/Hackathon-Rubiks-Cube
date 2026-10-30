#include "app/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <thorvg.h>

#include "app/ApplicationState.hpp"

#include "cube/Cube.hpp"
#include "cube/CubeDraft.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/PackedMove.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"
#include "cube/solver/Solver.hpp"
#include "graphics/3d/Camera.hpp"
#include "graphics/frame/FrameInputs.hpp"
#include "graphics/Layout.hpp"
#include "graphics/3d/Light.hpp"
#include "graphics/2d/net/NetGeometry.hpp"
#include "graphics/2d/rings/RingsGeometry.hpp"
#include "graphics/3d/OrbitCamera.hpp"
#include "interaction/InteractionController.hpp"
#include "interaction/NetPicking.hpp"
#include "interaction/RingsPicking.hpp"
#include "render/Renderer.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

namespace rubiks::app {

using namespace detail;

std::unique_ptr<ApplicationState> detail::state;

namespace {

/**
 * How a multiplier turns a tempo into a duration.
 *
 * `duration = base / scale`, so 2 is twice as fast and 0.5 is half. Written
 * once here because it is the whole meaning of the setting, and the two places
 * a tempo is handed over both go through it.
 */
[[nodiscard]] double scaled_tempo(double base_ms, float scale) noexcept
{
    return base_ms / static_cast<double>(scale);
}

/** The camera looking into one cube region, the one it was drawn with. */
[[nodiscard]] graphics::Camera camera_for(const graphics::Rect& cube_rect) noexcept
{
    return graphics::camera_into(state->orbit, cube_rect);
}

/**
 * What a mode puts on the screen, said once for everything that has to ask.
 *
 * Routing a press and drawing a frame both used to ask by saying which mode it
 * is not -- "anything but Net draws the cube", "anything but Cube3D draws the
 * net" -- which is true of the modes that existed and quietly wrong of any
 * added afterwards. Naming what is shown instead means a new mode shows only
 * what it says it shows, and the compiler asks about it here.
 */
struct VisibleViews {
    bool cube;
    bool flat;
};

[[nodiscard]] VisibleViews visible_views(graphics::ViewMode mode) noexcept
{
    switch (mode) {
        case graphics::ViewMode::Cube3D:
            return VisibleViews{true, false};
        case graphics::ViewMode::Flat:
            return VisibleViews{false, true};
        case graphics::ViewMode::Both:
            break;
    }
    return VisibleViews{true, true};
}

/** Which drawings the flat region holds, said once for everything asking. */
struct FlatParts {
    bool net;
    bool rings;
};

/**
 * The drawings the flat region actually holds right now.
 *
 * Folds in whether that region is on screen at all, because every caller wants
 * both questions answered together and answering them apart left the same
 * conditional written out at each of them.
 */
[[nodiscard]] FlatParts flat_parts() noexcept
{
    if (!visible_views(state->view_mode).flat) return FlatParts{false, false};

    switch (state->flat_style) {
        case graphics::FlatStyle::Net:
            return FlatParts{true, false};
        case graphics::FlatStyle::Rings:
            return FlatParts{false, true};
        case graphics::FlatStyle::Both:
            break;
    }
    return FlatParts{true, true};
}

/**
 * The scenes the view mode shows, on whichever surface holds them.
 *
 * The axes go with the cube: they say which way the cube's axes point from
 * where it is being looked at, which means nothing with no cube on screen.
 */
[[nodiscard]] std::uint32_t shown_scenes() noexcept
{
    const auto flat = flat_parts();
    std::uint32_t shown = 0;
    if (visible_views(state->view_mode).cube) shown |= kSceneCube | kSceneAxes;
    if (flat.net) shown |= kSceneNet;
    if (flat.rings) shown |= kSceneRings;
    return shown;
}

/** What one surface draws: the scenes it holds that are shown. */
[[nodiscard]] std::uint32_t drawn_scenes(const Surface& surface) noexcept
{
    return surface.scenes & shown_scenes();
}

/**
 * Works out where a surface's scenes sit on it.
 *
 * With the same layout one canvas has always had, asked about the scenes this
 * surface draws rather than about every scene: the cube alone is the cube
 * alone, the net alone is the net alone, and a surface holding them all is the
 * canvas it always was. The axes take no region of their own beside the cube;
 * alone, they take the whole surface.
 */
void relayout(Surface& surface) noexcept
{
    const std::uint32_t drawn = drawn_scenes(surface);
    const bool cube = (drawn & kSceneCube) != 0;
    const bool net = (drawn & kSceneNet) != 0;
    const bool rings = (drawn & kSceneRings) != 0;

    if (surface.width == 0 || surface.height == 0 || (!cube && !net && !rings)) {
        surface.placement = graphics::CanvasLayout{};
        return;
    }

    const auto mode = cube && (net || rings) ? graphics::ViewMode::Both
                      : cube                 ? graphics::ViewMode::Cube3D
                                             : graphics::ViewMode::Flat;
    const auto style = net && rings ? graphics::FlatStyle::Both
                       : net        ? graphics::FlatStyle::Net
                                    : graphics::FlatStyle::Rings;
    surface.placement =
        graphics::layout(surface.width, surface.height, mode, style);
}

/** Whether `id` names a surface, and that surface has a target to draw on. */
[[nodiscard]] bool live_surface(std::uint32_t id) noexcept
{
    return id < kSurfaceCount && state->surfaces[id].renderer != nullptr;
}

// Defined with the painting below; the press on a draft reaches them first.
[[nodiscard]] bool paint_on(std::uint32_t id, float x, float y) noexcept;
[[nodiscard]] bool fill_on(std::uint32_t id, float x, float y) noexcept;

/**
 * The one place a settled turn becomes a change to the logical cube.
 *
 * Two paths reach it -- a snap ending on its own frame, and one confirmed
 * early by the next press -- and later phases hang the history, the move log
 * and the sound off this function, so neither path can be left out of them.
 */
void commit_move(const cube::CubeMove& move) noexcept
{
    state->cube_state.apply(move);

    // Two branches and no more: a move being played is already in the record,
    // so the cursor follows it the way its sequence says, and a move the user
    // made is written down where the cursor is. Read here rather than passed
    // in -- while a sequence plays, every way of turning a layer by hand is
    // shut, so there is no moment when the answer could differ from what the
    // caller would have said.
    if (state->playback) {
        state->timeline.step(state->playback->timeline_effect);
    } else {
        state->timeline.record(move);
    }
}

/** Whether anything at all owns the cube, playback included. */
[[nodiscard]] bool busy() noexcept
{
    return state->playback.has_value() || state->interaction.is_busy();
}

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
 * Starts the next move of a sequence once the previous one has landed.
 *
 * One move per frame at most. `next` moves on when a move is taken rather than
 * when it starts, so a move the controller will not animate is passed over
 * instead of retried -- retrying would leave the sequence stuck asking for
 * frames that never make progress.
 */
void advance_playback() noexcept
{
    if (!state->playback) return;

    // A turn of the sequence is still settling. The next one waits for it --
    // and so does the end of the sequence, which outlives its last move: the
    // player is what makes the application busy and what says the commit just
    // made was nobody's, and both have to hold on the frame that move lands.
    if (state->interaction.is_busy()) return;

    Player& player = *state->playback;

    // The whole of repeating: a player that has run out starts over, and so
    // never runs out. Nothing counts the rounds, because nothing needs to.
    if (player.repeats && player.next >= player.plan.size()) player.next = 0;

    while (player.next < player.plan.size()) {
        if (state->interaction.start_move(
                player.plan[player.next++],
                scaled_tempo(player.tempo_ms, state->speed_scale))) {
            return;
        }
    }

    // Nothing left to take, and nothing left turning.
    state->playback.reset();
}

}  // namespace

namespace detail {

/** How many layers the cube in hand has, which nothing else stores. */
[[nodiscard]] int size_of_cube() noexcept
{
    return state->cube_state.size();
}

/** Whether a size is one this application builds. */
[[nodiscard]] bool valid_cube_size(int size) noexcept
{
    return size >= kMinCubeSize && size <= kMaxCubeSize;
}

void relayout_all() noexcept
{
    for (auto& surface : state->surfaces) relayout(surface);
}

/**
 * Applies whatever sweep the controller has accumulated.
 *
 * Called from every point where a camera is about to be used, so the camera
 * rendered with, the camera picked against, and the camera a gesture captures
 * are always the same one.
 */
void drain_orbit() noexcept
{
    if (const auto delta = state->interaction.take_orbit_delta()) {
        state->orbit.turn(delta->yaw_degrees, delta->pitch_degrees);
    }
}

/**
 * Puts a different cube in the place of the one that is there.
 *
 * Every command that replaces the cube comes through here, so the order is
 * written once. The draft goes, because it is a colouring of the cube being
 * replaced. Whatever was playing goes, with any turn of it still in flight,
 * before the cube that turn would have landed on arrives. Then the cube, a
 * controller for it, and an empty record: a command that has a record to put
 * on the new cube writes it afterwards.
 *
 * The controller is replaced rather than told, because the size is the rule
 * it turns moves down by and a gesture half made on the cube before is not
 * something to carry across.
 *
 * `origin` is the colouring the session begins from when the cube was read
 * from one, and nothing otherwise, so every fresh cube forgets where the last
 * session began. Closing a draft does not do that on its own: only replacing
 * the cube makes the colouring this session started from stop being true. It
 * is taken by value so that the draft's own stickers can be handed over; they
 * have left the draft by the time this closes it.
 */
void install_cube(cube::Cube next,
                  std::vector<cube::FaceColor> origin) noexcept
{
    discard_painting();
    discard_playback();

    const int size = next.size();
    state->cube_state = std::move(next);
    state->interaction = interaction::InteractionController(size);
    state->timeline.clear();
    state->origin = std::move(origin);
}

/**
 * Whether the cube is not to be moved by a command now.
 *
 * While anything owns it, and while a draft is open. A draft is a copy of this
 * cube that somebody is mending, and the promise made of it is that backing
 * out leaves the cube as it was -- which a turn made behind it would break.
 * The commands that put a different cube there instead (a scramble, a reset,
 * a new size) close the draft rather than wait for it.
 */
[[nodiscard]] bool cube_held() noexcept
{
    return busy() || state->paint.has_value();
}

/**
 * Whether a layer set is one this application turns and writes down, for a
 * cube of `size`.
 *
 * One unbroken run of that cube's layers, short of all of them: exactly the
 * moves this application can make and can write down. Every layer at once is
 * a rotation of the whole cube, which it neither makes nor records.
 *
 * Asked where a move is made from a face and a range of depths, and asked
 * again of every move that arrives in a shared record. The mask field of a
 * packed move is wide enough for the slices of a much larger cube and for
 * sets with gaps in them, and a word arriving from a link is free to name any
 * of those -- but nothing here can make one, so letting one in would put a
 * move on the record that has no notation to be written in and no command
 * that could have produced it.
 */
[[nodiscard]] bool turnable_layers(cube::LayerMask layers, int size) noexcept
{
    return cube::is_layer_run(layers, size) &&
           layers != cube::layers_through(0, size - 1);
}

/**
 * The move a face names, taken as deep as a range of depths says and turned
 * `turns` quarters clockwise as seen from outside it.
 *
 * `cube::face_move()` with this application's refusals in front of it: which
 * layers the depths select and which way the face turns about its axis are
 * worked out there, not here. Depth 1 is the face; `(Right, 1, 1, 1)` is R
 * and `(Right, 1, 2, 1)` is Rw.
 *
 * Nothing for a range no move corresponds to: empty, reversed, off the far
 * side of the cube, or the whole of it -- the last being a rotation, which
 * this application neither makes nor writes down.
 */
[[nodiscard]] std::optional<cube::CubeMove> depth_move(cube::Face face,
                                                       int first_depth,
                                                       int last_depth,
                                                       int turns,
                                                       int size) noexcept
{
    const cube::CubeMove move =
        cube::face_move(face, first_depth, last_depth, turns, size);
    if (!turnable_layers(move.layers, size)) return std::nullopt;
    return move;
}

/**
 * Throws away everything a playback is made of, in one call.
 *
 * Two things, not one: the sequence, and any turn of it already in flight. A
 * turn that has left the player still commits on its own a few frames later,
 * so dropping only the player would apply the last move of an abandoned
 * scramble onto the cube that replaced it. The sweep the pointer has made is
 * kept -- a viewpoint is where the user left it -- and moves already committed
 * are never taken back.
 */
void discard_playback() noexcept
{
    drain_orbit();
    state->interaction.reset();
    state->playback.reset();
}

}  // namespace detail

bool initialize(std::uint32_t width, std::uint32_t height) noexcept
{
    // Invalid dimensions fail even when already initialized, so a caller
    // can never mistake a rejected size for an active configuration.
    if (!render::ThorVGSoftwareRenderer::valid_dimensions(width, height)) {
        return false;
    }

    if (state) return true;

    if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

    auto renderer = render::ThorVGSoftwareRenderer::create(width, height);
    if (!renderer) {
        static_cast<void>(tvg::Initializer::term());
        return false;
    }

    state = std::make_unique<ApplicationState>();
    Surface& first = state->surfaces[0];
    first.renderer = std::move(renderer);
    first.width = width;
    first.height = height;
    first.scenes = kAllScenes;
    relayout(first);
    return true;
}

bool resize_surface(std::uint32_t id, std::uint32_t width,
                    std::uint32_t height) noexcept
{
    if (!state || id >= kSurfaceCount) return false;

    const bool away = width == 0 && height == 0;
    if (!away &&
        !render::ThorVGSoftwareRenderer::valid_dimensions(width, height)) {
        return false;
    }

    Surface& surface = state->surfaces[id];
    if (away) {
        surface.renderer.reset();
    } else if (surface.renderer) {
        if (!surface.renderer->resize(width, height)) return false;
    } else {
        auto renderer = render::ThorVGSoftwareRenderer::create(width, height);
        if (!renderer) return false;
        // A surface given a size after the theme was chosen is cleared to
        // that theme like the rest, not to the renderer's own default.
        renderer->set_background(graphics::canvas_background(state->canvas_theme));
        surface.renderer = std::move(renderer);
    }

    // Whatever the gesture swept before the resize still counts; only the
    // gesture itself cannot continue -- and only a gesture on this surface.
    // One on another surface holds directions read off that surface, which
    // has not changed.
    drain_orbit();

    // A drag holds screen directions derived from the old viewport, so it
    // cannot survive the change; dropping it leaves the cube untouched. A snap
    // only animates an angle, so it is unaffected and keeps running.
    if (state->gesture_surface == id) state->interaction.cancel();

    surface.width = away ? 0 : width;
    surface.height = away ? 0 : height;
    surface.drawn.reset();
    relayout(surface);
    return true;
}

bool set_surface_scenes(std::uint32_t id, std::uint32_t scenes) noexcept
{
    if (!state || id >= kSurfaceCount) return false;
    if ((scenes & ~kAllScenes) != 0) return false;

    Surface& surface = state->surfaces[id];
    if (surface.scenes == scenes) return true;

    // The same as a resize to the gesture on it: its regions are about to
    // move or vanish.
    drain_orbit();
    if (state->gesture_surface == id) state->interaction.cancel();

    surface.scenes = scenes;
    relayout(surface);
    return true;
}

bool pointer_down_on(std::uint32_t id, float x, float y) noexcept
{
    if (!state) return false;

    // Everything that could reject the press comes first. A press that starts
    // nothing must not change anything either, and confirming the snap below
    // is a change. A surface with no size has nothing on it to press.
    if (!live_surface(id)) return false;
    if (!std::isfinite(x) || !std::isfinite(y)) return false;

    const Surface& surface = state->surfaces[id];
    const std::uint32_t drawn = drawn_scenes(surface);
    const bool cube_here = (drawn & kSceneCube) != 0;

    // Ahead of every pick and of the confirmation below, which is the only
    // position where it does what it says: while a sequence plays, a press may
    // look around the cube and do nothing else. Whether the snap in flight is
    // the user's never has to be asked, because the path that would ask it is
    // no longer reachable. With no cube on screen there is nothing to look
    // around either, so the press is refused rather than aimed at a region
    // that has no size -- which would make the viewpoint permanently NaN.
    //
    // A watched pattern is a sequence like any other here, and so is looked
    // around rather than interrupted. Looking is not input into the cube: the
    // viewpoint is the user's at every moment, including this one, and a
    // pattern is worth turning round to watch. What ends the watching is a
    // command, which a drag is not.
    if (state->playback) {
        if (!cube_here) return false;

        drain_orbit();
        if (!state->interaction.start_orbit(x, y, surface.placement.cube)) {
            return false;
        }
        state->gesture_surface = id;
        return true;
    }

    // The layout is what routes the press. The flat view owns every cell or
    // sticker inside its own region, and the 3D region and the background
    // around it own the rest, so the two never contend for the same pixel.
    // Which views there are is this surface's: a press lands on the surface
    // it was made on and on nothing else.

    // A draft turns the net from something to turn into something to colour.
    // The press is answered here and goes no further: nothing about the cube
    // may move while somebody is writing down what is on theirs, and a drag
    // from this press carries the brush rather than a layer.
    if (state->paint) {
        state->paint->stroking = state->paint->filling ? fill_on(id, x, y)
                                                       : paint_on(id, x, y);
        if (state->paint->stroking) state->gesture_surface = id;
        return state->paint->stroking;
    }

    // The two drawings never share a rectangle, so asking both in turn cannot
    // give two answers.
    std::optional<interaction::NetPick> net_pick;
    std::optional<interaction::RingsPick> rings_pick;
    if ((drawn & kSceneNet) != 0) {
        net_pick =
            interaction::pick_net(x, y, surface.placement.net, size_of_cube());
    }
    if (!net_pick && (drawn & kSceneRings) != 0) {
        rings_pick = interaction::pick_rings(x, y, surface.placement.rings,
                                             size_of_cube());
    }

    // Nothing to start: no cube on this surface and the press missed
    // everything flat on it. Sweeping the viewpoint is not the fallback here,
    // because there is no viewpoint on this surface to sweep.
    if (!net_pick && !rings_pick && !cube_here) return false;

    // A snap still animating already knows its turn, so it is applied here
    // rather than made to block the new gesture: that is what kept fast
    // consecutive drags from losing every other stroke.
    if (const auto move = state->interaction.finish_snap()) {
        commit_move(*move);
    }

    // Before the camera is captured, never after: a press arriving between
    // two frames must aim at the viewpoint the last one produced.
    drain_orbit();

    bool began = false;
    if (net_pick) {
        began = state->interaction.net_pointer_down(
            x, y, surface.placement.net, *net_pick);
    } else if (rings_pick) {
        began = state->interaction.rings_pointer_down(
            x, y, surface.placement.rings, *rings_pick);
    } else {
        began = state->interaction.pointer_down(
            x, y, camera_for(surface.placement.cube), surface.placement.cube);
    }
    if (began) state->gesture_surface = id;
    return began;
}

void pointer_move(float x, float y) noexcept
{
    if (!state) return;

    // A drag across the net colours every cell it crosses, which is how a face
    // is copied off a real one without lifting the finger between squares --
    // on the surface the stroke began on, whose pixels the moves are in.
    if (state->paint) {
        if (state->paint->stroking) {
            static_cast<void>(paint_on(state->gesture_surface, x, y));
        }
        return;
    }

    state->interaction.pointer_move(x, y);
}

void pointer_up() noexcept
{
    if (!state) return;

    if (state->paint) {
        state->paint->stroking = false;
        return;
    }

    // The multiplier reaches a drag release as much as a played sequence: one
    // slider for how fast the cube turns, whoever turned it.
    state->interaction.pointer_up(
        scaled_tempo(kUserTempoMs, state->speed_scale));
}

void pointer_cancel() noexcept
{
    if (!state) return;

    if (state->paint) {
        state->paint->stroking = false;
        return;
    }

    state->interaction.cancel();
}

bool advance(double elapsed_ms) noexcept
{
    if (!state) return false;

    const bool more_frames = state->interaction.advance(elapsed_ms);

    drain_orbit();

    if (const auto move = state->interaction.take_committed_move()) {
        commit_move(*move);
    }

    advance_playback();

    // A sequence between two of its moves has nothing animating, so the
    // controller would let the frame loop stop halfway through it.
    return more_frames || state->playback.has_value();
}

namespace {

/**
 * Notes whether the cube has changed since it was last drawn.
 *
 * By looking rather than by being told. Every command that replaces the cube
 * and every commit that turns it would otherwise have to remember to say so,
 * and a skipped frame is only safe if none of them ever forgets. The look is
 * not free -- a cube of N layers is 6N^2 colours, 4704 at the largest -- but
 * it is a fraction of a millisecond next to a frame of that cube that takes a
 * dozen.
 */
void note_cube_change() noexcept
{
    if (!(state->cube_state == state->drawn_cube)) {
        state->drawn_cube = state->cube_state;
        ++state->cube_revision;
    }
}

/**
 * What a surface's next frame would be drawn from.
 *
 * Each scene it draws, handed everything that scene reads and nothing it does
 * not -- which is the whole of skipping a frame: the net holds no viewpoint,
 * so a sweep leaves its frame standing, and the axes hold no cube, so a turn
 * leaves theirs.
 */
[[nodiscard]] graphics::FrameInputs frame_for(const Surface& surface) noexcept
{
    const std::uint32_t drawn = drawn_scenes(surface);
    const graphics::CubeRevision cube{&state->cube_state, state->cube_revision};
    const auto rotation = state->interaction.active_rotation();

    graphics::FrameInputs frame;
    frame.width = surface.width;
    frame.height = surface.height;
    frame.placement = surface.placement;

    if ((drawn & kSceneCube) != 0) {
        frame.cube = graphics::CubeDrawing{cube, rotation, state->palette,
                                           state->lighting, state->orbit};
    }
    if ((drawn & kSceneNet) != 0) {
        std::optional<graphics::NetPainting> painting;
        if (state->paint) {
            std::optional<cube::FaceCell> cursor;
            if (state->paint->cursor_shown) cursor = state->paint->cursor;
            const cube::ColorView colors = state->paint->model.view().flat();
            painting = graphics::NetPainting{
                std::vector<cube::FaceColor>(colors.begin(), colors.end()),
                state->paint->model.reading().blamed, cursor};
        }
        frame.net = graphics::NetDrawing{cube, rotation, state->palette,
                                         state->interaction.net_guides(),
                                         std::move(painting)};
    }
    if ((drawn & kSceneRings) != 0) {
        frame.rings = graphics::RingsDrawing{cube, rotation, state->palette,
                                             state->interaction.rings_guides()};
    }
    if ((drawn & kSceneAxes) != 0) {
        frame.axes = graphics::AxesDrawing{state->orbit};
    }
    return frame;
}

}  // namespace

bool render() noexcept
{
    if (!state) return false;

    note_cube_change();

    // Each surface draws only when what it shows has changed since its last
    // frame. A viewpoint sweep leaves the net's frame where it was, and a turn
    // leaves the axes' -- and a host copies a surface out only when its frame
    // count has moved, so the skipped work is skipped all the way to the page.
    bool drawn_all = true;
    for (auto& surface : state->surfaces) {
        if (!surface.renderer) continue;

        graphics::FrameInputs frame = frame_for(surface);
        if (surface.drawn == frame) continue;

        if (!surface.renderer->render(graphics::compose(frame))) {
            surface.drawn.reset();
            drawn_all = false;
            continue;
        }
        surface.drawn = std::move(frame);
        ++surface.frame;
    }
    return drawn_all;
}

bool scramble(std::uint32_t seed, std::uint32_t move_count) noexcept
{
    if (!state) return false;

    // Everything that could refuse comes before anything is changed, so a
    // rejected count leaves the cube and any playback exactly as they were.
    // Zero would be a scramble that leaves the cube solved; the upper bound is
    // against a number typed into the box that would never finish playing.
    if (move_count == 0 || move_count > kMaxScrambleMoves) return false;

    auto plan = cube::make_scramble(size_of_cube(), seed, move_count);
    if (plan.empty()) return false;

    // The cube restarts but the viewpoint does not, so a sweep the controller
    // has not published yet still counts. Everything else goes, including a
    // snap that would otherwise commit onto the new cube, and a colouring of
    // the cube that is about to stop existing.
    install_cube(cube::Cube(size_of_cube()));

    // The whole sequence is recorded before any of it has happened, and the
    // cursor walks up it as the moves land -- so a scramble interrupted half
    // way leaves a record of exactly what is on the cube.
    state->timeline.begin_scramble(plan);

    // Turned rather than applied: the cube is still solved when this returns
    // and arrives at the scrambled state a few hundred frames later.
    // Runs out and has nowhere to go back to: a scramble is where the cube is
    // meant to be left, not an interlude. Its moves are already written down,
    // so each commit only carries the cursor forward -- the same thing a redo
    // does, because it is the same thing.
    state->playback = Player{std::move(plan),
                             0,
                             kScrambleTempoMs,
                             cube::TimelineEffect::Advance,
                             false,
                             false,
                             std::nullopt};
    return true;
}

void reset_cube() noexcept
{
    if (!state) return;

    // Same split as scramble(): the cube is the only thing this command owns.
    install_cube(cube::Cube(size_of_cube()));
}

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

bool is_solved() noexcept
{
    return state && state->cube_state.is_solved();
}

namespace {

/** The solver that handles a cube of this size, or nothing. */
[[nodiscard]] const cube::solver::Solver* solver_for(int size) noexcept
{
    if (!state) return nullptr;

    for (const auto& solver : state->solvers) {
        if (solver->supports(size)) return solver.get();
    }
    return nullptr;
}

}  // namespace

bool can_solve() noexcept
{
    // The lifecycle first: `size_of_cube()` asks the state what it is, so
    // there has to be one before anything else is asked.
    return state && solver_for(size_of_cube()) != nullptr;
}

bool solve() noexcept
{
    if (!can_rewind() || !can_solve()) return false;

    // Worked out before anything is written, so that a cube already solved --
    // the one case with nothing to play -- refuses here with the record and
    // the cube untouched, the way every other empty command does.
    auto plan = solver_for(size_of_cube())->solve(state->cube_state);
    if (plan.empty()) return false;

    // Written down and then played forward, which is what a scramble does. So
    // there is no third way of getting moves into the cube, and a solve broken
    // off part way leaves the rest of it above the cursor for a redo.
    state->timeline.record_ahead(plan);
    return play_rewind(std::move(plan), cube::TimelineEffect::Advance);
}

void stop_playback() noexcept
{
    if (!state || !state->playback || !state->playback->stoppable) return;

    // Confirmed rather than discarded, and confirmed while the player is still
    // here: the commit goes through the ordinary path, sees a sequence
    // playing, and moves the cursor with it. So wherever a rewind is stopped,
    // the record and the cube say the same thing.
    if (const auto move = state->interaction.finish_snap()) {
        commit_move(*move);
    }

    state->playback.reset();
}

bool turn_face(cube::Face face, int first_depth, int last_depth,
               int face_turns) noexcept
{
    if (!state) return false;
    if (!cube::face_from(static_cast<int>(face))) return false;
    if (face_turns != -1 && face_turns != 1 && face_turns != 2) return false;

    // The controller does not know about playback, and between a sequence
    // being accepted and its first frame it is idle -- so without this the
    // very next keypress would land in the middle of the sequence. Nor does
    // it know about a draft, which the cube may not turn behind.
    if (cube_held()) return false;

    // Every refusal a range can earn is in here: off the cube, inside out, or
    // all of it at once.
    const auto move =
        depth_move(face, first_depth, last_depth, face_turns, size_of_cube());
    if (!move) return false;

    return state->interaction.start_move(*move);
}

void reset_view() noexcept
{
    if (!state) return;

    state->interaction.cancel();
    // The requested home is authoritative, so a pending orbit step must not
    // be applied on the next frame after it.
    static_cast<void>(state->interaction.take_orbit_delta());
    state->orbit = graphics::home_orbit();
}

bool is_busy() noexcept
{
    return state && busy();
}

std::uintptr_t surface_pixel_buffer(std::uint32_t id) noexcept
{
    if (!state || !live_surface(id)) return 0;
    return state->surfaces[id].renderer->pixel_buffer();
}

std::uint32_t surface_pixel_byte_length(std::uint32_t id) noexcept
{
    if (!state || !live_surface(id)) return 0;
    return state->surfaces[id].renderer->pixel_byte_length();
}

std::uint32_t surface_frame(std::uint32_t id) noexcept
{
    if (!state || id >= kSurfaceCount) return 0;
    return state->surfaces[id].frame;
}

void shutdown() noexcept
{
    if (!state) return;

    // Destroying the state releases everything it owns, the renderer
    // included, so ThorVG is only terminated afterwards.
    state.reset();
    static_cast<void>(tvg::Initializer::term());
}

}  // namespace rubiks::app
