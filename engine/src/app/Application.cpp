#include "app/Application.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <thorvg.h>

#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "graphics/AxisGizmo.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/RingsGeometry.hpp"
#include "graphics/OrbitCamera.hpp"
#include "graphics/Pipeline.hpp"
#include "graphics/RenderScene.hpp"
#include "interaction/InteractionController.hpp"
#include "interaction/NetPicking.hpp"
#include "interaction/RingsPicking.hpp"
#include "math/Transform.hpp"
#include "render/Renderer.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

namespace rubiks::app {
namespace {

constexpr int kCubeSize = 3;

/** How fast a user's own release settles, in milliseconds per quarter turn. */
constexpr double kUserTempoMs = interaction::kSnapMsPerQuarterTurn;

/**
 * The same for a scramble, which is watched rather than made.
 *
 * Brisker than a user's own turn, and no brisker than the time a piece takes
 * to lift and settle again: a move shorter than that never lets the lift form,
 * and the drawing would flatten into a slideshow of states. Twenty moves land
 * inside three seconds at this pace.
 */
constexpr double kScrambleTempoMs = interaction::kOpeningMs;

/**
 * The same for a pattern being watched, which is not waited on at all.
 *
 * Slower than either: nobody is waiting for it to arrive anywhere, and the
 * point of it is to be looked at. At this pace the shortest of the patterns
 * comes back round to where it started in a little over a minute.
 */
constexpr double kAmbientTempoMs = 300.0;

/**
 * One scripted sequence being played back, with how to play it.
 *
 * A value rather than a queue and a flag beside it: a sequence and the way it
 * is being played are set up together and die together, and two pieces of
 * state with one lifetime are one object. Later phases add fields here -- a
 * timeline effect for a rewind -- rather than adding names to an enum of where
 * the moves came from, because nothing downstream asks where they came from,
 * only what to do with them.
 */
struct Player {
    std::vector<cube::CubeMove> plan;
    std::size_t next = 0;
    /** Milliseconds per quarter turn, as `start_move` takes it. */
    double tempo_ms;
    /** Whether running out of moves starts the sequence again. */
    bool repeats = false;
    /**
     * The cube to put back when this player is stopped rather than thrown away.
     *
     * Here rather than beside the player because the two have exactly one
     * lifetime: a snapshot is taken when a pattern starts repeating and means
     * nothing at any other moment. Keeping it here is also what makes
     * "a scramble, a reset or a shutdown ends the watching without putting
     * anything back" true without a line of its own -- those throw the player
     * away, and the snapshot goes with it, which is right, because the state
     * such a command makes is the one that should stand.
     */
    std::optional<cube::CubeState> restore_to;
};

/**
 * Everything one running application owns.
 *
 * One object rather than a list of globals so that its lifetime is the
 * initialization state: initialize() creates it, shutdown() destroys it, and
 * there is no reset list that a new field could be left off of.
 */
struct ApplicationState {
    std::unique_ptr<render::Renderer> renderer;

    // Mirrors the renderer target size; only updated after a successful
    // initialize() or resize(), so it cannot drift from the actual target.
    graphics::CanvasLayout placement;
    std::uint32_t surface_width = 0;
    std::uint32_t surface_height = 0;

    // The whole of the viewpoint state. The Camera is derived from this and
    // the layout whenever one is needed rather than stored, so there is no
    // second copy to keep in step and no way to forget to rebuild it.
    graphics::OrbitCamera orbit = graphics::home_orbit();

    // The logical cube, only ever holding quarter turns. A drag in progress
    // lives in the controller instead, and reaches this through a committed
    // move.
    cube::CubeState cube_state{kCubeSize};
    interaction::InteractionController interaction{kCubeSize};
    graphics::ViewMode view_mode = graphics::ViewMode::Both;
    graphics::FlatStyle flat_style = graphics::FlatStyle::Net;
    std::uint32_t user_move_count = 0;

    // The whole of the playback state. Its presence is what "a sequence is
    // playing" means, so there is no second flag to fall out of step with it.
    std::optional<Player> playback;
};

std::unique_ptr<ApplicationState> state;

void adopt_surface(std::uint32_t width, std::uint32_t height) noexcept
{
    state->surface_width = width;
    state->surface_height = height;
    state->placement = graphics::layout(width, height, state->view_mode,
                                        state->flat_style);
}

[[nodiscard]] graphics::Camera current_camera() noexcept
{
    // Follows the cube region rather than the canvas. That region is square,
    // so the aspect is always 1, but deriving it keeps the two in step if the
    // layout ever changes.
    const auto& cube_rect = state->placement.cube;
    const float aspect =
        cube_rect.height > 0.0f ? cube_rect.width / cube_rect.height : 1.0f;
    return state->orbit.to_camera(aspect);
}

/** Returns the named clockwise move for one external face. */
[[nodiscard]] cube::CubeMove clockwise_move(cube::Face face) noexcept
{
    switch (face) {
        case cube::Face::Right:
            return cube::moves::R(kCubeSize);
        case cube::Face::Left:
            return cube::moves::L(kCubeSize);
        case cube::Face::Up:
            return cube::moves::U(kCubeSize);
        case cube::Face::Down:
            return cube::moves::D(kCubeSize);
        case cube::Face::Front:
            return cube::moves::F(kCubeSize);
        case cube::Face::Back:
            break;
    }
    return cube::moves::B(kCubeSize);
}

/**
 * The middle slice between L and R, turning the way L does.
 *
 * Not one of `cube::moves`, which are the six outer faces of a cube of any
 * size. A slice of an N x N x N cube would have to say which of its N - 2
 * inner layers was meant, and at the one size this application builds there is
 * nothing to say.
 */
[[nodiscard]] cube::CubeMove middle_slice() noexcept
{
    return cube::CubeMove{cube::Axis::X, cube::layer(kCubeSize / 2), -1};
}

/**
 * The patterns the watching mode repeats, one of which a choice picks out.
 *
 * Moves only. Which of them comes back round soonest is worked out in the
 * phase document and is what put these four here rather than four others;
 * carrying those numbers in the code would be carrying values nothing reads.
 *
 * Built from the named moves rather than written out as axes and layers, so
 * which layer a face is and which way it turns stay facts of the domain with
 * one copy. Built once, on first use, because those names are functions.
 */
[[nodiscard]] const std::array<std::vector<cube::CubeMove>,
                               kAmbientPatternCount>&
ambient_patterns()
{
    using namespace cube::moves;

    static const std::array<std::vector<cube::CubeMove>, kAmbientPatternCount>
        patterns{{
            // R U M' F
            {R(kCubeSize), U(kCubeSize), inverse(middle_slice()),
             F(kCubeSize)},
            // R U' D' F
            {R(kCubeSize), inverse(U(kCubeSize)), inverse(D(kCubeSize)),
             F(kCubeSize)},
            // R U F' D
            {R(kCubeSize), U(kCubeSize), inverse(F(kCubeSize)), D(kCubeSize)},
            // R U F D
            {R(kCubeSize), U(kCubeSize), F(kCubeSize), D(kCubeSize)},
        }};

    return patterns;
}

/** Reports whether a face value is one of the six external cube faces. */
[[nodiscard]] bool valid_face(cube::Face face) noexcept
{
    switch (face) {
        case cube::Face::Right:
        case cube::Face::Left:
        case cube::Face::Up:
        case cube::Face::Down:
        case cube::Face::Front:
        case cube::Face::Back:
            return true;
    }
    return false;
}

/** Reports whether an enum arriving through a primitive boundary is valid. */
[[nodiscard]] bool valid_view_mode(graphics::ViewMode mode) noexcept
{
    switch (mode) {
        case graphics::ViewMode::Cube3D:
        case graphics::ViewMode::Both:
        case graphics::ViewMode::Flat:
            return true;
    }
    return false;
}

[[nodiscard]] bool valid_flat_style(graphics::FlatStyle style) noexcept
{
    switch (style) {
        case graphics::FlatStyle::Net:
        case graphics::FlatStyle::Rings:
        case graphics::FlatStyle::Both:
            return true;
    }
    return false;
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
 * The one place a settled turn becomes a change to the logical cube.
 *
 * Two paths reach it -- a snap ending on its own frame, and one confirmed
 * early by the next press -- and later phases hang the history, the move log
 * and the sound off this function, so neither path can be left out of them.
 */
void commit_move(const cube::CubeMove& move) noexcept
{
    state->cube_state.apply(move);

    // A played move is nobody's, so it is not counted and does not start the
    // clock. Read here rather than passed in: while a sequence plays, every
    // way of turning a layer by hand is shut, so there is no moment when the
    // answer could differ from what the caller would have said.
    if (!state->playback) ++state->user_move_count;
}

/** Whether anything at all owns the cube, playback included. */
[[nodiscard]] bool busy() noexcept
{
    return state->playback.has_value() || state->interaction.is_busy();
}

/**
 * Whether the sequence playing is a pattern being watched.
 *
 * Which is to say whether there is a cube waiting to be put back. Watching is
 * a playback with somewhere to return to and no end of its own, so having the
 * snapshot is not a symptom of it -- it is the whole of what it is.
 */
[[nodiscard]] bool ambient_running() noexcept
{
    return state->playback && state->playback->restore_to.has_value();
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
        if (state->interaction.start_move(player.plan[player.next++],
                                          player.tempo_ms)) {
            return;
        }
    }

    // Nothing left to take, and nothing left turning.
    state->playback.reset();
}

}  // namespace

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
    state->renderer = std::move(renderer);
    adopt_surface(width, height);
    return true;
}

bool resize(std::uint32_t width, std::uint32_t height) noexcept
{
    if (!state) return false;

    if (!state->renderer->resize(width, height)) return false;

    // Whatever the gesture swept before the resize still counts; only the
    // gesture itself cannot continue.
    drain_orbit();

    // A drag holds screen directions derived from the old viewport, so it
    // cannot survive the change; dropping it leaves the cube untouched. A snap
    // only animates an angle, so it is unaffected and keeps running.
    state->interaction.cancel();

    adopt_surface(width, height);
    return true;
}

bool pointer_down(float x, float y) noexcept
{
    if (!state) return false;

    // Everything that could reject the press comes first. A press that starts
    // nothing must not change anything either, and confirming the snap below
    // is a change.
    if (!std::isfinite(x) || !std::isfinite(y)) return false;

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
        if (!visible_views(state->view_mode).cube) return false;

        drain_orbit();
        return state->interaction.start_orbit(x, y, state->placement.cube);
    }

    // The layout is what routes the press. The flat view owns every cell or
    // sticker inside its own region, and the 3D region and the background
    // around it own the rest, so the two never contend for the same pixel.
    const auto shown = visible_views(state->view_mode);
    const auto flat = flat_parts();

    // The two drawings never share a rectangle, so asking both in turn cannot
    // give two answers.
    std::optional<interaction::NetPick> net_pick;
    std::optional<interaction::RingsPick> rings_pick;
    if (flat.net) {
        net_pick = interaction::pick_net(x, y, state->placement.net, kCubeSize);
    }
    if (!net_pick && flat.rings) {
        rings_pick =
            interaction::pick_rings(x, y, state->placement.rings, kCubeSize);
    }

    // Nothing to start: the flat view is the only one showing and the press
    // missed everything in it. Sweeping the viewpoint is not the fallback
    // here, because there is no viewpoint on screen to sweep.
    if (!net_pick && !rings_pick && !shown.cube) return false;

    // A snap still animating already knows its turn, so it is applied here
    // rather than made to block the new gesture: that is what kept fast
    // consecutive drags from losing every other stroke.
    if (const auto move = state->interaction.finish_snap()) {
        commit_move(*move);
    }

    // Before the camera is captured, never after: a press arriving between
    // two frames must aim at the viewpoint the last one produced.
    drain_orbit();

    if (net_pick) {
        return state->interaction.net_pointer_down(x, y, state->placement.net,
                                                   *net_pick);
    }

    if (rings_pick) {
        return state->interaction.rings_pointer_down(
            x, y, state->placement.rings, *rings_pick);
    }

    return state->interaction.pointer_down(x, y, current_camera(),
                                           state->placement.cube);
}

void pointer_move(float x, float y) noexcept
{
    if (!state) return;

    state->interaction.pointer_move(x, y);
}

void pointer_up() noexcept
{
    if (!state) return;

    state->interaction.pointer_up(kUserTempoMs);
}

void pointer_cancel() noexcept
{
    if (!state) return;

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

bool render() noexcept
{
    if (!state) return false;

    // The model transform is identity for now, but it goes through the real
    // path so the quaternion-to-matrix step is exercised every frame.
    const math::Transform model;
    const graphics::Camera camera = current_camera();

    const auto shown = visible_views(state->view_mode);

    graphics::RenderScene scene;
    if (shown.cube) {
        scene = graphics::build_cube_scene(
                    state->cube_state, state->interaction.active_rotation())  //
                | graphics::transform(model)                                  //
                | graphics::view(camera)                                      //
                | graphics::project(camera)                                   //
                | graphics::cull()                                            //
                | graphics::depth_sort()                                      //
                | graphics::viewport(state->placement.cube);

        // Which way each axis points from here, in the colors the net's guide
        // lines use, so a cyan loop over there and a cyan arm over here are
        // plainly the same axis.
        graphics::append_scene(
            scene, graphics::build_axis_gizmo(camera, state->placement.cube));
    }

    const auto flat = flat_parts();

    if (flat.net) {
        // The net is already screen-space, so it only has to be appended. It
        // gets the same rotation as the 3D scene, which is what makes one
        // gesture move every view in the same frame.
        graphics::append_scene(
            scene, graphics::build_net_scene(
                       state->cube_state, state->placement.net,
                       state->interaction.active_rotation()));

        // The rings the pressed cell could turn on, drawn over the stickers
        // so the gesture says where it is about to go before it goes there.
        graphics::append_scene(
            scene, graphics::build_net_guides(state->interaction.net_guides(),
                                              state->placement.net,
                                              kCubeSize));
    }

    if (flat.rings) {
        // The same rotation again. The rings a press is offering come with it,
        // since picking one out is a matter of drawing it heavier rather than
        // adding a line.
        graphics::append_scene(
            scene, graphics::build_rings_scene(
                       state->cube_state, state->placement.rings,
                       state->interaction.active_rotation(),
                       state->interaction.rings_guides()));
    }

    return state->renderer->render(scene);
}

bool scramble(std::uint32_t seed, std::uint32_t move_count) noexcept
{
    if (!state) return false;

    // Everything that could refuse comes before anything is changed, so a
    // rejected count leaves the cube and any playback exactly as they were.
    // Zero would be a scramble that leaves the cube solved; the upper bound is
    // against a number typed into the box that would never finish playing.
    if (move_count == 0 || move_count > kMaxScrambleMoves) return false;

    auto plan = cube::make_scramble(kCubeSize, seed, move_count);
    if (plan.empty()) return false;

    // The cube restarts but the viewpoint does not, so a sweep the controller
    // has not published yet still counts. Everything else goes, including a
    // snap that would otherwise commit onto the new cube.
    discard_playback();
    state->cube_state = cube::CubeState(kCubeSize);
    state->user_move_count = 0;

    // Turned rather than applied: the cube is still solved when this returns
    // and arrives at the scrambled state a few hundred frames later.
    // Runs out and has nowhere to go back to: a scramble is where the cube is
    // meant to be left, not an interlude.
    state->playback =
        Player{std::move(plan), 0, kScrambleTempoMs, false, std::nullopt};
    return true;
}

void reset_cube() noexcept
{
    if (!state) return;

    // Same split as scramble(): the cube is the only thing this command owns.
    discard_playback();
    state->cube_state = cube::CubeState(kCubeSize);
    state->user_move_count = 0;
}

const std::vector<cube::CubeMove>& ambient_pattern(
    std::uint32_t choice) noexcept
{
    return ambient_patterns()[choice % kAmbientPatternCount];
}

bool ambient_start(std::uint32_t choice) noexcept
{
    if (!state) return false;
    if (ambient_running()) return false;

    // Whatever else was playing gives way, the way it does for a scramble.
    // Nothing is put back on the way in: the cube being watched is the cube
    // that was there, which is exactly what the snapshot below records.
    discard_playback();

    state->playback = Player{ambient_pattern(choice), 0, kAmbientTempoMs, true,
                             state->cube_state};
    return true;
}

void ambient_stop() noexcept
{
    if (!state || !ambient_running()) return;

    // Taken out of the player before the player is thrown away, and put back
    // afterwards: the discard is what stops a turn already in flight, and a
    // cube restored ahead of it would have that turn land on top of it.
    cube::CubeState origin = std::move(*state->playback->restore_to);
    discard_playback();
    state->cube_state = std::move(origin);
}

bool is_ambient() noexcept
{
    return state && ambient_running();
}

bool is_solved() noexcept
{
    return state && state->cube_state.is_solved();
}

std::uint32_t committed_move_count() noexcept
{
    return state ? state->user_move_count : 0;
}

bool turn_face(cube::Face face, int face_turns) noexcept
{
    if (!state) return false;
    if (!valid_face(face)) return false;
    if (face_turns != -1 && face_turns != 1 && face_turns != 2) return false;

    // The controller does not know about playback, and between a sequence
    // being accepted and its first frame it is idle -- so without this the
    // very next keypress would land in the middle of the sequence.
    if (busy()) return false;

    cube::CubeMove move = clockwise_move(face);
    move.quarter_turns *= face_turns;
    return state->interaction.start_move(move);
}

bool set_view_mode(graphics::ViewMode mode) noexcept
{
    if (!state || !valid_view_mode(mode)) return false;
    if (mode == state->view_mode) return true;

    // Preserve every orbit step produced before the command, then end only
    // gestures that still depend on the old viewport. Snaps survive cancel.
    drain_orbit();
    state->interaction.cancel();
    state->view_mode = mode;
    adopt_surface(state->surface_width, state->surface_height);
    return true;
}

graphics::ViewMode view_mode() noexcept
{
    return state ? state->view_mode : graphics::ViewMode::Both;
}

bool set_flat_style(graphics::FlatStyle style) noexcept
{
    if (!state || !valid_flat_style(style)) return false;
    if (style == state->flat_style) return true;

    // A drag holds a rectangle that is about to move or vanish, so it goes the
    // same way a resize sends it. A snap only animates an angle and carries on.
    state->interaction.cancel();
    state->flat_style = style;
    adopt_surface(state->surface_width, state->surface_height);
    return true;
}

graphics::FlatStyle flat_style() noexcept
{
    return state ? state->flat_style : graphics::FlatStyle::Net;
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

std::uintptr_t pixel_buffer() noexcept
{
    if (!state) return 0;

    return state->renderer->pixel_buffer();
}

std::uint32_t pixel_byte_length() noexcept
{
    if (!state) return 0;

    return state->renderer->pixel_byte_length();
}

void shutdown() noexcept
{
    if (!state) return;

    // Destroying the state releases everything it owns, the renderer
    // included, so ThorVG is only terminated afterwards.
    state.reset();
    static_cast<void>(tvg::Initializer::term());
}

bool is_initialized() noexcept
{
    return static_cast<bool>(state);
}

}  // namespace rubiks::app
