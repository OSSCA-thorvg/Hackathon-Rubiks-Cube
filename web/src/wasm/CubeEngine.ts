import type { ThorvgRubiksModule } from './generated/thorvg-rubiks.js';

/**
 * TypeScript boundary for the WASM engine.
 *
 * This is the only Web module that touches the generated Emscripten module.
 * It owns the module lifecycle, translates C ABI return codes into errors,
 * and connects the engine-owned pixel buffer to a canvas without copying
 * pixels into a separate JavaScript array.
 */

/** Drawing buffer size in physical pixels. */
export type CubeEngineSize = {
  readonly width: number;
  readonly height: number;
};

/** Options for CubeEngine.create(). */
export type CubeEngineOptions = {
  /**
   * Overrides how the Emscripten module is loaded.
   * Unit tests inject a fake module factory here; the default loads the
   * generated module from the Vite module graph.
   */
  readonly loadModule?: () => Promise<ThorvgRubiksModule>;
};

/** External faces in the same stable order as the C++ cube domain. */
export const CubeFace = {
  Right: 0,
  Left: 1,
  Up: 2,
  Down: 3,
  Front: 4,
  Back: 5,
} as const;

/** One stable external face value. */
export type CubeFace = (typeof CubeFace)[keyof typeof CubeFace];

/** Render regions available to the browser UI. */
export const CubeViewMode = {
  Cube3D: 0,
  Both: 1,
  Net: 2,
} as const;

/** One render-region mode value. */
export type CubeViewMode =
  (typeof CubeViewMode)[keyof typeof CubeViewMode];

/** Face-relative turns accepted by programmatic move controls. */
export type FaceTurns = -1 | 1 | 2;

async function loadGeneratedModule(): Promise<ThorvgRubiksModule> {
  // The generated module lives in the Vite module graph, so this literal
  // dynamic import is code-split and the WASM binary referenced through
  // `new URL(..., import.meta.url)` is emitted as a hashed asset.
  const { default: createModule } = await import(
    './generated/thorvg-rubiks.js'
  );

  return createModule();
}

/** Largest drawing buffer dimension; mirrors the C++ engine limit. */
export const MAX_DIMENSION = 8192;

/**
 * Computes the drawing buffer size for a CSS size and device pixel ratio.
 *
 * Non-finite inputs normalize to one pixel; the result is clamped to at
 * least one pixel and at most the engine limit, so the output always
 * satisfies isValidDimension().
 */
export function computeDrawingBufferSize(
  cssWidth: number,
  cssHeight: number,
  devicePixelRatio: number,
): CubeEngineSize {
  const clamp = (value: number): number => {
    if (!Number.isFinite(value)) return 1;
    return Math.min(MAX_DIMENSION, Math.max(1, Math.round(value)));
  };

  return {
    width: clamp(cssWidth * devicePixelRatio),
    height: clamp(cssHeight * devicePixelRatio),
  };
}

/**
 * Reports whether a value is a valid drawing buffer dimension: a positive
 * integer no larger than MAX_DIMENSION.
 */
export function isValidDimension(value: number): boolean {
  return Number.isInteger(value) && value >= 1 && value <= MAX_DIMENSION;
}

/**
 * Owns one engine module instance and presents its pixel buffer on a canvas.
 */
export class CubeEngine {
  private readonly module: ThorvgRubiksModule;
  private readonly canvas: HTMLCanvasElement;
  private readonly context: CanvasRenderingContext2D;
  private width: number;
  private height: number;
  private pixelPointer = 0;
  private pixelByteLength = 0;
  private view: Uint8ClampedArray<ArrayBuffer> | null = null;
  private image: ImageData | null = null;
  private disposed = false;

  /**
   * Loads the WASM module, initializes the engine to the canvas CSS size,
   * and sizes the canvas drawing buffer to match.
   *
   * @throws Error when the module, the 2D context, or initialization fails.
   */
  static async create(
    canvas: HTMLCanvasElement,
    options: CubeEngineOptions = {},
  ): Promise<CubeEngine> {
    const module = await (options.loadModule ?? loadGeneratedModule)();

    const context = canvas.getContext('2d');
    if (context === null) {
      throw new Error('CubeEngine requires a 2D canvas context.');
    }

    const size = computeDrawingBufferSize(
      canvas.clientWidth,
      canvas.clientHeight,
      window.devicePixelRatio,
    );

    if (module._thorvg_rubiks_initialize(size.width, size.height) === 0) {
      throw new Error(
        `Engine initialization failed for ${size.width}x${size.height}.`,
      );
    }

    // From here the native side holds resources: any failure before the
    // engine instance exists must shut the module down again, or the only
    // owner able to release them is lost.
    try {
      canvas.width = size.width;
      canvas.height = size.height;
      return new CubeEngine(module, canvas, context, size);
    } catch (error) {
      module._thorvg_rubiks_shutdown();
      throw error;
    }
  }

  private constructor(
    module: ThorvgRubiksModule,
    canvas: HTMLCanvasElement,
    context: CanvasRenderingContext2D,
    size: CubeEngineSize,
  ) {
    this.module = module;
    this.canvas = canvas;
    this.context = context;
    this.width = size.width;
    this.height = size.height;
    this.refreshPixelSource();
  }

  /**
   * Resizes the engine drawing buffer and the canvas to a new size.
   *
   * A size equal to the current one returns without calling the engine.
   *
   * @throws Error when disposed or when the engine rejects the resize.
   */
  resize(size: CubeEngineSize): void {
    this.assertUsable();

    // CubeEngineSize is compile-time only; reject values the Emscripten
    // i32 boundary would silently coerce into unrelated integers.
    if (!isValidDimension(size.width) || !isValidDimension(size.height)) {
      throw new Error(
        `Invalid drawing buffer size ${size.width}x${size.height}.`,
      );
    }

    if (size.width === this.width && size.height === this.height) return;

    if (this.module._thorvg_rubiks_resize(size.width, size.height) === 0) {
      throw new Error(`Engine resize failed for ${size.width}x${size.height}.`);
    }

    this.width = size.width;
    this.height = size.height;
    this.canvas.width = size.width;
    this.canvas.height = size.height;

    try {
      this.refreshPixelSource();
    } catch (error) {
      // The new size is already committed; without a valid buffer the
      // instance cannot recover, so release the native side entirely.
      this.dispose();
      throw error;
    }
  }

  /**
   * Renders one frame and presents the pixel buffer on the canvas.
   *
   * @throws Error when disposed or when the engine rendering fails.
   */
  render(): void {
    this.assertUsable();

    if (this.module._thorvg_rubiks_render() === 0) {
      throw new Error('Engine rendering failed.');
    }

    this.context.putImageData(this.imageData(), 0, 0);
  }

  /**
   * Begins a gesture at a point in drawing buffer pixels.
   *
   * Over the cube that is a layer drag, elsewhere a viewpoint sweep.
   *
   * @returns true when a gesture began.
   * @throws Error when disposed.
   */
  pointerDown(x: number, y: number): boolean {
    this.assertUsable();

    return this.module._thorvg_rubiks_pointer_down(x, y) !== 0;
  }

  /** Continues the active gesture. @throws Error when disposed. */
  pointerMove(x: number, y: number): void {
    this.assertUsable();

    this.module._thorvg_rubiks_pointer_move(x, y);
  }

  /** Releases the active gesture. @throws Error when disposed. */
  pointerUp(): void {
    this.assertUsable();

    this.module._thorvg_rubiks_pointer_up();
  }

  /**
   * Abandons the active gesture without turning the cube.
   *
   * @throws Error when disposed.
   */
  pointerCancel(): void {
    this.assertUsable();

    this.module._thorvg_rubiks_pointer_cancel();
  }

  /**
   * Advances animation by an elapsed time in milliseconds.
   *
   * The engine reads no clock, so this is where time enters it.
   *
   * @returns true while further frames still have to be drawn.
   * @throws Error when disposed.
   */
  advance(elapsedMs: number): boolean {
    this.assertUsable();

    return this.module._thorvg_rubiks_advance(elapsedMs) !== 0;
  }

  /** Replaces the cube with a deterministic scramble for `seed`. */
  scramble(seed: number): void {
    this.assertUsable();
    if (!Number.isInteger(seed) || seed < 0 || seed > 0xffffffff) {
      throw new Error(`Invalid scramble seed ${seed}.`);
    }
    if (this.module._thorvg_rubiks_scramble(seed) === 0) {
      throw new Error('Engine rejected the scramble.');
    }
  }

  /** Restores the solved cube while preserving camera and view mode. */
  resetCube(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_reset_cube();
  }

  /** Reports whether the committed logical cube is solved. */
  isSolved(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_solved() !== 0;
  }

  /** Returns user moves committed since the latest scramble or reset. */
  committedMoveCount(): number {
    this.assertUsable();
    const count = this.module._thorvg_rubiks_committed_move_count();
    if (!Number.isSafeInteger(count) || count < 0) {
      throw new Error(`Engine returned an invalid move count ${count}.`);
    }
    return count;
  }

  /** Starts one animated face turn, returning false while the engine is busy. */
  turnFace(face: CubeFace, faceTurns: FaceTurns): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_turn_face(face, faceTurns) !== 0;
  }

  /** Changes which cube views are rendered. */
  setViewMode(mode: CubeViewMode): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_view_mode(mode) === 0) {
      throw new Error(`Engine rejected view mode ${mode}.`);
    }
  }

  /** Returns the currently selected render mode. */
  viewMode(): CubeViewMode {
    this.assertUsable();
    const mode = this.module._thorvg_rubiks_view_mode();
    if (
      mode !== CubeViewMode.Cube3D &&
      mode !== CubeViewMode.Both &&
      mode !== CubeViewMode.Net
    ) {
      throw new Error(`Engine returned an invalid view mode ${mode}.`);
    }
    return mode;
  }

  /** Restores only the turntable camera. */
  resetView(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_reset_view();
  }

  /**
   * Reports whether a gesture, animation, or pending commit is active.
   * An orbit sweep is not busy; the viewpoint moves while the cube cannot.
   */
  isBusy(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_busy() !== 0;
  }

  /**
   * Shuts the engine down and drops the borrowed pixel buffer views.
   *
   * Safe to call more than once; other methods fail afterwards.
   */
  dispose(): void {
    if (this.disposed) return;

    this.disposed = true;
    this.view = null;
    this.image = null;
    this.module._thorvg_rubiks_shutdown();
  }

  private assertUsable(): void {
    if (this.disposed) {
      throw new Error('CubeEngine has been disposed.');
    }
  }

  /**
   * Re-queries and validates the pixel source after initialize or resize.
   *
   * State is committed only when the full metadata contract holds: safe
   * integers, the exact width * height * 4 byte length, 4-byte pixel
   * alignment, and pointer plus length inside the current heap.
   */
  private refreshPixelSource(): void {
    const pointer = this.module._thorvg_rubiks_pixel_buffer();
    const length = this.module._thorvg_rubiks_pixel_byte_length();
    const expectedLength = this.width * this.height * 4;

    const valid =
      Number.isSafeInteger(pointer) &&
      Number.isSafeInteger(length) &&
      pointer > 0 &&
      pointer % 4 === 0 &&
      length === expectedLength &&
      pointer + length <= this.module.HEAPU8.byteLength;

    if (!valid) {
      throw new Error(
        `Engine returned an invalid pixel buffer for ` +
          `${this.width}x${this.height}: pointer ${pointer}, ` +
          `byte length ${length}.`,
      );
    }

    this.pixelPointer = pointer;
    this.pixelByteLength = length;
    this.view = null;
    this.image = null;
  }

  /**
   * Returns an ImageData view over the WASM pixel buffer.
   *
   * The view is reused while the buffer stays valid and is recreated after
   * memory growth detaches the previous ArrayBuffer.
   */
  private imageData(): ImageData {
    const heapBuffer = this.module.HEAPU8.buffer;
    const stale =
      this.view === null ||
      this.image === null ||
      this.view.buffer !== heapBuffer ||
      this.view.buffer.byteLength === 0;

    if (stale) {
      this.view = new Uint8ClampedArray(
        heapBuffer,
        this.pixelPointer,
        this.pixelByteLength,
      );
      this.image = new ImageData(this.view, this.width, this.height);
    }

    return this.image as ImageData;
  }
}
