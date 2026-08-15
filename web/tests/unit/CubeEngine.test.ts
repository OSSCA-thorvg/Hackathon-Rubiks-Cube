import { describe, expect, it, vi } from 'vitest';

import {
  computeDrawingBufferSize,
  CubeFace,
  CubeEngine,
  CubeFlatStyle,
  CubePalette,
  CubeViewMode,
  MAX_DIMENSION,
  MAX_SCRAMBLE_MOVES,
} from '../../src/wasm/CubeEngine.ts';
import type { ThorvgRubiksModule } from '../../src/wasm/generated/thorvg-rubiks.js';

/**
 * Fake Emscripten module implementing the C ABI contract in JavaScript.
 * Behavior flags let each test trigger a specific failure branch.
 */
function createFakeModule() {
  let buffer = new ArrayBuffer(1 << 20);
  let heap = new Uint8Array(buffer);
  let pointer = 4096;
  let width = 0;
  let height = 0;
  let initialized = false;

  const behavior = {
    initializeResult: 1,
    resizeResult: 1,
    renderResult: 1,
    pointerDownResult: 1,
    advanceResult: 0,
    scrambleResult: 1,
    solvedResult: 1,
    moveCount: 0,
    turnFaceResult: 1,
    setViewModeResult: 1,
    viewMode: CubeViewMode.Both,
    flatStyle: CubeFlatStyle.Net,
    setFlatStyleResult: 1,
    palette: CubePalette.Classic,
    setPaletteResult: 1,
    speedScale: 1,
    setSpeedScaleResult: 1,
    busyResult: 0,
    ambientStartResult: 1,
    ambientResult: 0,
    undoResult: 1,
    redoResult: 1,
    solveRewindResult: 1,
    timelineLength: 0,
    timelineCursor: 0,
    timelineScrambleEnd: 0,
    // The record itself, so a packed move can be read by index the way the
    // engine hands them over.
    timelineMoves: [] as number[],
    timelineMoveOverride: null as number | null,
    pixelBufferOverride: null as number | null,
    pixelByteLengthOverride: null as number | null,
  };

  const module = {
    get HEAPU8(): Uint8Array<ArrayBuffer> {
      return heap;
    },
    _thorvg_rubiks_initialize: vi.fn((w: number, h: number): number => {
      if (behavior.initializeResult === 0) return 0;
      initialized = true;
      width = w;
      height = h;
      return 1;
    }),
    _thorvg_rubiks_resize: vi.fn((w: number, h: number): number => {
      if (behavior.resizeResult === 0) return 0;
      width = w;
      height = h;
      // A real resize replaces the buffer, so the pointer moves.
      pointer += 1024;
      return 1;
    }),
    _thorvg_rubiks_render: vi.fn((): number => behavior.renderResult),
    _thorvg_rubiks_pixel_buffer: vi.fn((): number => {
      if (behavior.pixelBufferOverride !== null) {
        return behavior.pixelBufferOverride;
      }
      return initialized ? pointer : 0;
    }),
    _thorvg_rubiks_pixel_byte_length: vi.fn((): number => {
      if (behavior.pixelByteLengthOverride !== null) {
        return behavior.pixelByteLengthOverride;
      }
      return initialized ? width * height * 4 : 0;
    }),
    _thorvg_rubiks_shutdown: vi.fn((): void => {
      initialized = false;
    }),
    _thorvg_rubiks_pointer_down: vi.fn(
      (): number => behavior.pointerDownResult,
    ),
    _thorvg_rubiks_pointer_move: vi.fn((): void => {}),
    _thorvg_rubiks_pointer_up: vi.fn((): void => {}),
    _thorvg_rubiks_pointer_cancel: vi.fn((): void => {}),
    _thorvg_rubiks_advance: vi.fn((): number => behavior.advanceResult),
    _thorvg_rubiks_scramble: vi.fn((): number => behavior.scrambleResult),
    _thorvg_rubiks_reset_cube: vi.fn((): void => {}),
    _thorvg_rubiks_ambient_start: vi.fn(
      (): number => behavior.ambientStartResult,
    ),
    _thorvg_rubiks_ambient_stop: vi.fn((): void => {}),
    _thorvg_rubiks_is_ambient: vi.fn((): number => behavior.ambientResult),
    _thorvg_rubiks_is_solved: vi.fn((): number => behavior.solvedResult),
    _thorvg_rubiks_committed_move_count: vi.fn(
      (): number => behavior.moveCount,
    ),
    _thorvg_rubiks_undo: vi.fn((): number => behavior.undoResult),
    _thorvg_rubiks_redo: vi.fn((): number => behavior.redoResult),
    _thorvg_rubiks_solve_rewind: vi.fn(
      (): number => behavior.solveRewindResult,
    ),
    _thorvg_rubiks_stop_playback: vi.fn((): void => {}),
    _thorvg_rubiks_timeline_length: vi.fn(
      (): number => behavior.timelineLength,
    ),
    _thorvg_rubiks_timeline_cursor: vi.fn(
      (): number => behavior.timelineCursor,
    ),
    _thorvg_rubiks_timeline_scramble_end: vi.fn(
      (): number => behavior.timelineScrambleEnd,
    ),
    _thorvg_rubiks_timeline_move: vi.fn((index: number): number => {
      if (behavior.timelineMoveOverride !== null) {
        return behavior.timelineMoveOverride;
      }
      // Zero for an index the record does not hold, which is what the engine
      // answers rather than refusing the question.
      return behavior.timelineMoves[index] ?? 0;
    }),
    _thorvg_rubiks_turn_face: vi.fn((): number => behavior.turnFaceResult),
    _thorvg_rubiks_set_view_mode: vi.fn(
      (mode: number): number => {
        if (behavior.setViewModeResult !== 0) behavior.viewMode = mode;
        return behavior.setViewModeResult;
      },
    ),
    _thorvg_rubiks_view_mode: vi.fn((): number => behavior.viewMode),
    _thorvg_rubiks_set_flat_style: vi.fn((style: number): number => {
      if (behavior.setFlatStyleResult !== 0) behavior.flatStyle = style;
      return behavior.setFlatStyleResult;
    }),
    _thorvg_rubiks_flat_style: vi.fn((): number => behavior.flatStyle),
    _thorvg_rubiks_set_palette: vi.fn((palette: number): number => {
      if (behavior.setPaletteResult !== 0) behavior.palette = palette;
      return behavior.setPaletteResult;
    }),
    _thorvg_rubiks_palette: vi.fn((): number => behavior.palette),
    _thorvg_rubiks_set_speed_scale: vi.fn((scale: number): number => {
      // Clamped rather than refused, the way the engine does it.
      if (behavior.setSpeedScaleResult !== 0) {
        behavior.speedScale = Math.min(4, Math.max(0.25, scale));
      }
      return behavior.setSpeedScaleResult;
    }),
    _thorvg_rubiks_speed_scale: vi.fn((): number => behavior.speedScale),
    _thorvg_rubiks_reset_view: vi.fn((): void => {}),
    _thorvg_rubiks_is_busy: vi.fn((): number => behavior.busyResult),
  } satisfies ThorvgRubiksModule;

  /** Simulates WASM memory growth: the old ArrayBuffer is replaced. */
  const growMemory = (): void => {
    const next = new ArrayBuffer(buffer.byteLength * 2);
    new Uint8Array(next).set(heap);
    buffer = next;
    heap = new Uint8Array(next);
  };

  return { module, behavior, growMemory };
}

/** Canvas stub exposing only what CubeEngine touches. */
function createFakeCanvas() {
  const putImageData = vi.fn();
  const canvas = {
    width: 0,
    height: 0,
    clientWidth: 100,
    clientHeight: 50,
    getContext: vi.fn((id: string) => (id === '2d' ? { putImageData } : null)),
  };
  return { canvas: canvas as unknown as HTMLCanvasElement, putImageData };
}

async function createEngine() {
  const fake = createFakeModule();
  const surface = createFakeCanvas();
  const engine = await CubeEngine.create(surface.canvas, {
    loadModule: async () => fake.module,
  });
  return { engine, ...fake, ...surface };
}

describe('computeDrawingBufferSize', () => {
  it('rounds CSS size by the device pixel ratio', () => {
    expect(computeDrawingBufferSize(100, 50, 2)).toEqual({
      width: 200,
      height: 100,
    });
  });

  it('clamps to at least one pixel and at most the engine limit', () => {
    expect(computeDrawingBufferSize(0, 0, 1)).toEqual({ width: 1, height: 1 });
    expect(computeDrawingBufferSize(100000, 10, 1)).toEqual({
      width: MAX_DIMENSION,
      height: 10,
    });
  });

  it('normalizes non-finite inputs to one pixel', () => {
    expect(computeDrawingBufferSize(Number.NaN, 50, 1)).toEqual({
      width: 1,
      height: 50,
    });
    expect(
      computeDrawingBufferSize(100, 50, Number.POSITIVE_INFINITY),
    ).toEqual({ width: 1, height: 1 });
  });
});

describe('CubeEngine.create', () => {
  it('initializes with the drawing buffer size and sizes the canvas', async () => {
    const { engine, module, canvas } = await createEngine();

    expect(module._thorvg_rubiks_initialize).toHaveBeenCalledWith(100, 50);
    expect(canvas.width).toBe(100);
    expect(canvas.height).toBe(50);

    engine.dispose();
  });

  it('rejects when initialization fails, with nothing to clean up', async () => {
    const fake = createFakeModule();
    fake.behavior.initializeResult = 0;
    const { canvas } = createFakeCanvas();

    await expect(
      CubeEngine.create(canvas, { loadModule: async () => fake.module }),
    ).rejects.toThrow('initialization failed');
    expect(fake.module._thorvg_rubiks_shutdown).not.toHaveBeenCalled();
  });

  it('shuts the module down when construction fails after initialization', async () => {
    const fake = createFakeModule();
    fake.behavior.pixelBufferOverride = 0;
    const { canvas } = createFakeCanvas();

    await expect(
      CubeEngine.create(canvas, { loadModule: async () => fake.module }),
    ).rejects.toThrow('invalid pixel buffer');
    expect(fake.module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
  });

  it('rejects a byte length that does not match the drawing buffer size', async () => {
    const fake = createFakeModule();
    fake.behavior.pixelByteLengthOverride = 100 * 50 * 4 - 4;
    const { canvas } = createFakeCanvas();

    await expect(
      CubeEngine.create(canvas, { loadModule: async () => fake.module }),
    ).rejects.toThrow('invalid pixel buffer');
    expect(fake.module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
  });

  it('rejects unaligned, fractional, unsafe, and negative pointers', async () => {
    const invalidPointers = [4097, 4096.5, 2 ** 53, -4];
    for (const pointer of invalidPointers) {
      const fake = createFakeModule();
      fake.behavior.pixelBufferOverride = pointer;
      const { canvas } = createFakeCanvas();

      await expect(
        CubeEngine.create(canvas, { loadModule: async () => fake.module }),
      ).rejects.toThrow('invalid pixel buffer');
      expect(fake.module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
    }
  });

  it('rejects a buffer that does not fit inside the heap', async () => {
    const fake = createFakeModule();
    fake.behavior.pixelBufferOverride = fake.module.HEAPU8.byteLength - 4;
    const { canvas } = createFakeCanvas();

    await expect(
      CubeEngine.create(canvas, { loadModule: async () => fake.module }),
    ).rejects.toThrow('invalid pixel buffer');
    expect(fake.module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
  });
});

describe('CubeEngine.render', () => {
  it('presents the pixel buffer and reuses the ImageData while valid', async () => {
    const { engine, putImageData } = await createEngine();

    engine.render();
    engine.render();

    expect(putImageData).toHaveBeenCalledTimes(2);
    expect(putImageData.mock.calls[0][0]).toBe(putImageData.mock.calls[1][0]);

    engine.dispose();
  });

  it('recreates the view after WASM memory growth', async () => {
    const { engine, module, growMemory, putImageData } = await createEngine();

    engine.render();
    growMemory();
    engine.render();

    const [first] = putImageData.mock.calls[0] as [ImageData];
    const [second] = putImageData.mock.calls[1] as [ImageData];
    expect(second).not.toBe(first);
    expect(second.data.buffer).toBe(module.HEAPU8.buffer);

    engine.dispose();
  });

  it('translates a render failure into an Error', async () => {
    const { engine, behavior } = await createEngine();

    behavior.renderResult = 0;
    expect(() => engine.render()).toThrow('rendering failed');

    engine.dispose();
  });
});

describe('CubeEngine.resize', () => {
  it('re-queries the pixel source and resizes the canvas', async () => {
    const { engine, canvas, putImageData } = await createEngine();

    engine.render();
    engine.resize({ width: 40, height: 40 });
    engine.render();

    expect(canvas.width).toBe(40);
    expect(canvas.height).toBe(40);
    const [image] = putImageData.mock.calls[1] as [ImageData];
    expect(image.data.byteLength).toBe(40 * 40 * 4);

    engine.dispose();
  });

  it('skips the engine call when the size is unchanged', async () => {
    const { engine, module } = await createEngine();

    engine.resize({ width: 100, height: 50 });
    expect(module._thorvg_rubiks_resize).not.toHaveBeenCalled();

    engine.dispose();
  });

  it('translates a resize failure into an Error', async () => {
    const { engine, behavior } = await createEngine();

    behavior.resizeResult = 0;
    expect(() => engine.resize({ width: 40, height: 40 })).toThrow(
      'resize failed',
    );

    engine.dispose();
  });

  it('rejects invalid runtime sizes before reaching WASM', async () => {
    const { engine, module } = await createEngine();

    const invalidSizes = [
      { width: Number.NaN, height: 40 },
      { width: 40.5, height: 40 },
      { width: -1, height: 40 },
      { width: Number.POSITIVE_INFINITY, height: 40 },
      { width: MAX_DIMENSION + 1, height: 40 },
      { width: 40, height: 0 },
    ];
    for (const size of invalidSizes) {
      expect(() => engine.resize(size)).toThrow('Invalid drawing buffer size');
    }
    expect(module._thorvg_rubiks_resize).not.toHaveBeenCalled();

    engine.dispose();
  });

  it('disposes the engine when post-resize metadata is invalid', async () => {
    const { engine, module, behavior } = await createEngine();

    behavior.pixelByteLengthOverride = 40 * 40 * 4 - 4;
    expect(() => engine.resize({ width: 40, height: 40 })).toThrow(
      'invalid pixel buffer',
    );

    expect(module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
    expect(() => engine.render()).toThrow('disposed');
  });
});

describe('CubeEngine pointer and animation', () => {
  it('passes pointer events straight through', async () => {
    const { engine, module } = await createEngine();

    expect(engine.pointerDown(12, 34)).toBe(true);
    expect(module._thorvg_rubiks_pointer_down).toHaveBeenCalledWith(12, 34);

    engine.pointerMove(56, 78);
    expect(module._thorvg_rubiks_pointer_move).toHaveBeenCalledWith(56, 78);

    engine.pointerUp();
    expect(module._thorvg_rubiks_pointer_up).toHaveBeenCalledTimes(1);

    engine.pointerCancel();
    expect(module._thorvg_rubiks_pointer_cancel).toHaveBeenCalledTimes(1);
  });

  it('reports a missed press as false', async () => {
    const { engine, behavior } = await createEngine();

    behavior.pointerDownResult = 0;
    expect(engine.pointerDown(1, 2)).toBe(false);
  });

  it('turns the advance return code into whether frames remain', async () => {
    const { engine, module, behavior } = await createEngine();

    expect(engine.advance(16)).toBe(false);
    expect(module._thorvg_rubiks_advance).toHaveBeenCalledWith(16);

    behavior.advanceResult = 1;
    expect(engine.advance(16)).toBe(true);
  });
});

describe('CubeEngine gameplay and view controls', () => {
  it('passes scramble, reset, status, and camera commands through', async () => {
    const { engine, module, behavior } = await createEngine();

    engine.scramble(0x12345678, 20);
    expect(module._thorvg_rubiks_scramble).toHaveBeenCalledWith(
      0x12345678,
      20,
    );

    behavior.solvedResult = 0;
    behavior.moveCount = 7;
    behavior.busyResult = 1;
    expect(engine.isSolved()).toBe(false);
    expect(engine.committedMoveCount()).toBe(7);
    expect(engine.isBusy()).toBe(true);

    engine.resetCube();
    engine.resetView();
    expect(module._thorvg_rubiks_reset_cube).toHaveBeenCalledTimes(1);
    expect(module._thorvg_rubiks_reset_view).toHaveBeenCalledTimes(1);
  });

  it('validates scramble seeds and counts before crossing the C ABI', async () => {
    const { engine, module } = await createEngine();

    for (const seed of [-1, 1.5, Number.NaN, 0x1_0000_0000]) {
      expect(() => engine.scramble(seed, 20)).toThrow('Invalid scramble seed');
    }
    for (const count of [0, -1, 2.5, Number.NaN, MAX_SCRAMBLE_MOVES + 1]) {
      expect(() => engine.scramble(1, count)).toThrow(
        'Invalid scramble move count',
      );
    }
    expect(module._thorvg_rubiks_scramble).not.toHaveBeenCalled();
  });

  it('starts face turns and reports busy rejection', async () => {
    const { engine, module, behavior } = await createEngine();

    expect(engine.turnFace(CubeFace.Right, 1)).toBe(true);
    expect(module._thorvg_rubiks_turn_face).toHaveBeenCalledWith(
      CubeFace.Right,
      1,
    );

    behavior.turnFaceResult = 0;
    expect(engine.turnFace(CubeFace.Up, -1)).toBe(false);
  });

  it('sets and validates the current view mode', async () => {
    const { engine, behavior } = await createEngine();

    expect(engine.viewMode()).toBe(CubeViewMode.Both);
    engine.setViewMode(CubeViewMode.Flat);
    expect(engine.viewMode()).toBe(CubeViewMode.Flat);

    // The flat style is its own axis, and its own validation.
    expect(engine.flatStyle()).toBe(CubeFlatStyle.Net);
    engine.setFlatStyle(CubeFlatStyle.Rings);
    expect(engine.flatStyle()).toBe(CubeFlatStyle.Rings);
    engine.setFlatStyle(CubeFlatStyle.Both);
    expect(engine.flatStyle()).toBe(CubeFlatStyle.Both);
    behavior.flatStyle = 3;
    expect(() => engine.flatStyle()).toThrow('invalid flat style');
    behavior.flatStyle = CubeFlatStyle.Net;

    // The palette is a third axis, validated the same way in both directions.
    expect(engine.palette()).toBe(CubePalette.Classic);
    engine.setPalette(CubePalette.HighContrast);
    expect(engine.palette()).toBe(CubePalette.HighContrast);
    behavior.palette = 2;
    expect(() => engine.palette()).toThrow('invalid palette');
    behavior.palette = CubePalette.Classic;
    behavior.setPaletteResult = 0;
    expect(() => engine.setPalette(CubePalette.HighContrast)).toThrow(
      'rejected palette',
    );
    behavior.setPaletteResult = 1;

    // The speed comes back clamped rather than echoed, and a value that is
    // not a number is the one thing the engine refuses outright.
    expect(engine.speedScale()).toBe(1);
    engine.setSpeedScale(2);
    expect(engine.speedScale()).toBe(2);
    engine.setSpeedScale(99);
    expect(engine.speedScale()).toBe(4);
    behavior.setSpeedScaleResult = 0;
    expect(() => engine.setSpeedScale(Number.NaN)).toThrow(
      'rejected speed scale',
    );
    behavior.setSpeedScaleResult = 1;
    behavior.speedScale = 0;
    expect(() => engine.speedScale()).toThrow('invalid speed scale');
    behavior.speedScale = 1;

    // The value that used to be a fourth mode is not one.
    behavior.viewMode = 3;
    expect(() => engine.viewMode()).toThrow('invalid view mode');

    behavior.viewMode = 99;
    expect(() => engine.viewMode()).toThrow('invalid view mode');
    behavior.setViewModeResult = 0;
    expect(() => engine.setViewMode(CubeViewMode.Cube3D)).toThrow(
      'rejected view mode',
    );
  });

  it('rejects invalid native query values', async () => {
    const { engine, behavior } = await createEngine();

    behavior.moveCount = -1;
    expect(() => engine.committedMoveCount()).toThrow('invalid move count');

    // Every count comes back as a uint32, so all of them are refused the same
    // way: anything the i32 boundary would have reinterpreted arrives here as
    // a negative or fractional number rather than as a length.
    behavior.timelineLength = -1;
    expect(() => engine.timelineLength()).toThrow('invalid timeline length');
    behavior.timelineCursor = 1.5;
    expect(() => engine.timelineCursor()).toThrow('invalid timeline cursor');
    behavior.timelineScrambleEnd = Number.NaN;
    expect(() => engine.timelineScrambleEnd()).toThrow(
      'invalid timeline scramble end',
    );
  });

  it('carries the rewinds across and reports each refusal', async () => {
    const { engine, module, behavior } = await createEngine();

    expect(engine.undo()).toBe(true);
    expect(engine.redo()).toBe(true);
    expect(engine.solveRewind()).toBe(true);
    expect(module._thorvg_rubiks_undo).toHaveBeenCalledTimes(1);
    expect(module._thorvg_rubiks_solve_rewind).toHaveBeenCalledTimes(1);

    // Refused rather than thrown: having nothing to rewind, or something else
    // owning the cube, is an answer rather than a failure.
    behavior.undoResult = 0;
    behavior.redoResult = 0;
    behavior.solveRewindResult = 0;
    expect(engine.undo()).toBe(false);
    expect(engine.redo()).toBe(false);
    expect(engine.solveRewind()).toBe(false);

    engine.stopPlayback();
    expect(module._thorvg_rubiks_stop_playback).toHaveBeenCalledTimes(1);
  });

  it('reads the record as three counts', async () => {
    const { engine, behavior } = await createEngine();

    expect(engine.timelineLength()).toBe(0);
    expect(engine.timelineCursor()).toBe(0);
    expect(engine.timelineScrambleEnd()).toBe(0);

    behavior.timelineLength = 22;
    behavior.timelineCursor = 21;
    behavior.timelineScrambleEnd = 20;
    expect(engine.timelineLength()).toBe(22);
    expect(engine.timelineCursor()).toBe(21);
    expect(engine.timelineScrambleEnd()).toBe(20);
  });

  it('reads a packed move back by index', async () => {
    const { engine, behavior } = await createEngine();

    // Nothing recorded, so nothing at any index.
    expect(engine.timelineMove(0)).toBe(0);

    behavior.timelineMoves = [0x44, 0x10];
    expect(engine.timelineMove(0)).toBe(0x44);
    expect(engine.timelineMove(1)).toBe(0x10);
    expect(engine.timelineMove(2)).toBe(0);

    // The top bit of the mask is a layer like any other, so a word the i32
    // boundary hands back as a negative number is read unsigned rather than
    // refused the way a count would be.
    behavior.timelineMoveOverride = -2147483644;
    expect(engine.timelineMove(0)).toBe(0x8000_0004);

    behavior.timelineMoveOverride = 1.5;
    expect(() => engine.timelineMove(0)).toThrow('invalid packed move');

    // An index that could not have come from a length is stopped on this side
    // rather than being reinterpreted on the other.
    behavior.timelineMoveOverride = null;
    expect(() => engine.timelineMove(-1)).toThrow('Invalid timeline index');
    expect(() => engine.timelineMove(1.5)).toThrow('Invalid timeline index');
  });

  it('carries a watching choice across and reports a refused start', async () => {
    const { engine, module, behavior } = await createEngine();

    expect(engine.isAmbient()).toBe(false);
    expect(engine.ambientStart(0xffffffff)).toBe(true);
    expect(module._thorvg_rubiks_ambient_start).toHaveBeenCalledWith(
      0xffffffff,
    );

    // Refused rather than thrown: the only reason a valid choice is turned
    // down is that watching had already begun, which is an answer.
    behavior.ambientStartResult = 0;
    expect(engine.ambientStart(0)).toBe(false);

    behavior.ambientResult = 1;
    expect(engine.isAmbient()).toBe(true);
    engine.ambientStop();
    expect(module._thorvg_rubiks_ambient_stop).toHaveBeenCalledTimes(1);
  });

  it('validates a watching choice before crossing the C ABI', async () => {
    const { engine, module } = await createEngine();

    for (const choice of [-1, 1.5, Number.NaN, 0x1_0000_0000]) {
      expect(() => engine.ambientStart(choice)).toThrow(
        'Invalid ambient choice',
      );
    }
    expect(module._thorvg_rubiks_ambient_start).not.toHaveBeenCalled();
  });
});

describe('CubeEngine.dispose', () => {
  it('is idempotent and rejects later calls', async () => {
    const { engine, module } = await createEngine();

    engine.dispose();
    engine.dispose();

    expect(module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
    expect(() => engine.render()).toThrow('disposed');
    expect(() => engine.resize({ width: 40, height: 40 })).toThrow('disposed');
    expect(() => engine.pointerDown(1, 2)).toThrow('disposed');
    expect(() => engine.advance(16)).toThrow('disposed');
  });
});
