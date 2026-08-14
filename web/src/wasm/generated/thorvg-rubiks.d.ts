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
  /** Returns 1 when the cube was replaced by the scramble for `seed`. */
  _thorvg_rubiks_scramble(seed: number): number;
  /** Restores the solved cube, keeping the camera and the view mode. */
  _thorvg_rubiks_reset_cube(): void;
  /** Returns 1 when the committed logical cube is solved. */
  _thorvg_rubiks_is_solved(): number;
  /** User moves committed since the latest scramble or reset. */
  _thorvg_rubiks_committed_move_count(): number;
  /** Returns 1 when the turn started; face and turns follow cube::Face. */
  _thorvg_rubiks_turn_face(face: number, faceTurns: number): number;
  /** Returns 1 when the mode was accepted; invalid values are rejected. */
  _thorvg_rubiks_set_view_mode(mode: number): number;
  /** The current graphics::ViewMode as an integer. */
  _thorvg_rubiks_view_mode(): number;
  /** Returns 1 when the style was accepted; invalid values are rejected. */
  _thorvg_rubiks_set_flat_style(style: number): number;
  /** The current graphics::FlatStyle as an integer. */
  _thorvg_rubiks_flat_style(): number;
  /** Restores the turntable camera and nothing else. */
  _thorvg_rubiks_reset_view(): void;
  /** Returns 1 while a gesture, animation, or pending commit is active. */
  _thorvg_rubiks_is_busy(): number;
};

/** Factory that creates one engine module instance. */
export default function createThorvgRubiksModule(): Promise<ThorvgRubiksModule>;
