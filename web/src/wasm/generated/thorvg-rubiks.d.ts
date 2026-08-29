/**
 * Hand-maintained declaration for the generated Emscripten ES module.
 *
 * The generated JavaScript and WASM artifacts are synchronized into this
 * directory by build_wasm.sh and are not tracked by Git. This declaration
 * is source: update it whenever the exported C ABI changes.
 */

/** Surface of the generated Emscripten module used by this project. */
export type ThorvgRubiksModule = {
  /**
   * Byte view over the WASM linear memory; replaced after memory growth.
   * The engine builds without threads, so the backing memory is a plain
   * ArrayBuffer and never a SharedArrayBuffer.
   */
  readonly HEAPU8: Uint8Array<ArrayBuffer>;
  _thorvg_rubiks_initialize(width: number, height: number): number;
  _thorvg_rubiks_resize(width: number, height: number): number;
  _thorvg_rubiks_render(): number;
  _thorvg_rubiks_pixel_buffer(): number;
  _thorvg_rubiks_pixel_byte_length(): number;
  _thorvg_rubiks_shutdown(): void;
  /** Coordinates are drawing buffer pixels; returns 1 when a gesture began. */
  _thorvg_rubiks_pointer_down(x: number, y: number): number;
  _thorvg_rubiks_pointer_move(x: number, y: number): void;
  _thorvg_rubiks_pointer_up(): void;
  _thorvg_rubiks_pointer_cancel(): void;
  /** Returns 1 while further frames still have to be drawn. */
  _thorvg_rubiks_advance(elapsedMs: number): number;
  /**
   * Returns 1 when the scramble for `seed` was accepted and began playing.
   *
   * The cube is still solved on return and reaches the scrambled state once
   * the sequence has played; the engine is busy until it does.
   */
  _thorvg_rubiks_scramble(seed: number, moveCount: number): number;
  /** Restores the solved cube, keeping the camera and the view mode. */
  _thorvg_rubiks_reset_cube(): void;
  /**
   * Takes the buffer a shared record is written into and returns its address.
   *
   * Words are packed as _thorvg_rubiks_timeline_move() hands them back, the
   * scramble first. Nothing else may be called before restore_apply: another
   * call may grow the heap and leave the caller's view pointing at nothing.
   *
   * Zero when the count is nothing or past the engine's bound.
   */
  _thorvg_rubiks_restore_buffer(totalCount: number): number;
  /**
   * Reads the whole buffer back onto the cube, without animation.
   *
   * The size arrives with the record, because a mask means nothing without the
   * cube it was taken from. Returns 1 when every word was accepted; 0 leaves
   * the cube untouched.
   */
  _thorvg_rubiks_restore_apply(
    size: number,
    scrambleCount: number,
    userCount: number,
  ): number;
  /**
   * Returns 1 when watching began; 0 when it had already begun.
   *
   * `choice` picks a pattern modulo the table, so every value is a valid one
   * and the arbitrariness comes from here rather than from the engine.
   */
  _thorvg_rubiks_ambient_start(choice: number): number;
  /** Ends watching, putting the cube from before it back without animating. */
  _thorvg_rubiks_ambient_stop(): void;
  /** Returns 1 while a pattern is being watched. */
  _thorvg_rubiks_is_ambient(): number;
  /** Returns 1 when the committed logical cube is solved. */
  _thorvg_rubiks_is_solved(): number;
  /**
   * User moves committed since the latest scramble or reset.
   *
   * Derived from the record: a rewind takes moves back out of it.
   */
  _thorvg_rubiks_committed_move_count(): number;
  /** Returns 1 when a rewind of the user's last move began. */
  _thorvg_rubiks_undo(): number;
  /** Returns 1 when a replay of the last rewound move began. */
  _thorvg_rubiks_redo(): number;
  /** Returns 1 when a rewind of every applied move began. */
  _thorvg_rubiks_solve_rewind(): number;
  /** Returns 1 when a solver here handles the size of cube in hand. */
  _thorvg_rubiks_can_solve(): number;
  /** Returns 1 when a solve of the cube as it stands began. */
  _thorvg_rubiks_solve(): number;
  /** Breaks off a rewind; a no-op for a scramble or a watched pattern. */
  _thorvg_rubiks_stop_playback(): void;
  /** How many moves the record holds, scramble and user moves together. */
  _thorvg_rubiks_timeline_length(): number;
  /** How many of those moves are on the cube; every commit moves it by one. */
  _thorvg_rubiks_timeline_cursor(): number;
  /** Where the scramble stops and the user's own moves begin. */
  _thorvg_rubiks_timeline_scramble_end(): number;
  /**
   * The recorded move at `index`, packed as cube/PackedMove.hpp writes it:
   * axis in bits 0-1, turns in bits 2-3, layer mask from bit 4 up.
   *
   * Zero for an index the record does not hold; a move is never zero.
   */
  _thorvg_rubiks_timeline_move(index: number): number;
  /**
   * Returns 1 when the turn started; face and turns follow cube::Face.
   *
   * Depth 1 is the face itself and depths count inwards, so (face, 1, 1) is
   * the face turn and (face, 1, 2) is the wide move.
   */
  _thorvg_rubiks_turn_face(
    face: number,
    firstDepth: number,
    lastDepth: number,
    faceTurns: number,
  ): number;
  /** Returns 1 when the engine built a cube of that size; 0 changes nothing. */
  _thorvg_rubiks_set_cube_size(size: number): number;
  /** How many layers the cube has along an axis. */
  _thorvg_rubiks_cube_size(): number;
  /** Returns 1 when the mode was accepted; invalid values are rejected. */
  _thorvg_rubiks_set_view_mode(mode: number): number;
  /** The current graphics::ViewMode as an integer. */
  _thorvg_rubiks_view_mode(): number;
  /** Returns 1 when the style was accepted; invalid values are rejected. */
  _thorvg_rubiks_set_flat_style(style: number): number;
  /** The current graphics::FlatStyle as an integer. */
  _thorvg_rubiks_flat_style(): number;
  /** Returns 1 when the palette was accepted; invalid values are rejected. */
  _thorvg_rubiks_set_palette(palette: number): number;

  /** Returns 1 when the canvas theme was accepted; others are rejected. */
  _thorvg_rubiks_set_canvas_theme(theme: number): number;

  /** Returns the current canvas theme as its ABI integer. */
  _thorvg_rubiks_canvas_theme(): number;
  /** The current graphics::Palette as an integer. */
  _thorvg_rubiks_palette(): number;
  /** Returns 1 when taken; out-of-range values are clamped, not refused. */
  _thorvg_rubiks_set_speed_scale(scale: number): number;
  /** The current animation speed multiplier. */
  _thorvg_rubiks_speed_scale(): number;
  /** Restores the turntable camera and nothing else. */
  _thorvg_rubiks_reset_view(): void;
  /** Returns 1 while a gesture, animation, or pending commit is active. */
  _thorvg_rubiks_is_busy(): number;
  _thorvg_rubiks_paint_begin(): number;
  _thorvg_rubiks_paint_cancel(): void;
  _thorvg_rubiks_is_painting(): number;
  _thorvg_rubiks_set_paint_brush(colour: number): number;
  _thorvg_rubiks_paint_brush(): number;
  _thorvg_rubiks_paint_at(x: number, y: number): number;
  _thorvg_rubiks_paint_fill(x: number, y: number): number;
  _thorvg_rubiks_set_paint_filling(wholeFace: number): number;
  _thorvg_rubiks_is_paint_filling(): number;
  _thorvg_rubiks_painted_count(colour: number): number;
  _thorvg_rubiks_paint_apply(): number;
  _thorvg_rubiks_paint_fault(): number;
  _thorvg_rubiks_paint_blamed_count(): number;
  _thorvg_rubiks_paint_blamed_at(index: number): number;
  _thorvg_rubiks_origin_painting_count(): number;
  _thorvg_rubiks_origin_painting_at(index: number): number;
  _thorvg_rubiks_painting_buffer(count: number): number;
  _thorvg_rubiks_restore_painting(size: number, userCount: number): number;
};

/** Factory that creates one engine module instance. */
export default function createThorvgRubiksModule(): Promise<ThorvgRubiksModule>;
