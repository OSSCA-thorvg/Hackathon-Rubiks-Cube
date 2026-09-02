#include "app/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <thorvg.h>

#include "cube/CubeState.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/PackedMove.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"
#include "cube/solver/Solver.hpp"
#include "graphics/AxisGizmo.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Light.hpp"
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

/** The size a lifecycle opens with, and the one every phase before this had. */
constexpr int kDefaultCubeSize = 3;

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
 * The same for a rewind, which is watched rather than made, like a scramble.
 *
 * A constant of its own despite the matching value: a solve can be dozens of
 * moves and is the one sequence a user waits through, so it is the first thing
 * that would be tuned, and tuning it should not quietly change how a scramble
 * arrives.
 */
constexpr double kRewindTempoMs = kScrambleTempoMs;

/**
 * The bounds a speed multiplier is held inside.
 *
 * Four times either way. Faster than that and a quarter turn is over inside
 * two frames, so a sequence stops being an animation and becomes a slideshow;
 * slower and a scramble outlasts anyone's patience for watching it.
 */
constexpr float kMinSpeedScale = 0.25f;
constexpr float kMaxSpeedScale = 4.0f;

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

/**
 * One scripted sequence being played back, with how to play it.
 *
 * A value rather than a queue and a flag beside it: a sequence and the way it
 * is being played are set up together and die together, and two pieces of
 * state with one lifetime are one object. Fields here rather than names in an
 * enum of where the moves came from, because nothing downstream asks where
 * they came from, only what to do with them.
 */
struct Player {
    std::vector<cube::CubeMove> plan;
    std::size_t next = 0;
    /** Milliseconds per quarter turn, as `start_move` takes it. */
    double tempo_ms;
    /**
     * What each commit of this sequence does to the record.
     *
     * Deliberately without a default: every producer says it, because a
     * sequence handed the wrong one damages the record rather than merely
     * looking wrong. Sitting ahead of the fields that do have defaults is what
     * makes leaving it out visible at the call site.
     */
    cube::TimelineEffect timeline_effect;
    /** Whether running out of moves starts the sequence again. */
    bool repeats = false;
    /**
     * Whether stop_playback() may break this sequence off.
     *
     * Off by default, which is what protects the two sequences that must run
     * to their end: a half-played scramble is a cube nobody asked for, and a
     * watched pattern stopped this way would never put its cube back. Only a
     * rewind or a replay turns it on -- and it is the engine's answer rather
     * than the screen's, so a press arriving a frame after the controls went
     * away cannot reach the wrong sequence.
     */
    bool stoppable = false;
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

    // How much faster than the written tempos everything turns. One value for
    // every animation, because a person setting a speed means the speed of the
    // cube and not of one of the ways it can be moved.
    float speed_scale = 1.0f;

    // The logical cube, only ever holding quarter turns. A drag in progress
    // lives in the controller instead, and reaches this through a committed
    // move.
    //
    // How many layers the cube has is read off this and stored nowhere else.
    // The controller holds a size of its own because it is the rule it
    // refuses moves by and it has no cube to ask -- so a change of size
    // replaces it rather than telling it, which is what makes "a gesture from
    // the cube before" a state that cannot be written down.
    cube::CubeState cube_state{kDefaultCubeSize};
    interaction::InteractionController interaction{kDefaultCubeSize};
    graphics::ViewMode view_mode = graphics::ViewMode::Both;
    graphics::FlatStyle flat_style = graphics::FlatStyle::Net;

    // Which six shades the stickers are drawn in. Nothing but drawing reads
    // it, so it is the one setting a turn in progress can be changed under.
    graphics::Palette palette = graphics::Palette::Classic;

    // The light the 3D view is drawn under, in world space. A constant for
    // now, kept as state so that moving it later is a matter of writing here.
    graphics::Light light = graphics::kDefaultLight;

    // The ground behind the cube. Kept here rather than only on the renderer
    // so that a query has an answer without reaching through the rendering
    // boundary, and so the two cannot drift: every write goes through
    // set_canvas_theme(), which sets both.
    graphics::CanvasTheme canvas_theme = graphics::CanvasTheme::Dark;

    // Everything that has happened to the cube, as one sequence. How many
    // moves are the user's own is read off it rather than counted alongside
    // it, so there is no second number to keep in step.
    cube::MoveTimeline timeline;

    // The solvers this application has, each held by the interface rather than
    // by its type. Adding one is a line here and nothing that reads them, and
    // which of them answers for a cube is a question they answer themselves.
    // Held rather than made per call because a solver is allowed to have built
    // something it would be a waste to build twice -- one of these builds a
    // workshop for every size it is asked about and keeps it.
    std::vector<std::unique_ptr<cube::solver::Solver>> solvers = [] {
        std::vector<std::unique_ptr<cube::solver::Solver>> made;
        made.push_back(std::make_unique<cube::solver::LayerByLayer>());
        made.push_back(std::make_unique<cube::solver::Reduction>());
        return made;
    }();

    // The whole of the playback state. Its presence is what "a sequence is
    // playing" means, so there is no second flag to fall out of step with it.
    std::optional<Player> playback;

    /**
     * A colouring on its way to becoming the cube, when somebody is making one.
     *
     * Its presence is what "is being painted" means, so there is no flag
     * beside it to fall out of step. The reading beside the colours is of the
     * last refusal, kept so that what to mend stays on screen while it is
     * being mended rather than vanishing with the press that asked.
     */
    struct PaintDraft {
        std::vector<cube::FaceColor> stickers;
        cube::FaceColor brush = cube::FaceColor::White;
        cube::PaintReading reading;

        // Whether a press covers the face it lands on rather than the one
        // square. Held with the draft because it means nothing without one.
        bool filling = false;

        // Whether a press is still down. A drag across the net colours every
        // cell it crosses, which is how a face gets copied off a real cube
        // without lifting the finger between squares.
        bool stroking = false;
    };
    std::optional<PaintDraft> paint;

    /**
     * The colouring this session began from, when it began from one.
     *
     * Empty for a session that began from a scramble or a reset, so its
     * emptiness is also the answer to "does a link for this need the
     * colours". A painted cube cannot be written down as moves -- nothing
     * from solved arrives at it without solving it first -- so a link has to
     * carry these, and something has to remember them.
     */
    std::vector<cube::FaceColor> origin;

    // Where a colouring arriving from a link is written before it is read.
    // Empty except between painting_buffer() and restore_painting(), the same
    // lifetime and for the same reason as the record's buffer below.
    std::vector<std::uint8_t> painting_bytes;

    // Where a shared record is written before it is read. Empty except
    // between restore_buffer() and restore_apply(), which is the whole of its
    // lifetime -- so being empty is also what "no buffer was taken" means,
    // and there is no flag beside it saying the same.
    std::vector<std::uint32_t> restore;
};

std::unique_ptr<ApplicationState> state;

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

/**
 * The move a face names, taken as deep as a range of depths says.
 *
 * The one place a depth becomes a layer mask, so which end of an axis a face
 * sits at and which way it turns from outside are each written once. Depth 1
 * is the face; `(Right, 1, 1)` is R and `(Right, 1, 2)` is Rw.
 *
 * Nothing for a range no move corresponds to: empty, reversed, off the far
 * side of the cube, or the whole of it -- the last being a rotation, which
 * this application neither makes nor writes down.
 */
[[nodiscard]] std::optional<cube::CubeMove> depth_move(cube::Face face,
                                                       int first_depth,
                                                       int last_depth,
                                                       int size) noexcept
{
    const cube::LayerMask layers =
        cube::depth_layers(face, first_depth, last_depth, size);
    if (layers == 0) return std::nullopt;
    if (last_depth - first_depth + 1 >= size) return std::nullopt;

    // Clockwise from outside is a positive turn about the axis only for the
    // face at its positive end; the face opposite reads the axis backwards.
    const int direction = cube::outer_layer(face, size) == size - 1 ? 1 : -1;
    return cube::CubeMove{cube::axis_of(face), layers, direction};
}

/**
 * How much of the cube one move of a watched pattern takes with it.
 *
 * Said as a share of the cube rather than as a depth, because the table is
 * written once for every size: `Wide` is half the cube and `Slice` is the
 * layer at the bottom of that half, both measured with the same
 * `size / 2` the scramble picks its depths inside. On a 2x2 and a 3x3 that
 * half is one layer, so all three of these are the face turn and those two
 * cubes are watched exactly as they were before there were others.
 */
enum class AmbientDepth { Face, Wide, Slice };

/** One move of a watched pattern, before it is given a cube to turn. */
struct AmbientStep {
    cube::Face face;
    AmbientDepth depth;
    int face_turns;
};

/**
 * The patterns the watching mode repeats, one of which a choice picks out.
 *
 * Faces and shares rather than moves, because the cube they will turn is not
 * known here: the same four are offered at every size, and what a face and a
 * half of it are depends on the cube in hand. Which of them comes back round
 * soonest is worked out in the phase document and is what put these four here
 * rather than four others; carrying those numbers in the code would be
 * carrying values nothing reads.
 *
 * Four moves, and on a big cube four moves is not much of a cube to look at --
 * which is what the winding either side of them is for, below.
 *
 * The comments write each one as it reads on a 4x4, where the half is two
 * layers: `Rw` is the half, `2R` the slice behind the face, `R` the face.
 */
constexpr std::array<std::array<AmbientStep, 4>, kAmbientPatternCount>
    kAmbientPatterns{{
        // 2R Uw' Dw' 2F
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Up, AmbientDepth::Wide, -1},
          {cube::Face::Down, AmbientDepth::Wide, -1},
          {cube::Face::Front, AmbientDepth::Slice, 1}}},
        // 2R Uw 2F Dw
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Up, AmbientDepth::Wide, 1},
          {cube::Face::Front, AmbientDepth::Slice, 1},
          {cube::Face::Down, AmbientDepth::Wide, 1}}},
        // Rw 2B 2D' L2
        {{{cube::Face::Right, AmbientDepth::Wide, 1},
          {cube::Face::Back, AmbientDepth::Slice, 1},
          {cube::Face::Down, AmbientDepth::Slice, -1},
          {cube::Face::Left, AmbientDepth::Face, 2}}},
        // 2R 2L' Uw F2
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Left, AmbientDepth::Slice, -1},
          {cube::Face::Up, AmbientDepth::Wide, 1},
          {cube::Face::Front, AmbientDepth::Face, 2}}},
    }};

/**
 * How deep the walk below reaches on a cube of `size`.
 *
 * Half of it, rounded up, so an odd cube's middle layer is included: reached
 * from either face it is the same layer, and leaving it out would be the one
 * band of each axis that never moved.
 */
[[nodiscard]] int ambient_walk_depth(int size) noexcept
{
    return std::max(1, (size + 1) / 2);
}

/**
 * How many moves a cube of `size` is watched with.
 *
 * The written pattern for a cube with no layers inside; for one that has
 * them, a walk long enough to grip it by every face at every depth exactly
 * once -- six faces by however deep the walk reaches.
 *
 * Four moves on nine layers turn the same few bands over and over: most of
 * the cube sits still, the rest churns in place, and it stays that way
 * however long the watching runs.
 */
[[nodiscard]] int ambient_walk_length(int size) noexcept
{
    return size < 4 ? 0 : 6 * ambient_walk_depth(size);
}

/**
 * One move of the walk, worked out from where it falls rather than listed.
 *
 * Written as a rule rather than a table because what it has to do is cover
 * the cube, and covering is what a rule is good at. The face steps round all
 * six on every move and the depth only after all six have had a turn, so the
 * two counters do not fall into step and the walk grips every face at every
 * depth exactly once. Every other move takes the layers above its depth along
 * instead of turning one alone, and every third turns back the other way, so
 * what is on screen is a slice sliding, then a block swinging, rather than
 * one steady wind.
 *
 * Reaching every depth from every face is what reaches every band: a layer
 * this walk does not grip from one side it grips from the other.
 *
 * The cost is the one thing a written table bought, and it is paid knowingly.
 * A sequence that mixes the whole of a big cube is an element of a very large
 * group, so the number of rounds before it comes back to a solved cube is
 * astronomical -- where four hand-picked moves came back inside a couple of
 * hundred. The two cannot both be had: staying near the solved cube is the
 * same thing as leaving most of the cube alone, and that is what made a big
 * one not worth watching. Nothing rests on the return, because what puts the
 * cube back is the snapshot the watching took.
 *
 * Nothing for a move the cube cannot make, which the walk does not produce:
 * its deepest reach is half the cube and its widest is that same half.
 */
[[nodiscard]] std::optional<cube::CubeMove> ambient_walk_move(
    std::uint32_t choice, int index, int size) noexcept
{
    constexpr std::array<cube::Face, 6> kWalkFaces{
        cube::Face::Right, cube::Face::Up,   cube::Face::Front,
        cube::Face::Left,  cube::Face::Down, cube::Face::Back};

    const int deepest = ambient_walk_depth(size);
    const int pass = index / static_cast<int>(kWalkFaces.size());
    const auto turn = static_cast<int>(choice % kAmbientPatternCount);

    const auto face_index = static_cast<std::size_t>(index + turn);
    const cube::Face face = kWalkFaces[face_index % kWalkFaces.size()];

    // The depth moves on once the faces have come round, which is what keeps
    // the two from repeating together. Where it starts is the choice's, so
    // the four watchings do not open the same way.
    const int depth = 1 + (pass + turn) % deepest;

    // A slice, then a half, then a slice again: one carries a single layer
    // round and the other takes everything above it along, and having both on
    // screen is what a cube with layers inside has to show.
    const bool wide = (index + pass) % 2 != 0;

    auto move = depth_move(face, wide ? 1 : depth, depth, size);
    if (!move) return std::nullopt;

    if (index % 3 == 2) move->quarter_turns = -move->quarter_turns;
    if (choice % 2 != 0) move->quarter_turns = -move->quarter_turns;
    return move;
}

/** The depth range one share of a cube of `size` stands for. */
[[nodiscard]] std::pair<int, int> ambient_depths(AmbientDepth depth,
                                                 int size) noexcept
{
    // The same half the scramble measures its depths inside, so "how deep a
    // move goes on this cube" has one answer in this file.
    const int deepest = size / 2;

    switch (depth) {
        case AmbientDepth::Wide:
            return {1, deepest};
        case AmbientDepth::Slice:
            return {deepest, deepest};
        case AmbientDepth::Face:
            break;
    }
    return {1, 1};
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
[[nodiscard]] bool valid_palette(graphics::Palette palette) noexcept
{
    switch (palette) {
        case graphics::Palette::Classic:
        case graphics::Palette::HighContrast:
            return true;
    }
    return false;
}

/** Reports whether an enum arriving through a primitive boundary is valid. */
[[nodiscard]] bool valid_canvas_theme(graphics::CanvasTheme theme) noexcept
{
    switch (theme) {
        case graphics::CanvasTheme::Light:
        case graphics::CanvasTheme::Dark:
            return true;
    }
    return false;
}

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

/** How many of the moves on the cube right now are the user's own. */
[[nodiscard]] std::size_t applied_user_moves() noexcept
{
    const auto& timeline = state->timeline;
    return timeline.cursor() > timeline.scramble_end()
               ? timeline.cursor() - timeline.scramble_end()
               : 0;
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
 * Forgets where the session began, which every fresh cube does.
 *
 * Beside `discard_painting()` rather than inside it: closing a draft is not
 * the same as replacing the cube, and only the second of those makes the
 * colouring this session started from stop being true.
 */
void forget_origin() noexcept
{
    state->origin.clear();
}

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
        if (state->interaction.start_move(
                player.plan[player.next++],
                scaled_tempo(player.tempo_ms, state->speed_scale))) {
            return;
        }
    }

    // Nothing left to take, and nothing left turning.
    state->playback.reset();
}

/**
 * Hands a rewind or a replay to the player, if there is anything in it.
 *
 * What the three commands share once each has worked out its own target: the
 * plan is made in full here and consumed by the ordinary playback loop, so
 * this phase adds no second way of playing moves into the cube. An empty plan
 * is a refusal, which is what a command with nothing to do amounts to.
 */
[[nodiscard]] bool play_rewind(std::vector<cube::CubeMove> plan,
                               cube::TimelineEffect effect) noexcept
{
    if (plan.empty()) return false;

    // Stoppable, unlike either of the other two producers: this is the one
    // sequence a user waits through, and a solve can be dozens of moves.
    state->playback = Player{std::move(plan), 0,    kRewindTempoMs, effect,
                             false,           true, std::nullopt};
    return true;
}

/** Whether a rewind may be started at all, whatever it would rewind. */
[[nodiscard]] bool can_rewind() noexcept
{
    return state && !busy();
}

/**
 * Whether a layer set is one a shared record may carry, for a cube of `size`.
 *
 * One unbroken run of that cube's layers, short of all of them: exactly the
 * moves this application can make and can write down. The mask field is wide
 * enough for the slices of a much larger cube and for sets with gaps in them,
 * and a word arriving from a link is free to name any of those -- but nothing
 * here can make one, so letting one in would put a move on the record that has
 * no notation to be written in and no command that could have produced it.
 */
[[nodiscard]] bool restorable_layers(cube::LayerMask layers,
                                     int size) noexcept
{
    return cube::is_layer_run(layers, size) &&
           layers != cube::layers_through(0, size - 1);
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

    // A draft turns the net from something to turn into something to colour.
    // The press is answered here and goes no further: nothing about the cube
    // may move while somebody is writing down what is on theirs, and a drag
    // from this press carries the brush rather than a layer.
    if (state->paint) {
        state->paint->stroking = state->paint->filling ? fill_face_at(x, y)
                                                       : paint_at(x, y);
        return state->paint->stroking;
    }

    // The two drawings never share a rectangle, so asking both in turn cannot
    // give two answers.
    std::optional<interaction::NetPick> net_pick;
    std::optional<interaction::RingsPick> rings_pick;
    if (flat.net) {
        net_pick =
            interaction::pick_net(x, y, state->placement.net, size_of_cube());
    }
    if (!net_pick && flat.rings) {
        rings_pick = interaction::pick_rings(x, y, state->placement.rings,
                                             size_of_cube());
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

    // A drag across the net colours every cell it crosses, which is how a face
    // is copied off a real one without lifting the finger between squares.
    if (state->paint) {
        if (state->paint->stroking) static_cast<void>(paint_at(x, y));
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

bool render() noexcept
{
    if (!state) return false;

    // The model transform is identity for now, but it goes through the real
    // path so the quaternion-to-matrix step is exercised every frame.
    const math::Transform model;
    const graphics::Camera camera = current_camera();

    const auto shown = visible_views(state->view_mode);

    const auto flat = flat_parts();

    graphics::RenderScene scene;
    if (shown.cube) {
        // The ground the shadow may lie on: the canvas, less whatever flat
        // view shares it below. The cube's viewport is a square in the middle
        // of that, and its shadow leans out of the square into the margins.
        graphics::Rect stage{0.0f, 0.0f,
                             static_cast<float>(state->surface_width),
                             static_cast<float>(state->surface_height)};
        if (flat.net) stage.height = std::min(stage.height, state->placement.net.y);
        if (flat.rings) {
            stage.height = std::min(stage.height, state->placement.rings.y);
        }

        // The high-contrast palette exists to keep six shades apart for eyes
        // that would otherwise merge them; shading those shades would undo
        // that, so it is drawn under a light that leaves every colour as it
        // is. The passes run either way -- it is a value, not a branch.
        const graphics::Light lamp =
            state->palette == graphics::Palette::HighContrast
                ? graphics::Light::unlit()
                : state->light;

        scene = graphics::build_cube_scene(state->cube_state,
                                           state->interaction.active_rotation(),
                                           state->palette)  //
                | graphics::transform(model)                                  //
                | graphics::shadow(lamp, camera)                              //
                | graphics::view(camera)                                      //
                | graphics::light(lamp, camera)                               //
                | graphics::project(camera)                                   //
                | graphics::cull()                                            //
                | graphics::depth_sort()                                      //
                | graphics::viewport(state->placement.cube, stage);

        // Which way each axis points from here, in the colors the net's guide
        // lines use, so a cyan loop over there and a cyan arm over here are
        // plainly the same axis.
        graphics::append_scene(
            scene, graphics::build_axis_gizmo(camera, state->placement.cube));
    }

    if (flat.net && state->paint) {
        // A draft is drawn instead of the cube, because a draft is what is
        // being edited and mostly is not a cube at all until the last square
        // is right. No guides either: nothing here turns, so there is nowhere
        // for a guide to promise.
        graphics::append_scene(
            scene, graphics::build_net_painting(
                       state->paint->stickers, size_of_cube(),
                       state->placement.net, state->palette,
                       state->paint->reading.blamed));
    } else if (flat.net) {
        // The net is already screen-space, so it only has to be appended. It
        // gets the same rotation as the 3D scene, which is what makes one
        // gesture move every view in the same frame.
        graphics::append_scene(
            scene, graphics::build_net_scene(
                       state->cube_state, state->placement.net,
                       state->interaction.active_rotation(), state->palette));

        // The rings the pressed cell could turn on, drawn over the stickers
        // so the gesture says where it is about to go before it goes there.
        graphics::append_scene(
            scene, graphics::build_net_guides(state->interaction.net_guides(),
                                              state->placement.net,
                                              size_of_cube()));
    }

    if (flat.rings) {
        // The same rotation again. The rings a press is offering come with it,
        // since picking one out is a matter of drawing it heavier rather than
        // adding a line.
        graphics::append_scene(
            scene, graphics::build_rings_scene(
                       state->cube_state, state->placement.rings,
                       state->interaction.active_rotation(),
                       state->interaction.rings_guides(), state->palette));
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

    auto plan = cube::make_scramble(size_of_cube(), seed, move_count);
    if (plan.empty()) return false;

    // The cube restarts but the viewpoint does not, so a sweep the controller
    // has not published yet still counts. Everything else goes, including a
    // snap that would otherwise commit onto the new cube, and a colouring of
    // the cube that is about to stop existing.
    discard_painting();
    forget_origin();
    discard_playback();
    state->cube_state = cube::CubeState(size_of_cube());

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
    discard_painting();
    forget_origin();
    discard_playback();
    state->cube_state = cube::CubeState(size_of_cube());
    state->timeline.clear();
}

bool set_cube_size(int size) noexcept
{
    if (!state || !valid_cube_size(size)) return false;

    // Already this size, so nothing to do and nothing to throw away. The same
    // answer the view commands give, and for the same reason: asking for what
    // is there is not a command.
    if (size == size_of_cube()) return true;

    // A reset with one thing added. The controller is replaced rather than
    // told, because the size is the rule it turns moves down by and a gesture
    // half-made on the cube before is not something to carry across.
    reset_cube();
    state->cube_state = cube::CubeState(size);
    state->interaction = interaction::InteractionController(size);
    return true;
}

int cube_size() noexcept
{
    return state ? size_of_cube() : 0;
}

namespace {

/**
 * Which sticker of the whole surface a cell of the net is.
 *
 * The net names a cell by face, column and row; a painting numbers it by where
 * `surface_stickers()` counts it. This is the one place the two meet, and it
 * is here rather than in either of them because neither should have to know
 * the other: the domain has no net, and the drawing has no painting.
 */
[[nodiscard]] int painted_slot(const interaction::NetPick& pick, int size)
{
    // Worked out once for a size and kept, because filling a face asks this
    // once per cell and a drag asks it once per pointer move. Built fresh each
    // time it walked the whole surface looking for one sticker -- four
    // thousand seven hundred of them at the widest cube, times the seven
    // hundred and eighty-four cells of a face being filled.
    static std::map<int, std::vector<int>> known;

    auto found = known.find(size);
    if (found == known.end()) {
        std::vector<int> at(static_cast<std::size_t>(size) * size * size *
                                cube::kFaceCount,
                            -1);
        const auto all = cube::surface_stickers(size);
        for (std::size_t i = 0; i < all.size(); ++i) {
            const auto& s = all[i];
            at[static_cast<std::size_t>((s.x * size + s.y) * size + s.z) *
                   cube::kFaceCount +
               cube::face_index(s.face)] = static_cast<int>(i);
        }
        found = known.emplace(size, std::move(at)).first;
    }

    const auto cell = graphics::net_cell(pick.face, pick.col, pick.row, size);
    return found->second[static_cast<std::size_t>(
                             (cell.x * size + cell.y) * size + cell.z) *
                             cube::kFaceCount +
                         cube::face_index(cell.face)];
}

/** The cell a point is over, when the net is on screen and holds the point. */
[[nodiscard]] std::optional<interaction::NetPick> net_cell_at(float x,
                                                              float y) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    if (!flat_parts().net) return std::nullopt;
    return interaction::pick_net(x, y, state->placement.net, size_of_cube());
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

    ApplicationState::PaintDraft draft;
    draft.stickers = cube::painting_of(state->cube_state);
    state->paint = std::move(draft);
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
    state->paint->brush = colour;
    return true;
}

cube::FaceColor brush() noexcept
{
    if (!state || !state->paint) return cube::FaceColor::White;
    return state->paint->brush;
}

bool paint_at(float x, float y) noexcept
{
    if (!state || !state->paint) return false;

    const auto pick = net_cell_at(x, y);
    if (!pick) return false;

    const int slot = painted_slot(*pick, size_of_cube());
    if (slot < 0) return false;

    state->paint->stickers[static_cast<std::size_t>(slot)] =
        state->paint->brush;
    return true;
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

bool fill_face_at(float x, float y) noexcept
{
    if (!state || !state->paint) return false;

    const auto pick = net_cell_at(x, y);
    if (!pick) return false;

    const int size = size_of_cube();
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const int slot =
                painted_slot(interaction::NetPick{pick->face, col, row}, size);
            if (slot < 0) continue;
            state->paint->stickers[static_cast<std::size_t>(slot)] =
                state->paint->brush;
        }
    }
    return true;
}

int painted_count(cube::FaceColor colour) noexcept
{
    if (!state || !state->paint) return 0;

    int found = 0;
    for (const auto painted : state->paint->stickers) {
        if (painted == colour) ++found;
    }
    return found;
}

bool apply_painting() noexcept
{
    if (!state || !state->paint) return false;

    // Read once. `assembled()` reads the painting on its own account, so
    // asking for the reading first would run every check twice -- and at the
    // widest cube a check is every piece against every one of twenty-four ways
    // of holding it. What a refusal needs is fetched when there is one.
    const int size = size_of_cube();
    auto built = cube::assembled(size, state->paint->stickers);
    if (!built) {
        state->paint->reading =
            cube::read_painting(size, state->paint->stickers);
        return false;
    }
    state->paint->reading = cube::PaintReading{};

    // Past every refusal. What arrives is a starting position and not a move,
    // so the record goes the way it goes for a change of size -- and a rewind
    // afterwards comes back to this cube rather than to a solved one, which is
    // what a person who painted their own cube would expect it to mean.
    discard_playback();
    state->cube_state = std::move(*built);
    state->interaction = interaction::InteractionController(size);
    state->timeline.clear();

    // Kept before the draft goes, because this is the one thing about the
    // draft that outlives it: where the session began.
    state->origin = std::move(state->paint->stickers);
    discard_painting();
    return true;
}

cube::PaintFault painting_fault() noexcept
{
    if (!state || !state->paint) return cube::PaintFault::None;
    return state->paint->reading.fault;
}

int painting_blamed_count() noexcept
{
    if (!state || !state->paint) return 0;
    return static_cast<int>(state->paint->reading.blamed.size());
}

int painting_blamed_at(int index) noexcept
{
    if (!state || !state->paint) return -1;
    const auto& blamed = state->paint->reading.blamed;
    if (index < 0 || static_cast<std::size_t>(index) >= blamed.size()) {
        return -1;
    }
    return blamed[static_cast<std::size_t>(index)];
}

const std::vector<cube::FaceColor>& painting_draft() noexcept
{
    static const std::vector<cube::FaceColor> none;
    if (!state || !state->paint) return none;
    return state->paint->stickers;
}

std::vector<cube::FaceColor> cube_painting() noexcept
{
    if (!state) return {};
    return cube::painting_of(state->cube_state);
}

std::uint32_t origin_painting_count() noexcept
{
    if (!state) return 0;
    return static_cast<std::uint32_t>(state->origin.size());
}

int origin_painting_at(std::uint32_t index) noexcept
{
    if (!state || index >= state->origin.size()) return -1;
    return static_cast<int>(state->origin[index]);
}

std::uintptr_t painting_buffer(std::uint32_t count) noexcept
{
    if (!state) return 0;

    // The one count a cube of some size this application builds could have.
    // Refusing anything else is what keeps zero meaning "refused" for the
    // address as well.
    bool fits = false;
    for (int size = kMinCubeSize; size <= kMaxCubeSize; ++size) {
        if (count == static_cast<std::uint32_t>(6 * size * size)) fits = true;
    }
    if (!fits) return 0;

    state->painting_bytes.assign(count, 0);
    return reinterpret_cast<std::uintptr_t>(state->painting_bytes.data());
}

bool restore_painting(int size, std::uint32_t user_count) noexcept
{
    if (!state) return false;
    if (!valid_cube_size(size)) return false;

    // Both buffers are taken out before either is read, whichever way this
    // goes: they belong to one restore, so a second call with nothing written
    // into them is refused by the same emptiness that refuses a call without
    // them at all.
    std::vector<std::uint8_t> colours;
    colours.swap(state->painting_bytes);
    std::vector<std::uint32_t> words;
    words.swap(state->restore);

    if (colours.size() != static_cast<std::size_t>(6 * size * size)) {
        return false;
    }
    if (words.size() != user_count) return false;

    std::vector<cube::FaceColor> painting;
    painting.reserve(colours.size());
    for (const std::uint8_t colour : colours) {
        if (colour >= cube::kFaceCount) return false;
        painting.push_back(static_cast<cube::FaceColor>(colour));
    }

    std::vector<cube::CubeMove> moves;
    moves.reserve(words.size());
    for (const std::uint32_t word : words) {
        const auto move = cube::unpack(word);
        if (!move || !restorable_layers(move->layers, size)) return false;
        moves.push_back(*move);
    }

    // The last refusal, and the one this whole feature exists for: a colouring
    // that no turning reaches is not a cube, and a solver handed one would
    // look for a position that does not exist.
    auto built = cube::assembled(size, painting);
    if (!built) return false;

    // Past every refusal, so what follows cannot leave the cube half restored.
    discard_painting();
    discard_playback();
    state->cube_state = std::move(*built);
    state->interaction = interaction::InteractionController(size);
    state->timeline.clear();
    state->origin = std::move(painting);

    // The colouring is the starting position, so what follows it is the user's
    // own -- recorded the ordinary way, which is what happened to the cube
    // this link came from.
    for (const auto& move : moves) {
        state->cube_state.apply(move);
        state->timeline.record(move);
    }
    return true;
}

std::uintptr_t restore_buffer(std::uint32_t total_count) noexcept
{
    if (!state) return 0;

    // Nothing to restore is not a shorter restore: the caller has decoded a
    // record and is here because it holds something, so an empty one is a
    // fault rather than a case -- and refusing it is what keeps zero meaning
    // "refused" for the address as well.
    if (total_count == 0 || total_count > kMaxRestoreMoves) return 0;

    state->restore.assign(total_count, 0);
    return reinterpret_cast<std::uintptr_t>(state->restore.data());
}

bool restore_apply(int size, std::uint32_t scramble_count,
                   std::uint32_t user_count) noexcept
{
    if (!state) return false;
    if (!valid_cube_size(size)) return false;

    // Taken out before it is read, whichever way this goes: the buffer belongs
    // to one restore, so a second call with nothing written into it is refused
    // by the same emptiness that refuses a call without a buffer at all.
    std::vector<std::uint32_t> words;
    words.swap(state->restore);
    if (words.empty()) return false;

    // Widened before it is added. On wasm32 a size_t is thirty-two bits, so
    // two counts that wrap would agree with the buffer's length while one of
    // the two loops below walked off the end of it.
    const std::uint64_t declared =
        static_cast<std::uint64_t>(scramble_count) + user_count;
    if (declared != words.size()) return false;

    std::vector<cube::CubeMove> moves;
    moves.reserve(words.size());
    for (const std::uint32_t word : words) {
        const auto move = cube::unpack(word);
        if (!move || !restorable_layers(move->layers, size)) return false;
        moves.push_back(*move);
    }

    // Past every refusal, so what follows cannot leave the cube half restored
    // -- the size included, which is why it arrives with the record rather
    // than through a call of its own before it.
    discard_painting();
    forget_origin();
    discard_playback();
    state->cube_state = cube::CubeState(size);
    state->interaction = interaction::InteractionController(size);

    // Built through the ordinary operations rather than through a way in of
    // its own: the scramble is begun and then walked up, and the user's moves
    // are recorded on top of it, which is exactly what happened to the cube
    // this record came from.
    const auto scramble_end = static_cast<std::size_t>(scramble_count);
    state->timeline.begin_scramble(std::vector<cube::CubeMove>(
        moves.begin(), moves.begin() + static_cast<std::ptrdiff_t>(scramble_end)));

    for (std::size_t index = 0; index < moves.size(); ++index) {
        state->cube_state.apply(moves[index]);
        if (index < scramble_end) {
            state->timeline.step(cube::TimelineEffect::Advance);
        } else {
            state->timeline.record(moves[index]);
        }
    }

    return true;
}

std::vector<cube::CubeMove> ambient_pattern(std::uint32_t choice, int size)
{
    if (!valid_cube_size(size)) return {};

    // A cube with layers inside is walked over rather than played a written
    // pattern into: the pattern is four moves, and four moves of a 9x9 is not
    // a cube anybody would watch.
    const int walk = ambient_walk_length(size);
    if (walk > 0) {
        std::vector<cube::CubeMove> pattern;
        pattern.reserve(static_cast<std::size_t>(walk));
        for (int index = 0; index < walk; ++index) {
            const auto move = ambient_walk_move(choice, index, size);
            if (!move) return {};

            pattern.push_back(*move);
        }
        return pattern;
    }

    std::vector<cube::CubeMove> pattern;
    for (const AmbientStep& step :
         kAmbientPatterns[choice % kAmbientPatternCount]) {
        // Where the table meets the cube being watched: a face and a share of
        // it become the layers this size actually has. Every share is the
        // face itself at these two sizes, which is what they have always been
        // watched with.
        const auto [first, last] = ambient_depths(step.depth, size);
        auto move = depth_move(step.face, first, last, size);
        if (!move) return {};

        move->quarter_turns *= step.face_turns;
        pattern.push_back(*move);
    }
    return pattern;
}

bool ambient_start(std::uint32_t choice) noexcept
{
    if (!state) return false;
    if (ambient_running()) return false;

    // Whatever else was playing gives way, the way it does for a scramble.
    // Nothing is put back on the way in: the cube being watched is the cube
    // that was there, which is exactly what the snapshot below records.
    discard_playback();

    // Nothing of a watched pattern is written down: it is an interlude, and
    // the cube it borrowed is put back at the end of it. Told to advance, its
    // very first turn would carry the cursor past the end of the record.
    state->playback = Player{ambient_pattern(choice, size_of_cube()),
                             0,
                             kAmbientTempoMs,
                             cube::TimelineEffect::None,
                             true,
                             false,
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
    return state ? static_cast<std::uint32_t>(applied_user_moves()) : 0;
}

bool undo() noexcept
{
    if (!can_rewind()) return false;

    const auto& timeline = state->timeline;

    // The lower bound is the policy: what is below the end of the scramble is
    // not the user's to take back, so a cube that has only been scrambled and
    // one a solve has rewound into the scramble both refuse here without a
    // case of their own.
    if (timeline.cursor() <= timeline.scramble_end()) return false;

    return play_rewind(cube::rewind_plan(timeline, timeline.cursor() - 1),
                       cube::TimelineEffect::Rewind);
}

bool redo() noexcept
{
    if (!can_rewind()) return false;

    const auto& timeline = state->timeline;
    if (timeline.cursor() >= timeline.size()) return false;

    // Forward through the record, which is the same thing a scramble arriving
    // does -- so it carries the same effect.
    return play_rewind(cube::redo_plan(timeline, timeline.cursor() + 1),
                       cube::TimelineEffect::Advance);
}

bool solve_rewind() noexcept
{
    if (!can_rewind()) return false;

    // Undo with a different target and nothing else: the plan is longer, and
    // the length of it is the whole difference between the two commands.
    return play_rewind(cube::rewind_plan(state->timeline, 0),
                       cube::TimelineEffect::Rewind);
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

std::uint32_t timeline_length() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.size()) : 0;
}

std::uint32_t timeline_cursor() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.cursor()) : 0;
}

std::uint32_t timeline_scramble_end() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.scramble_end())
                 : 0;
}

std::uint32_t timeline_move(std::uint32_t index) noexcept
{
    if (!state || index >= state->timeline.size()) return 0;

    // The record hands over the move as it was played; normalizing the turns
    // happens in the packing, which is on nobody's replay path.
    return cube::pack(state->timeline.at(index));
}

bool turn_face(cube::Face face, int first_depth, int last_depth,
               int face_turns) noexcept
{
    if (!state) return false;
    if (!valid_face(face)) return false;
    if (face_turns != -1 && face_turns != 1 && face_turns != 2) return false;

    // The controller does not know about playback, and between a sequence
    // being accepted and its first frame it is idle -- so without this the
    // very next keypress would land in the middle of the sequence.
    if (busy()) return false;

    // Every refusal a range can earn is in here: off the cube, inside out, or
    // all of it at once.
    auto move = depth_move(face, first_depth, last_depth, size_of_cube());
    if (!move) return false;

    move->quarter_turns *= face_turns;
    return state->interaction.start_move(*move);
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

bool set_palette(graphics::Palette palette) noexcept
{
    if (!state || !valid_palette(palette)) return false;

    // No cancel and no relayout: the palette is read where a sticker becomes
    // pixels and nowhere else, so a drag keeps its grip and a snap keeps its
    // angle. The change shows on the next frame that is drawn anyway.
    state->palette = palette;
    return true;
}

graphics::Palette palette() noexcept
{
    return state ? state->palette : graphics::Palette::Classic;
}

bool set_canvas_theme(graphics::CanvasTheme theme) noexcept
{
    if (!state || !valid_canvas_theme(theme)) return false;

    // Both, in one place: the field is what a query reads and the renderer
    // is what a frame reads, and a refused value has already left above, so
    // neither can be written without the other.
    state->canvas_theme = theme;
    state->renderer->set_background(graphics::canvas_background(theme));
    return true;
}

graphics::CanvasTheme canvas_theme() noexcept
{
    return state ? state->canvas_theme : graphics::CanvasTheme::Dark;
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
    return state ? state->speed_scale : 1.0f;
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
