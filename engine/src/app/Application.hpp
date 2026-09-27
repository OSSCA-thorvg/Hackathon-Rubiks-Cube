#pragma once

#include <cstdint>
#include <vector>

#include "cube/Assembly.hpp"
#include "cube/CubeMove.hpp"
#include "cube/PackedMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/CanvasTheme.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"

namespace rubiks::app {

/**
 * How many surfaces the engine draws into.
 *
 * A surface is one canvas's worth of drawing: a target of its own size and
 * the scenes it shows. Surface 0 is made by initialize(), and every call
 * below that names no surface -- resize(), pixel_buffer(), pointer_down() --
 * is about it. A host with one canvas never needs another. A host that lays
 * its views out itself gives each of the others a size and a scene, and puts
 * the canvases wherever it likes: where the views sit and how big they are is
 * the host's, and what is in each of them is the engine's.
 */
inline constexpr std::uint32_t kSurfaceCount = 4;

/**
 * The scenes a surface can show, as bits of one set.
 *
 * A surface draws the scenes it holds that the view mode shows, laid out by
 * the one layout every canvas has always used -- so a surface holding one
 * scene shows that scene alone, filling it, and a surface holding them all is
 * the one canvas this application began with. The axes sit in the cube's
 * corner when the two share a surface, and fill a surface of their own.
 */
inline constexpr std::uint32_t kSceneCube = 1u << 0;
inline constexpr std::uint32_t kSceneNet = 1u << 1;
inline constexpr std::uint32_t kSceneRings = 1u << 2;
inline constexpr std::uint32_t kSceneAxes = 1u << 3;
inline constexpr std::uint32_t kAllScenes =
    kSceneCube | kSceneNet | kSceneRings | kSceneAxes;

/**
 * Initializes the ThorVG runtime and the software renderer.
 *
 * Dimensions are validated first and invalid ones fail even when already
 * initialized. Repeated calls with valid dimensions succeed without
 * reconfiguring the renderer; the size is then changed through resize().
 *
 * @return true when the runtime and renderer are ready for rendering.
 */
[[nodiscard]] bool initialize(std::uint32_t width,
                              std::uint32_t height) noexcept;

/**
 * Resizes surface 0 following the resize failure semantics of the Phase 1
 * contract. A size, never nothing: see resize_surface() for putting one away.
 *
 * @return true when the target uses the new size afterwards.
 */
[[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height) noexcept;

/**
 * Resizes surface `id`, or puts it away with zero by zero.
 *
 * A surface put away holds no target and draws nothing until it is given a
 * size again. A drag begun on it is dropped, as a resize has always dropped
 * one; a drag on any other surface carries on.
 *
 * @return false for an unknown surface or a size no target can have.
 */
[[nodiscard]] bool resize_surface(std::uint32_t id, std::uint32_t width,
                                  std::uint32_t height) noexcept;

/**
 * Says which scenes surface `id` shows, as a set of the kScene bits.
 *
 * @return false for an unknown surface or bits that name no scene.
 */
[[nodiscard]] bool set_surface_scenes(std::uint32_t id,
                                      std::uint32_t scenes) noexcept;

/** The scenes surface `id` holds; zero for an unknown surface. */
[[nodiscard]] std::uint32_t surface_scenes(std::uint32_t id) noexcept;

/**
 * Draws every surface whose picture has changed since its last frame.
 *
 * A surface is drawn again only when something it shows is different: the
 * net keeps its frame while the viewpoint sweeps, and the axes keep theirs
 * while a layer turns. surface_frame() counts the frames actually drawn.
 *
 * @return true when every surface that had to be drawn was.
 */
[[nodiscard]] bool render() noexcept;

/**
 * Makes the next render() draw every surface, changed or not.
 *
 * For measuring a frame, which a skipped one would not be.
 */
void invalidate() noexcept;

/**
 * Returns the address of the software pixel buffer.
 *
 * @return zero without a valid software buffer.
 */
[[nodiscard]] std::uintptr_t pixel_buffer() noexcept;

/**
 * Returns the byte length of the software pixel buffer.
 *
 * @return zero without a valid software buffer.
 */
[[nodiscard]] std::uint32_t pixel_byte_length() noexcept;

/** The same two, for surface `id`; zero for one with no target. */
[[nodiscard]] std::uintptr_t surface_pixel_buffer(std::uint32_t id) noexcept;
[[nodiscard]] std::uint32_t surface_pixel_byte_length(
    std::uint32_t id) noexcept;

/**
 * How many frames surface `id` has had drawn into it.
 *
 * A host copies a surface out when this has moved since it last did, and
 * leaves it alone when it has not.
 */
[[nodiscard]] std::uint32_t surface_frame(std::uint32_t id) noexcept;

/**
 * Begins a pointer gesture at a point in drawing-buffer pixels.
 *
 * Pressing a cell of either view drags a layer; pressing anywhere else sweeps
 * the viewpoint around the cube. Which view a press belongs to comes from the
 * layout, so the two never contend for a pixel. Converting CSS pixels and the
 * device pixel ratio is the caller's job, the same split the resize path uses.
 *
 * A snap left running by the previous release is confirmed here rather than
 * blocking the press, so drags in quick succession all reach the cube.
 *
 * While a sequence is playing, a press may only look around the cube: it never
 * reaches a pick, and it is refused outright when the 3D cube is not on screen,
 * for the same reason a press off the net is refused in the flat-only view.
 *
 * A watched pattern is a sequence like any other here, so it too is looked
 * around rather than interrupted; watching ends at a command, and a drag is
 * not one.
 *
 * @return true when a gesture began. False only when none could: before
 *         initialization, for non-finite coordinates, while another gesture is
 *         already running, and for a press with nothing on screen it could
 *         start -- off the net in the flat-only view, or anywhere in that view
 *         while a sequence plays.
 */
[[nodiscard]] bool pointer_down(float x, float y) noexcept;

/**
 * The same press, on surface `id`, at a point in that surface's pixels.
 *
 * What the press can start is what that surface shows: a cell of its net, a
 * sticker of its rings, or a layer or the viewpoint on its cube. The moves
 * that follow arrive in the same surface's pixels. A press outside a surface's
 * cube but on a surface that shows one sweeps the viewpoint, as a press on the
 * background of one canvas always has.
 */
[[nodiscard]] bool pointer_down_on(std::uint32_t id, float x, float y) noexcept;

/** Continues the active gesture. Ignored without one. */
void pointer_move(float x, float y) noexcept;

/** Releases the active gesture, the only path that can turn a layer. */
void pointer_up() noexcept;

/**
 * Abandons the active gesture without turning the cube.
 *
 * For a cancelled pointer or lost capture: the user never let go, so nothing
 * should be committed. A viewpoint sweep has nothing to withhold, so for it
 * this is the same as a release.
 */
void pointer_cancel() noexcept;

/**
 * Advances animation by an elapsed time in milliseconds and applies a turn
 * that has finished.
 *
 * @return true while further frames still have to be drawn.
 */
[[nodiscard]] bool advance(double elapsed_ms) noexcept;

/**
 * The sizes of cube this application will build.
 *
 * Two is the smallest cube there is. Twenty-eight is the largest one that can
 * be written down: a move carries its layers as a bit per layer in the field
 * `cube::kPackedMaxLayers` describes, so a wider cube has moves the record,
 * the move log and a shared link cannot hold. The ceiling is that limit rather
 * than a number somebody liked.
 *
 * Nine used to stand here on the grounds that the drawing stops being worth
 * looking at past it. Measured, that turned out not to be the binding cost:
 * a split frame at twenty-eight draws 4704 stickers three times over in about
 * six milliseconds. What does grow steeply is solving -- see Reduction -- and
 * that is answered where the solve is asked for, not by refusing to build the
 * cube.
 *
 * Both ends are read in one place, so moving either is one number.
 */
inline constexpr int kMinCubeSize = 2;
inline constexpr int kMaxCubeSize = cube::kPackedMaxLayers;

/**
 * Builds a cube of a different size, and starts the session over.
 *
 * Everything the cube is made of goes: the record, whatever was playing, and
 * any turn in flight. There is no carrying a record across, because a move of
 * the old cube may name a layer the new one does not have.
 *
 * What does not go is how the cube is being looked at -- the viewpoint, the
 * views on screen, the flat drawing, the shades, the speed. The size is a
 * property of the cube; those are the user's, at this moment as at every
 * other.
 *
 * The size already in use is accepted and changes nothing: asking for what is
 * already there is a confirmation rather than a command, and a session thrown
 * away by one would be a surprise.
 *
 * @return true when the size is one this application builds; false for
 *         anything outside kMinCubeSize..kMaxCubeSize, which leaves the cube
 *         exactly as it was.
 */
[[nodiscard]] bool set_cube_size(int size) noexcept;

/** How many layers the cube has along an axis; 0 outside a lifecycle. */
[[nodiscard]] int cube_size() noexcept;

/** The largest scramble that can be asked for. */
inline constexpr std::uint32_t kMaxScrambleMoves = 100;

/**
 * Restarts the cube and plays the deterministic scramble for `seed` into it.
 *
 * The moves are turned rather than applied, so the cube is still solved when
 * this returns and reaches the scrambled state some frames later. Until then
 * the application is busy and the played moves are nobody's.
 *
 * @return true when the sequence was accepted. False for a count outside
 *         1..kMaxScrambleMoves, which leaves the cube exactly as it was.
 */
[[nodiscard]] bool scramble(std::uint32_t seed,
                            std::uint32_t move_count) noexcept;

/** Restores the logical cube and clears the current solve session. */
void reset_cube() noexcept;

/** How many repeating patterns the watching mode chooses between. */
inline constexpr std::uint32_t kAmbientPatternCount = 4;

/**
 * How many times a written pattern may repeat before the cube is back where
 * it began.
 *
 * A bound rather than the exact orders, which are worked out in the phase
 * document and live nowhere else. Nothing in the engine reads an order -- the
 * repeat just keeps taking the next move -- so writing them down here would be
 * a set of numbers that only the tests guarding them ever looked at. What the
 * watching of those cubes promises is that it comes back round within a
 * while, and that is a bound; it goes on holding when the table is edited,
 * which is when the promise is easiest to break.
 *
 * It is about the written patterns, which is to say about cubes with no
 * layers inside. A bigger cube is walked over instead, and a walk that mixes
 * the whole of it does not come back round in any number of rounds anyone
 * would sit through -- staying near the solved cube and leaving most of the
 * cube alone are the same thing, and the second of those is what made a big
 * cube not worth watching. Nothing rests on the return: what puts the cube
 * back is the snapshot the watching took.
 */
inline constexpr int kAmbientMaxPeriod = 200;

/**
 * The pattern one choice selects, taken modulo the size of the table.
 *
 * Every value is a valid choice, so there is no rejection path and no size to
 * ask for before choosing. The table is fixed and public because the property
 * that matters about it -- that each of its patterns brings the cube back
 * round within kAmbientMaxPeriod -- is arithmetic, and checking arithmetic
 * needs the numbers rather than an application to play them into.
 *
 * The patterns are written as a face and a share of the cube, and built for
 * the cube in hand, so the same four are watchable at every size. A share
 * rather than a depth is what lets one table suit them all: on a big cube the
 * moves reach half way in, which is the only way the layers under the surface
 * are ever seen to move, and on a 2x2 or a 3x3 that half is one layer and the
 * patterns are the face turns they have always been.
 *
 * A cube with layers inside is walked over rather than played a pattern into,
 * because four moves of a 9x9 leave most of it standing still: the walk steps
 * round the six faces, counts its depth up through the halves, and alternates
 * a slice with a half, so every band of the cube is turned on the way past.
 * What that costs is the return above.
 *
 * Empty for a size this application does not build.
 */
[[nodiscard]] std::vector<cube::CubeMove> ambient_pattern(std::uint32_t choice,
                                                          int size);

/**
 * Begins watching: the cube is put away and a pattern repeats until stopped.
 *
 * The pattern is played through the same path a scramble takes, so the moves
 * are turned rather than applied and none of them is anybody's. It never runs
 * out, so the application stays busy and asking for frames until it is stopped.
 *
 * @return true when watching began; false before initialization, when it had
 *         already begun, and while a draft is open.
 */
[[nodiscard]] bool ambient_start(std::uint32_t choice) noexcept;

/**
 * Ends watching and puts back the cube from the moment it began.
 *
 * Without animation: this is not a move but the undoing of an interlude, and
 * there is nothing about it to watch. The move count and the viewpoint are
 * untouched, so nothing that happened while watching is left behind.
 *
 * A no-op when nothing is being watched.
 */
void ambient_stop() noexcept;

/** Returns whether a pattern is being watched right now. */
[[nodiscard]] bool is_ambient() noexcept;

/** Returns whether the committed logical cube is solved. */
[[nodiscard]] bool is_solved() noexcept;

/**
 * Returns the number of user moves committed since the latest scramble.
 *
 * Derived from the record rather than counted: it is the part of the timeline
 * past the scramble that is currently on the cube. A stored counter would need
 * a correction of its own at every rewind, every discarded redo tail and every
 * solve, and a derived one is right by construction at all of them.
 */
[[nodiscard]] std::uint32_t committed_move_count() noexcept;

/**
 * Takes back the user's last move, by turning it back rather than undoing it.
 *
 * The move is played as a sequence of one, so it settles on screen the way
 * every other turn does and the cursor follows the commit.
 *
 * Refused while anything else owns the cube or a draft is open, and refused
 * once the cursor has reached the end of the scramble: what is below that is
 * not the user's to take back. Use stop_playback() to break off a sequence rather than waiting
 * for one to end.
 *
 * @return true when a rewind began.
 */
[[nodiscard]] bool undo() noexcept;

/**
 * Plays back the move a rewind took off, exactly as it was made.
 *
 * @return true when a replay began; false when nothing has been rewound,
 *         something else owns the cube, or a draft is open.
 */
[[nodiscard]] bool redo() noexcept;

/**
 * Rewinds every applied move, leaving a solved cube.
 *
 * The same function undo uses with a different target, so a solve is a longer
 * plan and nothing else. It can be broken off part way; where it stops, the
 * record and the cube agree.
 *
 * @return true when a rewind began; false with nothing applied, or while
 *         something else owns the cube or a draft is open.
 */
[[nodiscard]] bool solve_rewind() noexcept;

/**
 * Whether the solver this application holds solves the cube in hand.
 *
 * The size and nothing else. Whether a solve may be started *now* is the same
 * question every other command answers with is_busy(), and the screen reads
 * both to decide whether the control is live -- so this one does not have to
 * know about gestures or sequences to be the honest answer to "is there a
 * solver for this cube".
 */
[[nodiscard]] bool can_solve() noexcept;

/**
 * Works out how to solve the cube in front of it, and plays that.
 *
 * Not the rewind above. A rewind goes backwards through the record and ends at
 * the cube the session began with, whoever made the moves it is taking off; a
 * solve reads the cube as it stands and goes forwards. On a cube brought in
 * from a shared link the difference is the whole point: rewinding replays
 * somebody else's session in reverse, and this finishes it.
 *
 * The moves are written into the record before they are played, so they count
 * as the user's own from that moment -- who chose them is not something a
 * record of what happened to a cube can hold. Whether a solve was somebody's
 * own work is a fact about the sitting, and the screen keeps it.
 *
 * Broken off with stop_playback(), like a rewind. What is left of the solution
 * stays above the cursor, so redo() takes it up again a move at a time.
 *
 * @return true when a solve began; false before initialization, while
 *         something else owns the cube or a draft is open, for a size no
 *         solver here handles, and for a cube that is already solved.
 */
[[nodiscard]] bool solve() noexcept;

/**
 * Breaks off a rewind, keeping everything it has already turned.
 *
 * The turn in flight is confirmed rather than dropped -- winding a rotation
 * back mid-way looks worse than letting it land, and confirming it goes
 * through the same commit as every other move, so the record follows it.
 *
 * A no-op for anything else being played: a scramble broken off half way is a
 * cube nobody asked for, and a watched pattern stopped this way would never
 * put back the cube it borrowed.
 */
void stop_playback() noexcept;

/** How many moves the record holds, scramble and user moves together. */
[[nodiscard]] std::uint32_t timeline_length() noexcept;

/** How many of those moves are on the cube right now. */
[[nodiscard]] std::uint32_t timeline_cursor() noexcept;

/**
 * Where the scramble stops and the user's own moves begin.
 *
 * With the two above, everything the screen and a shared link need: the
 * scramble is `[0, scramble_end)`, the user's moves are `[scramble_end,
 * length)`, and a cursor of nothing on a scramble that exists is a cube a
 * rewind solved rather than a person.
 */
[[nodiscard]] std::uint32_t timeline_scramble_end() noexcept;

/**
 * The recorded move at an index, packed as cube::pack writes it.
 *
 * The record is one list, so reading it is one query: the scramble and the
 * user's own moves come back through the same index and are told apart by
 * timeline_scramble_end() rather than by asking twice.
 *
 * @return zero for an index the record does not hold, which is the same value
 *         a packed move can never take.
 */
[[nodiscard]] std::uint32_t timeline_move(std::uint32_t index) noexcept;

/**
 * The largest record a shared state may bring in, both stretches together.
 *
 * Far past anything a session reaches -- a scramble is capped at a hundred and
 * a solve by hand is a few dozen more -- and chosen instead by what a link can
 * still be: at this bound the encoded fragment is about twenty thousand
 * characters -- twenty-eight with the colours of the largest painted cube --
 * which a browser carries and a person can still paste.
 */
inline constexpr std::uint32_t kMaxRestoreMoves = 4096;

/**
 * Takes the buffer a shared record is written into, and its address.
 *
 * The same arrangement the pixels use: the engine owns the memory and the
 * caller writes into it through a view, so a record of any length crosses in
 * one call rather than in one call per move. The words are packed exactly as
 * timeline_move() hands them back, the scramble first and the user's own moves
 * after it, and nothing is checked as it is written -- restore_apply() looks
 * at all of it at once.
 *
 * Nothing else may be called on the engine between this and restore_apply():
 * the address is a pointer into a vector, and any other call is free to grow
 * the heap out from under the view.
 *
 * @return zero for a count of nothing, for one past kMaxRestoreMoves, and
 *         before initialization.
 */
[[nodiscard]] std::uintptr_t restore_buffer(std::uint32_t total_count) noexcept;

/**
 * Reads the buffer back as a session and puts it on the cube, all at once.
 *
 * Not a transaction, because there is nothing to protect: this is called at
 * startup, so the state it would be rolling back to is a cube that has just
 * been made. Every word is checked before any of it is applied, so a refusal
 * leaves the cube exactly as it was and the caller starts a fresh session
 * rather than retrying.
 *
 * The size comes in here rather than through set_cube_size() beforehand, so
 * that a record and the cube it belongs to arrive together: a size set first
 * and a record refused after it would leave a cube nobody asked for.
 *
 * What is checked is what would otherwise be read out of range or turned into
 * a move that cannot exist: the size, the two counts summing to what
 * restore_buffer was told, and each word's axis, turns and layer mask. The
 * mask is held to one unbroken run of layers short of the whole cube -- the
 * same set of moves this application can make and can write down -- which is
 * what keeps a hand-written link from bringing in a move with no notation.
 *
 * The record is built through the ordinary timeline operations, so the cursor
 * lands at the end of it and the user's move count follows from the record the
 * way it always does. The cube arrives at once, without animation.
 *
 * @return false without a buffer to read, for a size this application does not
 *         build, for counts that do not match the buffer, and for any word the
 *         payload cannot carry.
 */
[[nodiscard]] bool restore_apply(int size, std::uint32_t scramble_count,
                                 std::uint32_t user_count) noexcept;

/**
 * Colouring a real cube onto the net, and what it takes to be believed.
 *
 * The one way into this application that does not begin from a solved cube. A
 * person holds the cube they actually have, colours what they see, and asks
 * for it to be solved -- which means the colours have to be checked before
 * they become a cube, because most colourings are not cubes and a solver
 * handed one would never finish. `cube/Assembly.hpp` is that check; this is
 * the state a person edits on their way to it.
 *
 * A draft rather than the cube itself. Turning is refused while one is open
 * and the cube underneath is untouched until it is applied, so backing out
 * costs nothing and leaves nothing behind. The draft starts as a copy of what
 * is on the cube, because a person mending a scrambled cube onto the net has
 * far less to change than one starting from an empty grid.
 */

/**
 * Opens a draft of the cube as it stands.
 *
 * @return false while a sequence is playing, since a draft taken of a cube
 *         mid-turn would be a copy of a moment nobody chose, and false if one
 *         is already open -- reopening would throw away what has been coloured
 *         so far, which is not what asking twice means.
 */
[[nodiscard]] bool begin_painting() noexcept;

/** Throws the draft away. The cube was never touched, so nothing else changes. */
void cancel_painting() noexcept;

/** Whether a draft is open, which is the whole of "is being painted". */
[[nodiscard]] bool is_painting() noexcept;

/** The colour a press lays down. */
[[nodiscard]] bool set_brush(cube::FaceColor colour) noexcept;
[[nodiscard]] cube::FaceColor brush() noexcept;

/**
 * Lays the brush on the cell under a point, in drawing-buffer pixels.
 *
 * @return false without a draft, and for a point outside the net -- the four
 *         empty corners of the cross included.
 */
[[nodiscard]] bool paint_at(float x, float y) noexcept;

/**
 * Whether a press lays the brush on one cell or on the whole face.
 *
 * A mode rather than a second gesture, because a button on the page has no
 * point on the net to act at -- and "press Fill, then press the face" is one
 * thing to learn where a modifier key or a double press would be two.
 */
[[nodiscard]] bool set_filling(bool whole_face) noexcept;
[[nodiscard]] bool is_filling() noexcept;

/**
 * Lays the brush on every cell of the face under a point.
 *
 * A four by four is ninety-six cells and a twenty-eight is four thousand seven
 * hundred; most faces of a real cube are mostly one colour, so filling and
 * then mending is the difference between this being usable at those sizes and
 * not.
 */
[[nodiscard]] bool fill_face_at(float x, float y) noexcept;

/** How many of a colour the draft carries, against the N^2 it needs. */
[[nodiscard]] int painted_count(cube::FaceColor colour) noexcept;

/**
 * Makes the draft the cube, if it is one.
 *
 * What arrives is the new starting position: the record is cleared, exactly as
 * a change of size clears it, so solving, rewinding, recording and replay
 * carry on afterwards without knowing where the cube came from.
 *
 * A refusal leaves the draft open and unchanged, with `painting_fault()` and
 * the blamed stickers saying what to mend. Nothing is half applied.
 *
 * @return false without a draft, and for a colouring no turning could reach.
 */
[[nodiscard]] bool apply_painting() noexcept;

/** What the last refusal was, or `None` when nothing has been refused yet. */
[[nodiscard]] cube::PaintFault painting_fault() noexcept;

/**
 * The stickers the last refusal blames, numbered as `surface_stickers()`
 * counts them.
 *
 * Read one at a time rather than handed over as a block: the count is small,
 * the boundary this crosses carries primitives only, and a fault that blames
 * nothing at all is an ordinary answer rather than a missing one.
 */
[[nodiscard]] int painting_blamed_count() noexcept;
[[nodiscard]] int painting_blamed_at(int index) noexcept;

/** The draft's colours, in `surface_stickers()` order. Empty without a draft. */
[[nodiscard]] const std::vector<cube::FaceColor>& painting_draft() noexcept;

/** Every sticker on the cube as it stands, in `surface_stickers()` order. */
[[nodiscard]] std::vector<cube::FaceColor> cube_painting() noexcept;

/**
 * The colouring this session began from, when it began from one.
 *
 * A cube that was painted cannot be written down as moves: no sequence from
 * solved arrives at it without solving it first. So a link that is to carry
 * one has to carry the colours, and this is what it carries -- read one at a
 * time, because the boundary takes primitives and a link is being built rather
 * than a frame drawn.
 *
 * Empty for a session that began from a scramble or a reset, which is the
 * question "does this link need the colours" answered without a flag beside
 * it.
 */
[[nodiscard]] std::uint32_t origin_painting_count() noexcept;
[[nodiscard]] int origin_painting_at(std::uint32_t index) noexcept;

/**
 * Where a colouring arriving from a link is written before it is read.
 *
 * The same two-call shape `restore_buffer` uses, and for the same reason: one
 * colour per sticker is thousands of values at the sizes this builds, and
 * handing them over one call at a time would be thousands of calls.
 *
 * @return zero for a count no cube has, and zero without an engine.
 */
[[nodiscard]] std::uintptr_t painting_buffer(std::uint32_t count) noexcept;

/**
 * Reads a painted session back onto the cube, all at once.
 *
 * Takes both buffers: the colours from `painting_buffer` and the user's moves
 * from `restore_buffer`, which is why a caller fills the two and then calls
 * this rather than restoring twice. Everything is checked before anything is
 * applied -- the size, the colour count, each colour, that the colouring is a
 * cube at all, and every move -- so a refusal leaves the cube as it was.
 *
 * The colouring becomes the starting position and the moves are recorded on
 * top of it, which is exactly what happened to the cube this link came from.
 *
 * @return false without both buffers, for a size this application does not
 *         build, for a colouring no turning reaches, and for any word the
 *         payload cannot carry.
 */
[[nodiscard]] bool restore_painting(int size, std::uint32_t user_count) noexcept;

/**
 * Starts one animated turn of the layers a face names.
 *
 * Depth 1 is the face itself and depths count inwards, so `(Right, 1, 1)` is
 * R, `(Right, 1, 2)` is Rw, and `(Right, 2, 2)` is the slice behind it. That
 * is the whole of how a wide move and a slice differ, which is why they are
 * one command with a range rather than a command each.
 *
 * `face_turns` is 1 clockwise, -1 counter-clockwise, or 2 for a half turn as
 * seen from outside `face`.
 *
 * A range covering every layer is refused. That is a whole-cube rotation: it
 * leaves the cube as solved or unsolved as it found it, it has no notation in
 * the letters this application writes, and refusing it here is what keeps
 * "every move that can be made can be written down" true.
 *
 * @return true when the command was accepted; false while anything owns the
 *         cube or a draft is open.
 */
[[nodiscard]] bool turn_face(cube::Face face, int first_depth, int last_depth,
                             int face_turns) noexcept;

/** Changes the rendered views while preserving cube, camera, and game state. */
[[nodiscard]] bool set_view_mode(graphics::ViewMode mode) noexcept;

/** Returns the current view mode, defaulting to Both outside a lifecycle. */
[[nodiscard]] graphics::ViewMode view_mode() noexcept;

/**
 * Chooses which drawing fills the flat region, wherever that region is.
 *
 * Independent of the view mode, so it can be changed while the flat view is
 * not on screen and be waiting when it comes back.
 */
[[nodiscard]] bool set_flat_style(graphics::FlatStyle style) noexcept;

/** Returns the current flat style, defaulting to Net outside a lifecycle. */
[[nodiscard]] graphics::FlatStyle flat_style() noexcept;

/**
 * Chooses which six shades the stickers are drawn in.
 *
 * Nothing but the next frame reads it -- not the cube, not the record, not the
 * clock, not a turn in progress -- so unlike the view commands this one
 * neither cancels a gesture nor relays out the surface, and is accepted while
 * the engine is busy.
 */
[[nodiscard]] bool set_palette(graphics::Palette palette) noexcept;

/** Returns the current palette, defaulting to Classic outside a lifecycle. */
[[nodiscard]] graphics::Palette palette() noexcept;

/**
 * Chooses the ground the cube is drawn against.
 *
 * Read where the target is cleared and nowhere else, so like the palette it
 * cancels no gesture, relays out nothing, and is accepted while the engine is
 * busy -- a person switching the page to Light in the middle of a watched
 * pattern gets a light page and the pattern keeps running.
 *
 * @return false outside a lifecycle and for a value that names no theme.
 */
[[nodiscard]] bool set_canvas_theme(graphics::CanvasTheme theme) noexcept;

/**
 * Returns the current canvas theme, defaulting to Dark outside a lifecycle.
 *
 * Dark rather than Light because that is the ground the renderer has always
 * cleared to and the one the native build, which has no page around it to ask,
 * still wants.
 */
[[nodiscard]] graphics::CanvasTheme canvas_theme() noexcept;

/**
 * Sets how much faster than the written tempos every animation runs.
 *
 * `duration = base / scale`, so 2 is twice as fast and 0.5 is half. One value
 * covers a user's own release, a scramble, a rewind and a watched pattern,
 * because a speed is a property of the cube rather than of who turned it.
 *
 * Out-of-range values are clamped and accepted; only a value that is not a
 * number at all is refused. A turn already running keeps the duration it
 * began with.
 *
 * @return true when the engine was there to take it.
 */
[[nodiscard]] bool set_speed_scale(float scale) noexcept;

/** Returns the current multiplier, defaulting to 1 outside a lifecycle. */
[[nodiscard]] float speed_scale() noexcept;

/**
 * Takes the buffer a lighting setup is written into, and its address.
 *
 * The same arrangement restore_buffer() uses: the engine owns the memory and
 * the caller writes into it through a view, then set_lighting() reads it. The
 * count has to satisfy graphics::Lighting::valid_count() and the order is the
 * one Lighting::from_values documents. Here so the lights can be tuned by eye
 * from the page without a rebuild; the values that come out of that tuning
 * are baked into Lighting::standard().
 *
 * Nothing else may be called on the engine between this and set_lighting().
 *
 * @return zero for a wrong count and before initialization.
 */
[[nodiscard]] std::uintptr_t lighting_buffer(std::uint32_t count) noexcept;

/**
 * Reads the buffer back as the lighting every following frame is drawn under.
 *
 * @return false for a wrong count or a value that is not a number, leaving
 *         the lighting as it was.
 */
[[nodiscard]] bool set_lighting(std::uint32_t count) noexcept;

/**
 * How many values lighting_values() writes for the current lighting: three
 * plus six per lamp. Zero before initialization.
 */
[[nodiscard]] std::uint32_t lighting_count() noexcept;

/**
 * Writes the current lighting into the buffer as the flat list set_lighting()
 * reads, and returns the buffer's address.
 *
 * The same buffer lighting_buffer() hands out, so a page that reads the
 * lights and writes them back works through one arrangement. Here so the page
 * can show the lights it is drawing under without keeping a copy of the
 * engine's defaults.
 *
 * @return zero when `count` is not lighting_count(), and before initialization.
 */
[[nodiscard]] std::uintptr_t lighting_values(std::uint32_t count) noexcept;

/** Restores the home camera without changing cube or view mode. */
void reset_view() noexcept;

/**
 * Returns whether a gesture, animation, unconsumed commit, or playback is on.
 *
 * An orbit sweep is not busy; the viewpoint can move while the cube cannot.
 * A sequence being played is, from the moment it is accepted until its last
 * turn has settled, so the value never falls away between two of its moves --
 * and a pattern being watched, which has no last turn, for as long as it runs.
 */
[[nodiscard]] bool is_busy() noexcept;

/**
 * Releases the renderer and the ThorVG runtime.
 *
 * Safe to call regardless of the initialization state.
 */
void shutdown() noexcept;

/**
 * Reports whether this application owns an active ThorVG initialization.
 *
 * @return true when initialize() has completed successfully.
 */
[[nodiscard]] bool is_initialized() noexcept;

}  // namespace rubiks::app
