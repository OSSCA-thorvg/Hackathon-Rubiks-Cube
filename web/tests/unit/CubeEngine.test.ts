import { describe, expect, it, vi } from 'vitest';

import {
  computeDrawingBufferSize,
  CubeEngine,
  MAX_DIMENSION,
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

describe('CubeEngine.dispose', () => {
  it('is idempotent and rejects later calls', async () => {
    const { engine, module } = await createEngine();

    engine.dispose();
    engine.dispose();

    expect(module._thorvg_rubiks_shutdown).toHaveBeenCalledTimes(1);
    expect(() => engine.render()).toThrow('disposed');
    expect(() => engine.resize({ width: 40, height: 40 })).toThrow('disposed');
  });
});
