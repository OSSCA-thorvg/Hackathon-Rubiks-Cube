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
  Flat: 2,
} as const;

/** One render-region mode value. */
export type CubeViewMode =
  (typeof CubeViewMode)[keyof typeof CubeViewMode];

/**
 * Which drawing fills the flat region, wherever that region is.
 *
 * A second axis rather than more entries in the list above: which regions are
 * up and what the flat one shows are independent, so a single list would have
 * to hold every pairing and grow by a factor each time a drawing is added.
 */
export const CubeFlatStyle = {
  Net: 0,
  Rings: 1,
  Both: 2,
} as const;

/** One flat-drawing value. */
export type CubeFlatStyle =
  (typeof CubeFlatStyle)[keyof typeof CubeFlatStyle];

/**
 * Which six shades the stickers are drawn in.
 *
 * Two verified sets rather than a color picker: what `HighContrast` promises
 * is that no pair of the six collapses under *any* of the three dichromacies,
 * and that it is further apart than the standard cube under every one of
 * them. That is a property of the set as a whole, which six freely chosen
 * colors could not keep.
 *
 * All three and not the famous one, because they do not pull the same way:
 * protanopia and deuteranopia lose the red-green axis and keep blue-yellow,
 * and tritanopia loses exactly the axis they keep. A set tuned for the first
 * pair alone drifts onto blue-yellow -- it is free distance there -- and
 * arrives worse than the cube it was replacing for the third.
 */
export const CubePalette = {
  Classic: 0,
  HighContrast: 1,
} as const;

/** One sticker-palette value. */
export type CubePalette = (typeof CubePalette)[keyof typeof CubePalette];

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
 * The sizes of cube the engine builds; mirrors its own bounds.
 *
 * The size in use is not mirrored, because it is a choice rather than a fact
 * about the build: everything that needs it asks `cubeSize()`. What is
 * mirrored is the range, so a control can offer it and a shared payload can
 * refuse a size before anything crosses the boundary.
 */
export const MIN_CUBE_SIZE = 2;
export const MAX_CUBE_SIZE = 9;

/** The size the engine opens with, before anyone chooses one. */
export const DEFAULT_CUBE_SIZE = 3;

/** Longest scramble the engine will play; mirrors its limit too. */
export const MAX_SCRAMBLE_MOVES = 100;

/** How many moves a scramble has when nobody has said otherwise. */
export const DEFAULT_SCRAMBLE_MOVES = 20;

/**
 * The longest record a shared state may carry; mirrors the engine's bound.
 *
 * Mirrored the way the scramble limit is, and for the same reason: the number
 * is a fact about the engine, and having it on this side is what lets a
 * payload be refused before anything crosses the boundary.
 */
export const MAX_SHARED_MOVES = 4096;

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
 * Reports whether a value is a scramble length the engine would accept.
 *
 * Said here rather than at each place that asks, because the box a length is
 * typed into and the call that carries it across the boundary were checking
 * the same thing in opposite directions, and only one of them mirrors C++.
 */
export function isValidScrambleMoves(value: number): boolean {
  return Number.isInteger(value) && value >= 1 && value <= MAX_SCRAMBLE_MOVES;
}

/**
 * Reports whether a value crosses the boundary as the uint32 it looks like.
 *
 * Two arguments are one: a scramble's seed and a watching pattern's choice.
 * The engine has no random source, so both are how arbitrariness gets into it,
 * and anything the Emscripten i32 boundary would quietly reinterpret has to be
 * stopped on this side of it.
 */
export function isUint32(value: number): boolean {
  return Number.isInteger(value) && value >= 0 && value <= 0xffffffff;
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

  /**
   * Restarts the cube and plays the scramble for `seed` into it.
   *
   * The cube is still solved when this returns: the moves are turned rather
   * than applied, so the caller has to run frames for the scramble to arrive.
   */
  scramble(seed: number, moveCount: number): void {
    this.assertUsable();
    if (!isUint32(seed)) {
      throw new Error(`Invalid scramble seed ${seed}.`);
    }
    if (!isValidScrambleMoves(moveCount)) {
      throw new Error(`Invalid scramble move count ${moveCount}.`);
    }
    if (this.module._thorvg_rubiks_scramble(seed, moveCount) === 0) {
      throw new Error('Engine rejected the scramble.');
    }
  }

  /** Restores the solved cube while preserving camera and view mode. */
  resetCube(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_reset_cube();
  }

  /**
   * Puts a shared record on the cube, at once and without animation.
   *
   * The record crosses the way the pixels do and in the opposite direction:
   * the engine hands over the address of a buffer it owns, this writes the
   * packed words into it through a view, and one further call reads all of
   * them. Nothing else may be called in between -- another call is free to
   * grow the heap, which would leave the view pointing at memory that has
   * moved -- and nothing is, so the view cannot go stale.
   *
   * Not a transaction on either side. A refusal leaves the cube exactly as it
   * was, which at the only moment this is called is a cube that has just been
   * made, so there is nothing to roll back and nothing to retry. The size
   * travels with the record for that reason: setting it first would leave a
   * cube nobody asked for behind a record that was then turned down.
   *
   * @returns false when the engine would not take the record, which is an
   *          answer about the record rather than a failure of the engine.
   * @throws Error when disposed, or when the engine hands back a buffer that
   *         is not there -- which is not an answer about the record at all,
   *         and takes the same route out as a bad pixel buffer.
   */
  restoreSession(
    size: number,
    scramble: readonly number[],
    user: readonly number[],
  ): boolean {
    this.assertUsable();

    const total = scramble.length + user.length;
    if (total === 0 || total > MAX_SHARED_MOVES) return false;

    const pointer = this.module._thorvg_rubiks_restore_buffer(total);

    // Zero is the engine's own refusal, and it is about the record. Anything
    // else that fails the heap contract is the engine being wrong about its
    // own memory, so it is raised rather than reported as a damaged link --
    // telling someone their link is bad when it was not would send them off to
    // fix the one thing that is not broken.
    if (pointer === 0) return false;

    const byteLength = total * 4;
    if (!this.heapRegionUsable(pointer, byteLength)) {
      throw new Error(
        `Engine returned an invalid restore buffer for ${total} moves: ` +
          `pointer ${pointer}, byte length ${byteLength}.`,
      );
    }

    const words = new Uint32Array(this.module.HEAPU8.buffer, pointer, total);
    words.set(scramble, 0);
    words.set(user, scramble.length);

    return (
      this.module._thorvg_rubiks_restore_apply(
        size,
        scramble.length,
        user.length,
      ) !== 0
    );
  }

  /**
   * Begins watching a repeating pattern, picked by `choice`.
   *
   * Every uint32 names a pattern, so this cannot be refused for the value --
   * only for watching having already begun, which is what false means. The
   * caller has to run frames, the same as for a scramble; unlike a scramble
   * they never stop coming until watching does.
   */
  ambientStart(choice: number): boolean {
    this.assertUsable();
    if (!isUint32(choice)) {
      throw new Error(`Invalid ambient choice ${choice}.`);
    }
    return this.module._thorvg_rubiks_ambient_start(choice) !== 0;
  }

  /**
   * Ends watching and puts the cube from before it straight back.
   *
   * A no-op when nothing is being watched. Nothing is animated, so the caller
   * draws once afterwards rather than running frames.
   */
  ambientStop(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_ambient_stop();
  }

  /** Reports whether a pattern is being watched right now. */
  isAmbient(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_ambient() !== 0;
  }

  /** Reports whether the committed logical cube is solved. */
  isSolved(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_solved() !== 0;
  }

  /**
   * Returns user moves committed since the latest scramble or reset.
   *
   * Read off the engine's record rather than counted, so a rewind takes moves
   * back out of it as surely as making them puts them in.
   */
  committedMoveCount(): number {
    this.assertUsable();
    return this.countFrom(
      this.module._thorvg_rubiks_committed_move_count(),
      'move count',
    );
  }

  /**
   * Turns the user's last move back, playing it as a sequence of one.
   *
   * @returns false when there is nothing of the user's own on the cube, or
   *          while anything else owns it.
   */
  undo(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_undo() !== 0;
  }

  /** Replays the move a rewind took off. @returns false with nothing to. */
  redo(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_redo() !== 0;
  }

  /**
   * Rewinds every applied move, leaving a solved cube.
   *
   * The same command undo is, with a further target: it plays back through the
   * scramble as well, and the caller has to run frames for it to arrive.
   *
   * @returns false with nothing applied, or while anything else owns the cube.
   */
  solveRewind(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_solve_rewind() !== 0;
  }

  /**
   * Breaks off a rewind, keeping every move it has already turned back.
   *
   * A no-op for a scramble or a watched pattern -- which is what makes a press
   * that arrives a frame after the control went away harmless.
   */
  stopPlayback(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_stop_playback();
  }

  /** How many moves the record holds, scramble and user moves together. */
  timelineLength(): number {
    this.assertUsable();
    return this.countFrom(
      this.module._thorvg_rubiks_timeline_length(),
      'timeline length',
    );
  }

  /**
   * How many of those moves are on the cube right now.
   *
   * Every commit moves this by exactly one, so watching it change is watching
   * moves commit -- which is why there is no commit counter beside it.
   */
  timelineCursor(): number {
    this.assertUsable();
    return this.countFrom(
      this.module._thorvg_rubiks_timeline_cursor(),
      'timeline cursor',
    );
  }

  /** Where the scramble stops and the user's own moves begin. */
  timelineScrambleEnd(): number {
    this.assertUsable();
    return this.countFrom(
      this.module._thorvg_rubiks_timeline_scramble_end(),
      'timeline scramble end',
    );
  }

  /**
   * The recorded move at `index`, packed into one word.
   *
   * The notation is assembled from it on this side, so nothing but numbers
   * crosses the boundary and a change of notation never reaches the engine.
   *
   * @returns zero for an index the record does not hold, which is a value a
   *          packed move can never take.
   */
  timelineMove(index: number): number {
    this.assertUsable();
    if (!isUint32(index)) {
      throw new Error(`Invalid timeline index ${index}.`);
    }
    const packed = this.module._thorvg_rubiks_timeline_move(index);
    if (!Number.isSafeInteger(packed)) {
      throw new Error(`Engine returned an invalid packed move ${packed}.`);
    }

    // Read back unsigned rather than checked for sign, which is what makes
    // this the one query that does not go through countFrom: a count coming
    // back negative is a boundary fault, but every bit of a packed move is
    // data, and the topmost one is a layer like any other.
    return packed >>> 0;
  }

  /** Starts one animated face turn, returning false while the engine is busy. */
  turnFace(
    face: CubeFace,
    firstDepth: number,
    lastDepth: number,
    faceTurns: FaceTurns,
  ): boolean {
    this.assertUsable();
    return (
      this.module._thorvg_rubiks_turn_face(
        face,
        firstDepth,
        lastDepth,
        faceTurns,
      ) !== 0
    );
  }

  /**
   * Builds a cube of a different size, starting the session over.
   *
   * How the cube is being looked at survives; the record and anything playing
   * do not. A size the engine does not build is refused rather than clamped,
   * the way a scramble length is: a five is not what someone asking for a
   * fifty meant.
   *
   * @returns false when the engine would not build that size.
   */
  setCubeSize(size: number): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_set_cube_size(size) !== 0;
  }

  /** How many layers the cube has along an axis. */
  cubeSize(): number {
    this.assertUsable();
    return this.module._thorvg_rubiks_cube_size();
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
      mode !== CubeViewMode.Flat
    ) {
      throw new Error(`Engine returned an invalid view mode ${mode}.`);
    }
    return mode;
  }

  /** Changes which drawing the flat region shows. */
  setFlatStyle(style: CubeFlatStyle): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_flat_style(style) === 0) {
      throw new Error(`Engine rejected flat style ${style}.`);
    }
  }

  /** Returns the drawing the flat region is showing. */
  flatStyle(): CubeFlatStyle {
    this.assertUsable();
    const style = this.module._thorvg_rubiks_flat_style();
    if (
      style !== CubeFlatStyle.Net &&
      style !== CubeFlatStyle.Rings &&
      style !== CubeFlatStyle.Both
    ) {
      throw new Error(`Engine returned an invalid flat style ${style}.`);
    }
    return style;
  }

  /**
   * Changes which six shades the stickers are drawn in.
   *
   * Accepted while the engine is busy, unlike the view commands: the palette
   * is read where a sticker becomes pixels and nowhere else, so it disturbs
   * neither a turn in progress nor anything the cube is.
   */
  setPalette(palette: CubePalette): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_palette(palette) === 0) {
      throw new Error(`Engine rejected palette ${palette}.`);
    }
  }

  /** Returns the palette the stickers are being drawn in. */
  palette(): CubePalette {
    this.assertUsable();
    const palette = this.module._thorvg_rubiks_palette();
    if (
      palette !== CubePalette.Classic &&
      palette !== CubePalette.HighContrast
    ) {
      throw new Error(`Engine returned an invalid palette ${palette}.`);
    }
    return palette;
  }

  /**
   * Sets how much faster than the written tempos every animation runs.
   *
   * `duration = base / scale`, so 2 is twice as fast and 0.5 is half. One
   * value covers a drag release, a scramble, a rewind and a watched pattern.
   * Values outside the engine's range are clamped by it rather than refused,
   * so what comes back here is only ever a value that was not a number.
   */
  setSpeedScale(scale: number): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_speed_scale(scale) === 0) {
      throw new Error(`Engine rejected speed scale ${scale}.`);
    }
  }

  /** Returns the multiplier the engine settled on, after its own clamp. */
  speedScale(): number {
    this.assertUsable();
    const scale = this.module._thorvg_rubiks_speed_scale();
    if (!Number.isFinite(scale) || scale <= 0) {
      throw new Error(`Engine returned an invalid speed scale ${scale}.`);
    }
    return scale;
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
   * Checks one count coming back across the boundary, whichever count it is.
   *
   * Every one of them is a uint32 on the other side, so anything the i32
   * boundary would have reinterpreted arrives negative or fractional and is
   * refused here rather than becoming a nonsense length somewhere later.
   */
  private countFrom(value: number, name: string): number {
    if (!Number.isSafeInteger(value) || value < 0) {
      throw new Error(`Engine returned an invalid ${name} ${value}.`);
    }
    return value;
  }

  /**
   * Whether an engine-owned region may have a typed view built over it.
   *
   * The one rule standing between an address the engine claims to own and a
   * read or write into arbitrary WASM memory: a safe integer, non-zero,
   * four-byte aligned, and ending inside the current heap. Both directions
   * ask it -- the pixels come out through one and a restored record goes in
   * through another -- so it is written once, and tightening it later
   * tightens both.
   */
  private heapRegionUsable(pointer: number, byteLength: number): boolean {
    return (
      Number.isSafeInteger(pointer) &&
      Number.isSafeInteger(byteLength) &&
      pointer > 0 &&
      pointer % 4 === 0 &&
      pointer + byteLength <= this.module.HEAPU8.byteLength
    );
  }

  /**
   * Re-queries and validates the pixel source after initialize or resize.
   *
   * State is committed only when the full metadata contract holds: a usable
   * heap region, and the exact width * height * 4 byte length on top of it.
   */
  private refreshPixelSource(): void {
    const pointer = this.module._thorvg_rubiks_pixel_buffer();
    const length = this.module._thorvg_rubiks_pixel_byte_length();
    const expectedLength = this.width * this.height * 4;

    // The length is the pixel buffer's own extra condition: a region can be
    // perfectly usable and still be the wrong size for this canvas.
    if (length !== expectedLength || !this.heapRegionUsable(pointer, length)) {
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
