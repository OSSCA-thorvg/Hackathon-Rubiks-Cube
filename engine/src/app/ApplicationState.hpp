#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "app/Application.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeDraft.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"
#include "cube/solver/Solver.hpp"
#include "graphics/3d/Light.hpp"
#include "graphics/3d/OrbitCamera.hpp"
#include "graphics/frame/FrameInputs.hpp"
#include "graphics/Layout.hpp"
#include "interaction/InteractionController.hpp"
#include "interaction/NetPicking.hpp"
#include "render/Renderer.hpp"

/**
 * The application's state, and what its source files share about it.
 *
 * The application is one set of commands over one state, written down in
 * more than one file. Every one of those files reaches the same state, so it
 * is declared here once.
 *
 * Only the files in engine/src/app include this. None of it is part of the
 * contract in Application.hpp, which is why it sits in a namespace of its
 * own: a name in `detail` is a convenience between those files, and the
 * bindings and the tests have no business with it. Each of the files opens
 * the namespace with a using-directive, so a command reads the same whichever
 * file it is written in.
 */
namespace rubiks::app::detail {

/** The size a lifecycle opens with, and the one every phase before this had. */
inline constexpr int kDefaultCubeSize = 3;

/**
 * The settings a lifecycle opens with, each said once.
 *
 * Every one of them is read in two places: where the state is first given its
 * value, and where the getter answers when there is nothing to ask -- no
 * engine, or for the brush no draft. The second is what a caller outside a
 * lifecycle is told, and one who asks for the brush with no draft open. The
 * two have to be one value, so that what such a caller is told is what a
 * lifecycle, or a draft, then opens with.
 */
inline constexpr float kDefaultSpeedScale = 1.0f;
inline constexpr graphics::ViewMode kDefaultViewMode = graphics::ViewMode::Both;
inline constexpr graphics::FlatStyle kDefaultFlatStyle =
    graphics::FlatStyle::Net;
inline constexpr graphics::Palette kDefaultPalette = graphics::Palette::Classic;
inline constexpr cube::FaceColor kDefaultBrush = cube::FaceColor::White;

/** How fast a user's own release settles, in milliseconds per quarter turn. */
inline constexpr double kUserTempoMs = interaction::kSnapMsPerQuarterTurn;

/**
 * The same for a scramble, which is watched rather than made.
 *
 * Brisker than a user's own turn, and no brisker than the time a piece takes
 * to lift and settle again: a move shorter than that never lets the lift form,
 * and the drawing would flatten into a slideshow of states. Twenty moves land
 * inside three seconds at this pace.
 */
inline constexpr double kScrambleTempoMs = interaction::kOpeningMs;

/**
 * The same for a pattern being watched, which is not waited on at all.
 *
 * Slower than either: nobody is waiting for it to arrive anywhere, and the
 * point of it is to be looked at. At this pace the shortest of the patterns
 * comes back round to where it started in a little over a minute.
 */
inline constexpr double kAmbientTempoMs = 300.0;

/**
 * The same for a rewind, which is watched rather than made, like a scramble.
 *
 * A constant of its own despite the matching value: a solve can be dozens of
 * moves and is the one sequence a user waits through, so it is the first thing
 * that would be tuned, and tuning it should not quietly change how a scramble
 * arrives.
 */
inline constexpr double kRewindTempoMs = kScrambleTempoMs;

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
    std::optional<cube::Cube> restore_to;
};

/**
 * One canvas's worth of drawing: a target, its size, and what it shows.
 *
 * Which scenes a surface holds is its own; which of those are drawn is the
 * view mode's, the same for every surface. Where they sit on it is worked
 * out from the two by the one layout every canvas has always used, so a
 * surface holding one scene shows that scene alone, filling it.
 */
struct Surface {
    // Absent while the surface has no size: a surface put away holds nothing.
    std::unique_ptr<render::Renderer> renderer;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t scenes = 0;
    graphics::CanvasLayout placement;

    // What the target holds now, or nothing when it has to be drawn whatever
    // happens -- a new size, a new renderer, a new ground, a failed frame.
    // Those are the renderer's own, which no frame's inputs describe.
    std::optional<graphics::FrameInputs> drawn;

    // How many frames have been drawn into it, for a host copying it out.
    std::uint32_t frame = 0;
};

/**
 * Everything one running application owns.
 *
 * One object rather than a list of globals so that its lifetime is the
 * initialization state: initialize() creates it, shutdown() destroys it, and
 * there is no reset list that a new field could be left off of.
 */
struct ApplicationState {
    // Where frames are drawn. Surface 0 is the one initialize() makes, and it
    // holds every scene until it is told otherwise -- which is all a host with
    // one canvas ever needs. A host with more gives each of the others a size
    // and the scenes it shows.
    std::array<Surface, kSurfaceCount> surfaces;

    // The surface the gesture in hand began on. Its moves arrive in that
    // surface's pixels, and resizing any other surface leaves it alone.
    std::uint32_t gesture_surface = 0;

    // The cube as it was last drawn, and a count that moves when it changes.
    // The one input a frame is not handed a copy of, being the one too large
    // to copy for every canvas on every frame. Compared with this copy rather
    // than tracked at each place the cube changes: there are a dozen of
    // those, and a frame skipped because one was missed would be a picture of
    // a cube that is not the cube.
    cube::Cube drawn_cube{kDefaultCubeSize};
    std::uint64_t cube_revision = 0;

    // The whole of the viewpoint state. The Camera is derived from this and
    // the layout whenever one is needed rather than stored, so there is no
    // second copy to keep in step and no way to forget to rebuild it.
    graphics::OrbitCamera orbit = graphics::home_orbit();

    // How much faster than the written tempos everything turns. One value for
    // every animation, because a person setting a speed means the speed of the
    // cube and not of one of the ways it can be moved.
    float speed_scale = kDefaultSpeedScale;

    // The logical cube, only ever holding quarter turns. A drag in progress
    // lives in the controller instead, and reaches this through a committed
    // move.
    //
    // How many layers the cube has is read off this and stored nowhere else.
    // The controller holds a size of its own because it is the rule it
    // refuses moves by and it has no cube to ask -- so a change of size
    // replaces it rather than telling it, which is what makes "a gesture from
    // the cube before" a state that cannot be written down.
    cube::Cube cube_state{kDefaultCubeSize};
    interaction::InteractionController interaction{kDefaultCubeSize};
    graphics::ViewMode view_mode = kDefaultViewMode;
    graphics::FlatStyle flat_style = kDefaultFlatStyle;

    // Which six shades the stickers are drawn in. Nothing but drawing reads
    // it, so it is the one setting a turn in progress can be changed under.
    graphics::Palette palette = kDefaultPalette;

    // The lights the 3D view is drawn under, in world space. The studio setup
    // by default; set_lighting() replaces it so the lamps can be tuned by eye
    // from the page, and the values that tuning settles on go back into
    // Lighting::standard().
    graphics::Lighting lighting = graphics::default_lighting();

    // Where a lighting setup is written before set_lighting() reads it; the
    // same arrangement as `restore`.
    std::vector<float> lighting_values;

    // The ground behind the cube. Kept here as well as on each renderer,
    // because a surface given a size after the theme was chosen gets a new
    // renderer that has to be told it. The two cannot drift: every write goes
    // through set_canvas_theme(), which sets both.
    //
    // Dark rather than Light to begin with, because that is the ground the
    // renderer has always cleared to and the one the native build, which has
    // no page around it to ask, still wants.
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
        /** Takes an independent editable copy of the logical cube. */
        explicit PaintDraft(const cube::Cube& cube) : model(cube) {}

        cube::CubeDraft model;
        cube::FaceColor brush = kDefaultBrush;

        // Whether a press covers the face it lands on rather than the one
        // square. Held with the draft because it means nothing without one.
        bool filling = false;

        // Whether a press is still down. A drag across the net colours every
        // cell it crosses, which is how a face gets copied off a real cube
        // without lifting the finger between squares.
        bool stroking = false;

        // Where the keyboard is on the net, once it has been there, and
        // whether that place is drawn -- only while the net has the focus.
        // Kept for the life of the draft, so a trip to the colours and back
        // finds it where it was left.
        std::optional<interaction::NetPick> cursor;
        bool cursor_shown = false;
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

extern std::unique_ptr<ApplicationState> state;

// Defined in Application.cpp, where each is described.
[[nodiscard]] int size_of_cube() noexcept;
[[nodiscard]] bool valid_cube_size(int size) noexcept;
void relayout_all() noexcept;
void drain_orbit() noexcept;
void install_cube(cube::Cube next,
                  std::vector<cube::FaceColor> origin = {}) noexcept;
[[nodiscard]] bool cube_held() noexcept;

// Defined in History.cpp, where each is described.
[[nodiscard]] bool play_rewind(std::vector<cube::CubeMove> plan,
                               cube::TimelineEffect effect) noexcept;
[[nodiscard]] bool can_rewind() noexcept;

}  // namespace rubiks::app::detail
