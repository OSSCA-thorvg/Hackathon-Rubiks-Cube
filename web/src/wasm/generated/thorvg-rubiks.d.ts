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
};

/** Factory that creates one engine module instance. */
export default function createThorvgRubiksModule(): Promise<ThorvgRubiksModule>;
