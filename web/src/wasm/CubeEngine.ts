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
  /**
   * The drawing buffer the engine starts at. A page with more than one view
   * decides it, since the density is shared between them; without it the
   * canvas's own box at the device pixel ratio is used.
   */
  readonly size?: CubeEngineSize;
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

/**
 * The surfaces a page with a canvas per view hands its canvases to.
 *
 * Surface 0 is the one the engine is created with; it holds every scene until
 * it is told otherwise, which is all a page with one canvas needs.
 */
export const CubeSurface = {
  Cube: 0,
  Net: 1,
  Rings: 2,
  Axes: 3,
} as const;

/** One surface id. */
export type CubeSurface = (typeof CubeSurface)[keyof typeof CubeSurface];

/** The scenes a surface can show, as the bits the C ABI takes. */
export const CubeScene = {
  Cube: 1,
  Net: 2,
  Rings: 4,
  Axes: 8,
} as const;

/** Every scene bit: what surface 0 holds until it is told otherwise. */
export const ALL_SCENES =
  CubeScene.Cube | CubeScene.Net | CubeScene.Rings | CubeScene.Axes;

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

/**
 * The six sticker colours, as the C ABI numbers them.
 *
 * Not shades. These name which sticker a square is -- red, orange, white,
 * yellow, green, blue -- and what each is drawn in is the palette's business,
 * which is why somebody colouring their own cube picks from six identities
 * rather than from a colour wheel.
 */
export const CubeStickerColour = {
  Red: 0,
  Orange: 1,
  White: 2,
  Yellow: 3,
  Green: 4,
  Blue: 5,
} as const;

/** One sticker colour. */
export type CubeStickerColour =
  (typeof CubeStickerColour)[keyof typeof CubeStickerColour];

/**
 * The keyboard's place on a colouring: which cell of the net, and what colour
 * the draft has there now.
 */
export type PaintCursor = {
  readonly face: CubeFace;
  readonly col: number;
  readonly row: number;
  readonly colour: CubeStickerColour;
};

/** Every sticker colour, in the order the engine numbers them. */
export const STICKER_COLOURS: readonly CubeStickerColour[] = [
  CubeStickerColour.Red,
  CubeStickerColour.Orange,
  CubeStickerColour.White,
  CubeStickerColour.Yellow,
  CubeStickerColour.Green,
  CubeStickerColour.Blue,
];

/**
 * Why a colouring was refused, as the C ABI numbers `cube::PaintFault`.
 *
 * Held here as well as in the engine because the page has a sentence to write
 * for each of them, and a number arriving that this build has no sentence for
 * is a mismatch worth noticing rather than a blank line.
 */
export const CubePaintFault = {
  None: 0,
  ColourCount: 1,
  OrbitCount: 2,
  OppositePairs: 3,
  ImpossiblePiece: 4,
  RepeatedPiece: 5,
  CornerTwist: 6,
  EdgeFlip: 7,
  Permutation: 8,
} as const;

/** One reason a colouring was refused. */
export type CubePaintFault =
  (typeof CubePaintFault)[keyof typeof CubePaintFault];

/**
 * The ground the software canvas is cleared to, as the C ABI numbers it.
 *
 * Two values where the page offers three: `System` is a question about the
 * machine and it is answered before anything reaches the engine. A renderer
 * needs a color, not a preference.
 *
 * Not the sticker palette. A palette is the six shades a cube is read by and
 * this is the one surface it is read against, so the two axes are independent
 * and all four combinations have to stay legible.
 *
 * An object rather than an `enum`, like every other ABI enum here: the build
 * compiles TypeScript with `erasableSyntaxOnly`, which has no room for a form
 * that emits code.
 */
export const CubeCanvasTheme = {
  Light: 0,
  Dark: 1,
} as const;

/** One canvas-theme value. */
export type CubeCanvasTheme =
  (typeof CubeCanvasTheme)[keyof typeof CubeCanvasTheme];

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
export const MAX_CUBE_SIZE = 28;

/**
 * The size past which a solve is worth warning about before it is asked for.
 *
 * Not a limit: every size up to the largest solves, and correctly. What grows
 * is the wait, and the wait is spent inside one call into the engine -- so
 * nothing on the page answers until it comes back, and there is no stopping
 * it half way. Nine was the largest cube this application built for a long
 * time and takes a few seconds; past that the number climbs into minutes.
 *
 * Mirrored here rather than asked of the engine because it is a fact about
 * what a person should be told, not about what the engine will do.
 */
export const SOLVE_WARNING_CUBE_SIZE = 10;

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
 * One canvas showing one engine surface.
 *
 * Holds the view over that surface's pixels and the frame it last copied
 * out, so a surface the engine did not draw again is not copied again either.
 */
type Presentation = {
  readonly id: CubeSurface;
  readonly canvas: HTMLCanvasElement;
  readonly context: CanvasRenderingContext2D;
  /** The size the engine last took for the surface; 0 by 0 while put away. */
  width: number;
  height: number;
  pointer: number;
  byteLength: number;
  view: Uint8ClampedArray<ArrayBuffer> | null;
  image: ImageData | null;
  /** The engine frame the canvas was last written from, or -1 for none. */
  frame: number;
};

function presentationOf(
  id: CubeSurface,
  canvas: HTMLCanvasElement,
): Presentation {
  const context = canvas.getContext('2d');
  if (context === null) {
    throw new Error('CubeEngine requires a 2D canvas context.');
  }
  return {
    id,
    canvas,
    context,
    width: 0,
    height: 0,
    pointer: 0,
    byteLength: 0,
    view: null,
    image: null,
    frame: -1,
  };
}

/**
 * Owns one engine module instance and presents its surfaces on canvases.
 *
 * The canvas it is created with shows surface 0; a page with a canvas per
 * view hands the others over with presentSurface(). Every canvas is then
 * sized, drawn and pressed the same way, whichever surface it shows.
 */
export class CubeEngine {
  private readonly module: ThorvgRubiksModule;
  /** Every canvas the engine draws on, by surface. */
  private readonly presentations = new Map<CubeSurface, Presentation>();
  private disposed = false;

  /**
   * Loads the WASM module and initializes the engine to a canvas: at the
   * size the options give, or at the canvas's CSS box otherwise.
   *
   * @throws Error when the module, the 2D context, or initialization fails.
   */
  static async create(
    canvas: HTMLCanvasElement,
    options: CubeEngineOptions = {},
  ): Promise<CubeEngine> {
    const module = await (options.loadModule ?? loadGeneratedModule)();
    const first = presentationOf(CubeSurface.Cube, canvas);

    const size =
      options.size ??
      computeDrawingBufferSize(
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
      return new CubeEngine(module, first, size);
    } catch (error) {
      module._thorvg_rubiks_shutdown();
      throw error;
    }
  }

  private constructor(
    module: ThorvgRubiksModule,
    first: Presentation,
    size: CubeEngineSize,
  ) {
    this.module = module;
    this.presentations.set(first.id, first);
    this.adopt(first, size);
  }

  /**
   * Hands a canvas to one of the other surfaces, to show what it draws.
   *
   * The surface has no size until resizeSurface() gives it one; until then
   * there is nothing on it to show.
   *
   * @throws Error when disposed, for surface 0 -- which shows on the canvas
   *         the engine was created with -- or without a 2D context.
   */
  presentSurface(id: CubeSurface, canvas: HTMLCanvasElement): void {
    this.assertUsable();
    if (id === CubeSurface.Cube || !Object.values(CubeSurface).includes(id)) {
      throw new Error(`Surface ${id} cannot be given a canvas.`);
    }
    this.presentations.set(id, presentationOf(id, canvas));
  }

  /**
   * Says which scenes a surface shows, as a set of CubeScene bits.
   *
   * @throws Error when disposed or when the engine refuses the set.
   */
  setSurfaceScenes(id: CubeSurface, scenes: number): void {
    this.assertUsable();
    if (!Number.isInteger(scenes) || (scenes & ~ALL_SCENES) !== 0 || scenes < 0) {
      throw new Error(`Invalid scene set ${scenes}.`);
    }
    if (this.module._thorvg_rubiks_set_surface_scenes(id, scenes) === 0) {
      throw new Error(`Engine refused scenes ${scenes} for surface ${id}.`);
    }
  }

  /**
   * Resizes one surface and its canvas, or puts it away with null.
   *
   * A surface put away draws nothing and holds no buffer: what a canvas the
   * page is not showing needs. The engine is asked only when the size is not
   * the one it already has, which is what the answer says.
   *
   * @returns whether the size changed, and so a frame is owed.
   * @throws Error when disposed, for a surface with no canvas, or when the
   *         engine rejects the size.
   */
  resizeSurface(id: CubeSurface, size: CubeEngineSize | null): boolean {
    this.assertUsable();

    // CubeEngineSize is compile-time only; reject values the Emscripten
    // i32 boundary would silently coerce into unrelated integers.
    if (
      size !== null &&
      (!isValidDimension(size.width) || !isValidDimension(size.height))
    ) {
      throw new Error(
        `Invalid drawing buffer size ${size.width}x${size.height}.`,
      );
    }

    const presentation = this.presentations.get(id);
    if (presentation === undefined) {
      throw new Error(`Surface ${id} has no canvas.`);
    }

    const width = size?.width ?? 0;
    const height = size?.height ?? 0;
    if (width === presentation.width && height === presentation.height) {
      return false;
    }

    if (this.module._thorvg_rubiks_resize_surface(id, width, height) === 0) {
      throw new Error(
        `Engine resize failed for surface ${id} at ${width}x${height}.`,
      );
    }

    try {
      this.adopt(presentation, size);
    } catch (error) {
      // The new size is already committed; without a valid buffer the
      // instance cannot recover, so release the native side entirely.
      this.dispose();
      throw error;
    }
    return true;
  }

  /**
   * Draws what changed and copies out the surfaces that were drawn.
   *
   * The engine skips a surface whose picture would not change, and says how
   * many frames each has had; a canvas is written only when that count has
   * moved since it was last written.
   *
   * @throws Error when disposed or when the engine rendering fails.
   */
  render(): void {
    this.assertUsable();

    if (this.module._thorvg_rubiks_render() === 0) {
      throw new Error('Engine rendering failed.');
    }

    for (const presentation of this.presentations.values()) {
      if (presentation.byteLength === 0) continue;
      const frame = this.module._thorvg_rubiks_surface_frame(presentation.id);
      if (frame === presentation.frame) continue;
      presentation.context.putImageData(this.imageOf(presentation), 0, 0);
      presentation.frame = frame;
    }
  }

  /**
   * Begins a gesture on one surface, at a point in its drawing buffer pixels.
   *
   * What it can start is what that surface shows. A point outside a surface
   * showing the cube still sweeps the viewpoint, which is how a press beside
   * the cube's canvas reaches it.
   *
   * @returns true when a gesture began.
   * @throws Error when disposed.
   */
  pointerDownOn(id: CubeSurface, x: number, y: number): boolean {
    this.assertUsable();

    return this.module._thorvg_rubiks_pointer_down_on(id, x, y) !== 0;
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
   * Whether the engine holds a solver for the size of cube in hand.
   *
   * The size alone. Whether a solve may begin at this moment is `isBusy()`,
   * the same question every other command is gated on.
   */
  canSolve(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_can_solve() !== 0;
  }

  /**
   * Works out how to solve the cube as it stands, and plays that.
   *
   * Forwards, unlike the rewind above: it reads the cube rather than the
   * record, so it finishes a cube that arrived through a shared link instead
   * of replaying the sender's session backwards.
   *
   * @returns false for a cube already solved, a size with no solver, and
   *          while anything else owns the cube.
   */
  solve(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_solve() !== 0;
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
   * Chooses the ground the cube is drawn against.
   *
   * Accepted while the engine is busy, for the same reason the palette is:
   * the color is read where the target is cleared and nowhere else, so a
   * person switching the page to Light in the middle of a watched pattern
   * gets a light page and the pattern keeps running.
   */
  setCanvasTheme(theme: CubeCanvasTheme): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_canvas_theme(theme) === 0) {
      throw new Error(`Engine rejected canvas theme ${theme}.`);
    }
  }

  /** Returns the ground the cube is being drawn against. */
  canvasTheme(): CubeCanvasTheme {
    this.assertUsable();
    const theme = this.module._thorvg_rubiks_canvas_theme();
    if (theme !== CubeCanvasTheme.Light && theme !== CubeCanvasTheme.Dark) {
      throw new Error(`Engine returned an invalid canvas theme ${theme}.`);
    }
    return theme;
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

  /**
   * Replaces the lights the 3D view is drawn under.
   *
   * A flat list in the engine's order -- ambient, attenuation, saturation,
   * then x, y, z, diffuse, specular, shininess for each lamp (one to four)
   * -- written into a buffer the engine owns, the way a shared record is. For
   * tuning by eye from the address bar; the values that tuning settles on are
   * baked into the engine's defaults.
   *
   * @returns false when the engine refused the list (a wrong count, or a value
   *   that is not a number), leaving the lights as they were.
   */
  setLighting(values: readonly number[]): boolean {
    this.assertUsable();
    const pointer = this.module._thorvg_rubiks_lighting_buffer(values.length);
    if (pointer === 0) return false;
    if (!this.heapRegionUsable(pointer, values.length * 4)) {
      throw new Error(
        `Engine returned an invalid lighting buffer for ${values.length} ` +
          `values: pointer ${pointer}.`,
      );
    }
    new Float32Array(this.module.HEAPU8.buffer, pointer, values.length).set(
      values,
    );
    return this.module._thorvg_rubiks_set_lighting(values.length) !== 0;
  }

  /**
   * The lights the 3D view is drawn under, in the order setLighting() takes.
   *
   * Read from the engine so the page shows what the cube is actually lit by
   * -- the defaults, or the list the address bar handed over -- rather than
   * a copy of the defaults kept on this side.
   */
  lighting(): number[] {
    this.assertUsable();
    const count = this.module._thorvg_rubiks_lighting_count();
    if (!Number.isSafeInteger(count) || count <= 0) {
      throw new Error(`Engine returned an invalid lighting count ${count}.`);
    }
    const pointer = this.module._thorvg_rubiks_lighting_values(count);
    if (!this.heapRegionUsable(pointer, count * 4)) {
      throw new Error(
        `Engine returned an invalid lighting buffer for ${count} values: ` +
          `pointer ${pointer}.`,
      );
    }
    return Array.from(
      new Float32Array(this.module.HEAPU8.buffer, pointer, count),
    );
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
   * Opens a draft of the cube as it stands, for somebody to colour.
   *
   * Refused rather than thrown on, because both refusals are ordinary: a
   * sequence is playing, or a draft is already open. Neither is a fault the
   * page should stop for.
   */
  beginPainting(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_paint_begin() !== 0;
  }

  /** Throws the draft away; the cube was never touched. */
  cancelPainting(): void {
    this.assertUsable();
    this.module._thorvg_rubiks_paint_cancel();
  }

  isPainting(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_painting() !== 0;
  }

  /** Chooses the colour a press lays down. Throws only for a colour that is not one. */
  setBrush(colour: CubeStickerColour): void {
    this.assertUsable();
    if (this.module._thorvg_rubiks_set_paint_brush(colour) === 0) {
      throw new Error(`Engine rejected brush colour ${colour}.`);
    }
  }

  brush(): CubeStickerColour {
    this.assertUsable();
    const colour = this.module._thorvg_rubiks_paint_brush();
    if (!STICKER_COLOURS.includes(colour as CubeStickerColour)) {
      throw new Error(`Engine returned an invalid brush colour ${colour}.`);
    }
    return colour as CubeStickerColour;
  }

  /**
   * The colouring this session began from, or an empty list.
   *
   * Its emptiness is the question "does a link for this need the colours"
   * answered, so a caller asks this rather than remembering what it did.
   */
  originPainting(): number[] {
    this.assertUsable();
    const count = this.module._thorvg_rubiks_origin_painting_count();
    const colours: number[] = [];
    for (let index = 0; index < count; index += 1) {
      const colour = this.module._thorvg_rubiks_origin_painting_at(index);
      if (colour < 0) return [];
      colours.push(colour);
    }
    return colours;
  }

  /**
   * Puts a painted session on the cube, all at once.
   *
   * Two buffers where the other restore takes one, because a painted session
   * is a colouring and a tail of moves and neither is the other. Each is asked
   * for and filled in turn, and the heap views are taken freshly after each
   * call: asking the engine for memory can grow it, which leaves any view made
   * before the ask pointing at nothing.
   */
  restorePainting(
    size: number,
    painting: readonly number[],
    user: readonly number[],
  ): boolean {
    this.assertUsable();

    if (painting.length !== 6 * size * size) return false;
    if (user.length > MAX_SHARED_MOVES) return false;

    const colours = this.module._thorvg_rubiks_painting_buffer(painting.length);
    if (colours === 0) return false;
    if (!this.heapRegionUsable(colours, painting.length)) {
      throw new Error(
        `Engine returned an invalid painting buffer for ${painting.length} ` +
          `stickers: pointer ${colours}.`,
      );
    }
    new Uint8Array(this.module.HEAPU8.buffer, colours, painting.length).set(
      painting,
    );

    // The record's buffer is asked for second and written second, so that the
    // ask that could move the heap happens before the view that reads it.
    if (user.length > 0) {
      const words = this.module._thorvg_rubiks_restore_buffer(user.length);
      if (words === 0) return false;
      if (!this.heapRegionUsable(words, user.length * 4)) {
        throw new Error(
          `Engine returned an invalid restore buffer for ${user.length} ` +
            `moves: pointer ${words}.`,
        );
      }
      new Uint32Array(this.module.HEAPU8.buffer, words, user.length).set(user);
    }

    return (
      this.module._thorvg_rubiks_restore_painting(size, user.length) !== 0
    );
  }

  /** Whether a press covers the whole face it lands on. */
  setFilling(wholeFace: boolean): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_set_paint_filling(wholeFace ? 1 : 0) !== 0;
  }

  /**
   * Moves the keyboard's place on the net a cell at a time, across the whole
   * cross; the first call puts it in the middle of the front face.
   *
   * @returns false without a draft, and for a step into an empty corner of
   *          the cross or off the net, which leaves the place where it was.
   */
  paintCursorStep(columns: number, rows: number): boolean {
    this.assertUsable();
    if (!Number.isInteger(columns) || !Number.isInteger(rows)) return false;
    return this.module._thorvg_rubiks_paint_cursor_step(columns, rows) !== 0;
  }

  /** Lays the brush at the place: the cell, or its whole face while filling. */
  paintAtCursor(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_paint_at_cursor() !== 0;
  }

  /** Whether the place is drawn, which it is while the net has the keyboard. */
  setPaintCursorShown(shown: boolean): void {
    this.assertUsable();
    this.module._thorvg_rubiks_set_paint_cursor_shown(shown ? 1 : 0);
  }

  /** Where the place is and what colour is there, or null before it is put down. */
  paintCursor(): PaintCursor | null {
    this.assertUsable();
    const packed = this.module._thorvg_rubiks_paint_cursor();
    if (packed === -1) return null;

    const face = Math.floor(packed / 0x1000000);
    const colour = Math.floor(packed / 0x10000) % 0x100;
    const row = Math.floor(packed / 0x100) % 0x100;
    const col = packed % 0x100;
    if (
      !Number.isInteger(packed) ||
      packed < 0 ||
      !Object.values(CubeFace).includes(face as CubeFace) ||
      !STICKER_COLOURS.includes(colour as CubeStickerColour)
    ) {
      throw new Error(`Engine returned an invalid paint cursor ${packed}.`);
    }
    return {
      face: face as CubeFace,
      colour: colour as CubeStickerColour,
      row,
      col,
    };
  }

  isFilling(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_is_paint_filling() !== 0;
  }

  /** How many squares of the draft carry a colour, against the N^2 it needs. */
  paintedCount(colour: CubeStickerColour): number {
    this.assertUsable();
    return this.module._thorvg_rubiks_painted_count(colour);
  }

  /**
   * Makes the draft the cube, if it is one.
   *
   * False is the ordinary answer for a colouring that is not a cube, and the
   * draft is still open afterwards with `paintFault()` saying what to mend.
   */
  applyPainting(): boolean {
    this.assertUsable();
    return this.module._thorvg_rubiks_paint_apply() !== 0;
  }

  /** What the last refusal was, or `None`. */
  paintFault(): CubePaintFault {
    this.assertUsable();
    const fault = this.module._thorvg_rubiks_paint_fault();
    if (!Object.values(CubePaintFault).includes(fault as CubePaintFault)) {
      throw new Error(`Engine returned an unknown paint fault ${fault}.`);
    }
    return fault as CubePaintFault;
  }

  /**
   * The squares the last refusal blames, as `surface_stickers()` numbers them.
   *
   * Read one at a time across the boundary and gathered here, because the
   * boundary carries primitives only and the list is short enough that a
   * shared buffer would be more machinery than it saves.
   */
  paintBlamed(): number[] {
    this.assertUsable();
    const count = this.module._thorvg_rubiks_paint_blamed_count();
    const blamed: number[] = [];
    for (let index = 0; index < count; index += 1) {
      const slot = this.module._thorvg_rubiks_paint_blamed_at(index);
      if (slot >= 0) blamed.push(slot);
    }
    return blamed;
  }

  /**
   * Shuts the engine down and drops the borrowed pixel buffer views.
   *
   * Safe to call more than once; other methods fail afterwards.
   */
  dispose(): void {
    if (this.disposed) return;

    this.disposed = true;
    this.presentations.clear();
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
   * Takes a surface's new size, and the pixel buffer that came with it.
   *
   * The size is committed first, since the engine already holds it; the
   * buffer only when the full metadata contract holds -- a usable heap
   * region, and the exact width * height * 4 byte length on top of it. A
   * region can be perfectly usable and still be the wrong size for its
   * canvas, which is why the length is checked as well.
   *
   * @throws Error when the engine hands back a buffer that is not there.
   */
  private adopt(presentation: Presentation, size: CubeEngineSize | null): void {
    presentation.width = size?.width ?? 0;
    presentation.height = size?.height ?? 0;
    presentation.pointer = 0;
    presentation.byteLength = 0;
    presentation.view = null;
    presentation.image = null;
    presentation.frame = -1;
    if (size === null) return;

    presentation.canvas.width = size.width;
    presentation.canvas.height = size.height;

    const { id } = presentation;
    const pointer = this.module._thorvg_rubiks_surface_pixel_buffer(id);
    const length = this.module._thorvg_rubiks_surface_pixel_byte_length(id);
    if (
      length !== size.width * size.height * 4 ||
      !this.heapRegionUsable(pointer, length)
    ) {
      throw new Error(
        `Engine returned an invalid pixel buffer for surface ${id} at ` +
          `${size.width}x${size.height}: pointer ${pointer}, ` +
          `byte length ${length}.`,
      );
    }
    presentation.pointer = pointer;
    presentation.byteLength = length;
  }

  /**
   * An ImageData view over a surface's pixel buffer in the WASM heap.
   *
   * The view is reused while the buffer stays valid and is recreated after
   * memory growth detaches the previous ArrayBuffer.
   */
  private imageOf(presentation: Presentation): ImageData {
    const heapBuffer = this.module.HEAPU8.buffer;
    const stale =
      presentation.view === null ||
      presentation.image === null ||
      presentation.view.buffer !== heapBuffer ||
      presentation.view.buffer.byteLength === 0;

    if (stale) {
      presentation.view = new Uint8ClampedArray(
        heapBuffer,
        presentation.pointer,
        presentation.byteLength,
      );
      presentation.image = new ImageData(
        presentation.view,
        presentation.width,
        presentation.height,
      );
    }

    return presentation.image as ImageData;
  }
}
