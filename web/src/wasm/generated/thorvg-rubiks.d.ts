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
};

/** Factory that creates one engine module instance. */
export default function createThorvgRubiksModule(): Promise<ThorvgRubiksModule>;
