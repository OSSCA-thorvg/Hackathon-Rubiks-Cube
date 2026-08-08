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
  const canvasListeners = new Map<string, Set<(event: Event) => void>>();

  const canvasState = {
    width: 100,
    height: 50,
    clientWidth: 100,
    clientHeight: 50,
    addEventListener: vi.fn((type: string, listener: (event: Event) => void) => {
      const bucket = canvasListeners.get(type) ?? new Set();
      bucket.add(listener);
      canvasListeners.set(type, bucket);
    }),
    removeEventListener: vi.fn(
      (type: string, listener: (event: Event) => void) => {
        canvasListeners.get(type)?.delete(listener);
      },
    ),
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 100, height: 50 }),
    hasPointerCapture: vi.fn(() => true),
    setPointerCapture: vi.fn(),
    releasePointerCapture: vi.fn(),
  };
  const canvas = canvasState as unknown as HTMLCanvasElement;

  const engine = {
    resize: vi.fn((size: { width: number; height: number }) => {
      canvasState.width = size.width;
      canvasState.height = size.height;
    }),
    render: vi.fn(),
    dispose: vi.fn(),
    advance: vi.fn(() => false),
    pointerDown: vi.fn(() => true),
    pointerMove: vi.fn(),
    pointerUp: vi.fn(),
    pointerCancel: vi.fn(),
  } satisfies EngineLike;

  // A hand-cranked animation frame queue, so tests decide when frames run.
  let pendingFrame: ((timestamp: number) => void) | null = null;
  let nextFrameHandle = 0;
  const requestFrame = vi.fn((callback: (timestamp: number) => void) => {
    pendingFrame = callback;
    return ++nextFrameHandle;
  });
  const cancelFrame = vi.fn(() => {
    pendingFrame = null;
  });

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
      requestFrame,
      cancelFrame,
    });

  const dispatch = (type: string, event: Event): void => {
    for (const listener of listeners.get(type) ?? []) listener(event);
  };

  const listenerCount = (type: string): number =>
    listeners.get(type)?.size ?? 0;

  const dispatchPointer = (type: string, event: Partial<PointerEvent>): void => {
    const full = {
      pointerId: 1,
      isPrimary: true,
      button: 0,
      clientX: 10,
      clientY: 10,
      ...event,
    } as PointerEvent;
    for (const listener of canvasListeners.get(type) ?? []) {
      listener(full as unknown as Event);
    }
  };

  const runFrame = (timestamp: number): void => {
    const callback = pendingFrame;
    pendingFrame = null;
    callback?.(timestamp);
  };

  return {
    start,
    canvasState,
    engine,
    observer,
    triggerObserver: () => observerCallback(),
    dispatch,
    listenerCount,
    dispatchPointer,
    runFrame,
    hasPendingFrame: () => pendingFrame !== null,
    requestFrame,
    cancelFrame,
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

  it('runs no frames while the cube is at rest', async () => {
    const harness = createHarness();
    await harness.start();

    // A still cube costs nothing: no loop is scheduled just by being ready.
    expect(harness.requestFrame).not.toHaveBeenCalled();
    expect(harness.engine.advance).not.toHaveBeenCalled();
  });

  it('starts a frame loop when a press begins a gesture', async () => {
    const harness = createHarness();
    await harness.start();

    harness.dispatchPointer('pointerdown', {});

    expect(harness.engine.pointerDown).toHaveBeenCalledTimes(1);
    expect(harness.hasPendingFrame()).toBe(true);
  });

  it('leaves the page alone when the engine declines the press', async () => {
    const harness = createHarness();
    harness.engine.pointerDown.mockReturnValueOnce(false);
    await harness.start();

    harness.dispatchPointer('pointerdown', {});

    expect(harness.canvasState.setPointerCapture).not.toHaveBeenCalled();
    expect(harness.hasPendingFrame()).toBe(false);
  });

  it('keeps drawing while the engine asks for frames and then stops', async () => {
    const harness = createHarness();
    await harness.start();

    harness.engine.advance.mockReturnValueOnce(true);
    harness.dispatchPointer('pointerdown', {});

    // The first frame of a run measures no elapsed time.
    harness.runFrame(1000);
    expect(harness.engine.advance).toHaveBeenLastCalledWith(0);
    expect(harness.hasPendingFrame()).toBe(true);

    harness.runFrame(1016);
    expect(harness.engine.advance).toHaveBeenLastCalledWith(16);

    // advance() returned false, so this was the last frame drawn.
    expect(harness.hasPendingFrame()).toBe(false);
    expect(harness.engine.render).toHaveBeenCalledTimes(3);
  });

  it('tears down and reports when a frame fails', async () => {
    const harness = createHarness();
    await harness.start();

    harness.engine.advance.mockImplementationOnce(() => {
      throw new Error('advance failed');
    });
    harness.dispatchPointer('pointerdown', {});
    harness.runFrame(1000);

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.onError).toHaveBeenCalledTimes(1);
    expect(harness.hasPendingFrame()).toBe(false);
  });

  it('cancels a pending frame and the gesture on teardown', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    harness.engine.advance.mockReturnValue(true);
    harness.dispatchPointer('pointerdown', {});
    expect(harness.hasPendingFrame()).toBe(true);

    controller.teardown();

    expect(harness.cancelFrame).toHaveBeenCalledTimes(1);
    // A gesture cut short by teardown must not commit a turn.
    expect(harness.engine.pointerCancel).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerUp).not.toHaveBeenCalled();
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
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
