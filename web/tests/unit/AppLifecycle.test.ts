import { describe, expect, it, vi } from 'vitest';

import {
  startApp,
  type EngineLike,
  type ObserverLike,
} from '../../src/AppLifecycle.ts';

/**
 * Builds a lifecycle harness with a mock engine, observer, and window.
 * The fake engine mirrors CubeEngine by committing the canvas size on a
 * successful resize.
 */
function createHarness(overrides: {
  createObserver?: (callback: () => void) => ObserverLike;
} = {}) {
  const canvasState = {
    width: 100,
    height: 50,
    clientWidth: 100,
    clientHeight: 50,
  };
  const canvas = canvasState as unknown as HTMLCanvasElement;

  const engine = {
    resize: vi.fn((size: { width: number; height: number }) => {
      canvasState.width = size.width;
      canvasState.height = size.height;
    }),
    render: vi.fn(),
    dispose: vi.fn(),
  } satisfies EngineLike;

  let observerCallback: () => void = () => {};
  const observer = { observe: vi.fn(), disconnect: vi.fn() };

  const listeners = new Map<string, Set<(event: Event) => void>>();
  const targetWindow = {
    devicePixelRatio: 1,
    addEventListener: vi.fn((type: string, listener: (event: Event) => void) => {
      const bucket = listeners.get(type) ?? new Set();
      bucket.add(listener);
      listeners.set(type, bucket);
    }),
    removeEventListener: vi.fn(
      (type: string, listener: (event: Event) => void) => {
        listeners.get(type)?.delete(listener);
      },
    ),
  };

  const states: Array<[string, string]> = [];
  const onError = vi.fn();

  const start = () =>
    startApp({
      canvas,
      setState: (state, message) => states.push([state, message]),
      onError,
      createEngine: async () => engine,
      createObserver:
        overrides.createObserver ??
        ((callback) => {
          observerCallback = callback;
          return observer;
        }),
      targetWindow,
    });

  const dispatch = (type: string, event: Event): void => {
    for (const listener of listeners.get(type) ?? []) listener(event);
  };

  const listenerCount = (type: string): number =>
    listeners.get(type)?.size ?? 0;

  return {
    start,
    canvasState,
    engine,
    observer,
    triggerObserver: () => observerCallback(),
    dispatch,
    listenerCount,
    states,
    onError,
  };
}

describe('startApp', () => {
  it('renders once, reaches ready, and wires the listeners', async () => {
    const harness = createHarness();

    await harness.start();

    expect(harness.engine.render).toHaveBeenCalledTimes(1);
    expect(harness.states.at(-1)?.[0]).toBe('ready');
    expect(harness.observer.observe).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(1);
    expect(harness.listenerCount('pagehide')).toBe(1);
    expect(harness.listenerCount('pageshow')).toBe(1);
  });

  it('disposes the engine and rejects when the initial render fails', async () => {
    const harness = createHarness();
    harness.engine.render.mockImplementationOnce(() => {
      throw new Error('render failed');
    });

    await expect(harness.start()).rejects.toThrow('render failed');
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.states.at(-1)?.[0]).toBe('loading');
  });

  it('resizes and re-renders when the observed size changes', async () => {
    const harness = createHarness();
    await harness.start();

    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();

    expect(harness.engine.resize).toHaveBeenCalledWith({
      width: 60,
      height: 50,
    });
    expect(harness.engine.render).toHaveBeenCalledTimes(2);
  });

  it('skips the engine when the drawing buffer size is unchanged', async () => {
    const harness = createHarness();
    await harness.start();

    harness.triggerObserver();

    expect(harness.engine.resize).not.toHaveBeenCalled();
    expect(harness.engine.render).toHaveBeenCalledTimes(1);
  });

  it('tears everything down and reports when a resize fails', async () => {
    const harness = createHarness();
    await harness.start();

    harness.engine.resize.mockImplementationOnce(() => {
      throw new Error('resize failed');
    });
    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.observer.disconnect).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
    expect(harness.listenerCount('pagehide')).toBe(0);
    expect(harness.listenerCount('pageshow')).toBe(0);
    expect(harness.onError).toHaveBeenCalledTimes(1);
  });

  it('disposes on a real page unload', async () => {
    const harness = createHarness();
    await harness.start();

    harness.dispatch('pagehide', { persisted: false } as unknown as Event);

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
  });

  it('survives a BFCache round trip and re-applies the size on restore', async () => {
    const harness = createHarness();
    await harness.start();

    harness.dispatch('pagehide', { persisted: true } as unknown as Event);
    expect(harness.engine.dispose).not.toHaveBeenCalled();

    // The viewport changed while the page sat in the cache.
    harness.canvasState.clientWidth = 60;
    harness.dispatch('pageshow', { persisted: true } as unknown as Event);

    expect(harness.engine.resize).toHaveBeenCalledWith({
      width: 60,
      height: 50,
    });
    expect(harness.engine.render).toHaveBeenCalledTimes(2);
  });

  it('releases everything through the returned controller', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    controller.teardown();

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.observer.disconnect).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
  });

  it('tears down when the render after a successful resize fails', async () => {
    const harness = createHarness();
    await harness.start();

    harness.engine.render.mockImplementationOnce(() => {
      throw new Error('second render failed');
    });
    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();

    expect(harness.engine.resize).toHaveBeenCalledTimes(1);
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.onError).toHaveBeenCalledTimes(1);
  });

  it('unwinds the engine when observer creation fails', async () => {
    const harness = createHarness({
      createObserver: () => {
        throw new Error('no observer');
      },
    });

    await expect(harness.start()).rejects.toThrow('no observer');
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
    expect(harness.listenerCount('pagehide')).toBe(0);
  });

  it('unwinds installed resources when observe() fails', async () => {
    const disconnect = vi.fn();
    const harness = createHarness({
      createObserver: () => ({
        observe: () => {
          throw new Error('observe failed');
        },
        disconnect,
      }),
    });

    await expect(harness.start()).rejects.toThrow('observe failed');
    expect(disconnect).toHaveBeenCalledTimes(1);
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
  });

  it('ignores observer callbacks that arrive after teardown', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    controller.teardown();
    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();

    expect(harness.engine.resize).not.toHaveBeenCalled();
    expect(harness.onError).not.toHaveBeenCalled();
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
  });

  it('finishes every release step and keeps the original error when disconnect throws', async () => {
    vi.spyOn(console, 'error').mockImplementation(() => {});
    let callback: () => void = () => {};
    const disconnect = vi.fn(() => {
      throw new Error('disconnect failed');
    });
    const harness = createHarness({
      createObserver: (cb) => {
        callback = cb;
        return { observe: vi.fn(), disconnect };
      },
    });
    await harness.start();

    harness.engine.resize.mockImplementationOnce(() => {
      throw new Error('resize failed');
    });
    harness.canvasState.clientWidth = 60;
    callback();

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
    expect(harness.listenerCount('pagehide')).toBe(0);
    expect(harness.onError).toHaveBeenCalledTimes(1);
    expect((harness.onError.mock.calls[0]![0] as Error).message).toBe(
      'resize failed',
    );
    expect(console.error).toHaveBeenCalledWith(
      expect.objectContaining({ message: 'disconnect failed' }),
    );
  });

  it('does not propagate a dispose failure out of teardown', async () => {
    const errorSpy = vi
      .spyOn(console, 'error')
      .mockImplementation(() => {});
    const harness = createHarness();
    harness.engine.dispose.mockImplementationOnce(() => {
      throw new Error('dispose failed');
    });
    const controller = await harness.start();

    expect(() => controller.teardown()).not.toThrow();
    expect(harness.observer.disconnect).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
    expect(errorSpy).toHaveBeenCalledWith(
      expect.objectContaining({ message: 'dispose failed' }),
    );
  });

  it('treats repeated teardown as a no-op', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    controller.teardown();
    controller.teardown();
    harness.dispatch('pagehide', { persisted: false } as unknown as Event);

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.observer.disconnect).toHaveBeenCalledTimes(1);
  });
});
