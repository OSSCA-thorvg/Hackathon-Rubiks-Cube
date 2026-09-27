import { describe, expect, it, vi } from 'vitest';

import {
  MAX_STAGE_PIXELS,
  stageDensity,
  startApp,
  type EngineLike,
  type ObserverLike,
  type StageView,
} from '../../src/AppLifecycle.ts';
import type {
  GameController,
  GameControllerOptions,
  GameUi,
} from '../../src/game/GameController.ts';
import { encodeSession } from '../../src/game/shareCode.ts';
import { CubeScene, CubeSurface } from '../../src/wasm/CubeEngine.ts';

/**
 * The gameplay DOM the lifecycle forwards. GameController owns what the
 * elements mean; here they only have to exist.
 */
function createGameUi(): GameUi {
  const button = (): HTMLButtonElement => document.createElement('button');
  return {
    root: document.createElement('main'),
    stage: document.createElement('section'),
    timer: document.createElement('output'),
    status: document.createElement('p'),
    scrambleButton: button(),
    scrambleMovesInput: document.createElement('input'),
    resetButton: button(),
    shareButton: button(),
    recordBest: document.createElement('p'),
    recordList: document.createElement('ol'),
    ambientButton: button(),
    homeViewButton: button(),
    viewButtons: [],
    flatButtons: [],
    moveButtons: [],
  };
}

/**
 * Builds a lifecycle harness with a mock engine, observer, and window.
 * The fake engine mirrors CubeEngine by committing the canvas size on a
 * successful resize.
 */
function createHarness(overrides: {
  createObserver?: (callback: () => void) => ObserverLike;
  createGameController?: (options: GameControllerOptions) => GameController;
  /** What the address bar holds when the page opens. */
  hash?: string;
  /** The canvases beside the cube's; none keeps every scene on it. */
  views?: readonly StageView[];
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
    getBoundingClientRect: () => ({
      left: 0,
      top: 0,
      width: canvasState.clientWidth,
      height: canvasState.clientHeight,
    }),
    hasPointerCapture: vi.fn(() => true),
    setPointerCapture: vi.fn(),
    releasePointerCapture: vi.fn(),
  };
  const canvas = canvasState as unknown as HTMLCanvasElement;
  const presented = new Map<number, HTMLCanvasElement>();
  // What the engine holds for each surface, the way CubeEngine keeps it:
  // surface 0 at the size it was made at, the others with none until given
  // one. A size it already has is answered with no change.
  const held = new Map<number, string>([[0, '100x50']]);

  const engine = {
    render: vi.fn(),
    dispose: vi.fn(),
    advance: vi.fn(() => false),
    scramble: vi.fn(),
    resetCube: vi.fn(),
    ambientStart: vi.fn(() => true),
    ambientStop: vi.fn(),
    isAmbient: vi.fn(() => false),
    isSolved: vi.fn(() => true),
    committedMoveCount: vi.fn(() => 0),
    turnFace: vi.fn(() => true),
    setViewMode: vi.fn(),
    viewMode: vi.fn(() => 1),
    setFlatStyle: vi.fn(),
    flatStyle: vi.fn(() => 0),
    resetView: vi.fn(),
    isBusy: vi.fn(() => false),
    restoreSession: vi.fn(() => true),
    pointerMove: vi.fn(),
    pointerUp: vi.fn(),
    pointerCancel: vi.fn(),
    presentSurface: vi.fn((id: number, target: HTMLCanvasElement) => {
      presented.set(id, target);
    }),
    setSurfaceScenes: vi.fn(),
    // Commits the size to the canvas the way CubeEngine does; surface 0's
    // canvas is the one the engine was made with.
    resizeSurface: vi.fn(
      (id: number, size: { width: number; height: number } | null): boolean => {
        const key = size === null ? 'away' : `${size.width}x${size.height}`;
        if ((held.get(id) ?? 'away') === key) return false;
        held.set(id, key);
        const target = id === 0 ? canvas : presented.get(id);
        if (size !== null && target !== undefined) {
          target.width = size.width;
          target.height = size.height;
        }
        return true;
      },
    ),
    pointerDownOn: vi.fn(() => true),
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

  const documentListeners = new Map<string, Set<(event: Event) => void>>();
  const targetDocument = {
    hidden: false,
    addEventListener: vi.fn((type: string, listener: (event: Event) => void) => {
      const bucket = documentListeners.get(type) ?? new Set();
      bucket.add(listener);
      documentListeners.set(type, bucket);
    }),
    removeEventListener: vi.fn(
      (type: string, listener: (event: Event) => void) => {
        documentListeners.get(type)?.delete(listener);
      },
    ),
  };

  const states: Array<[string, string]> = [];
  const onError = vi.fn();

  // The address bar, as much of it as the lifecycle touches: one fragment on
  // the way in, and one replaceState that has to take it off again.
  const hash = overrides.hash ?? '';
  const targetLocation = {
    href: `https://example.test/cube/${hash}`,
    hash,
  };
  const replaced: string[] = [];
  const targetHistory = {
    replaceState: vi.fn((_data: unknown, _unused: string, url: string) => {
      replaced.push(url);
    }),
  };

  // A stand-in for the game controller: the lifecycle only has to build it,
  // call it once per frame, and release it in the right order.
  const game = {
    state: 'idle' as const,
    afterEngineFrame: vi.fn(),
    teardown: vi.fn(),
  };
  let gameOptions: GameControllerOptions | null = null;
  const blocked = (): boolean => false;

  const gameUi = createGameUi();
  const start = () =>
    startApp({
      canvas,
      views: overrides.views,
      gameUi,
      blocked,
      setState: (state, message) => states.push([state, message]),
      onError,
      createEngine: async () => engine,
      createObserver:
        overrides.createObserver ??
        ((callback) => {
          observerCallback = callback;
          return observer;
        }),
      createGameController:
        overrides.createGameController ??
        ((options) => {
          gameOptions = options;
          return game;
        }),
      targetWindow,
      targetDocument,
      targetLocation,
      targetHistory,
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
    game,
    gameUi,
    gameOptions: () => gameOptions,
    blocked,
    triggerObserver: () => observerCallback(),
    dispatch,
    listenerCount,
    /** A screen of another density, as moving the window would give. */
    setDevicePixelRatio: (value: number): void => {
      targetWindow.devicePixelRatio = value;
    },
    /** Takes the tab out of sight, or brings it back, as the browser would. */
    setHidden: (hidden: boolean): void => {
      targetDocument.hidden = hidden;
      for (const listener of documentListeners.get('visibilitychange') ?? []) {
        listener(new Event('visibilitychange'));
      }
    },
    documentListenerCount: (type: string): number =>
      documentListeners.get(type)?.size ?? 0,
    dispatchPointer,
    runFrame,
    hasPendingFrame: () => pendingFrame !== null,
    requestFrame,
    cancelFrame,
    states,
    onError,
    targetHistory,
    replaced,
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

    expect(harness.engine.resizeSurface).toHaveBeenLastCalledWith(
      CubeSurface.Cube,
      { width: 60, height: 50 },
    );
    expect(harness.engine.render).toHaveBeenCalledTimes(2);
  });

  it('draws nothing more when no drawing buffer size changed', async () => {
    const harness = createHarness();
    await harness.start();
    harness.engine.resizeSurface.mockClear();

    harness.triggerObserver();

    // The engine is the one holding the sizes, so it is asked, and says no.
    expect(harness.engine.resizeSurface).toHaveBeenCalledTimes(1);
    expect(harness.engine.resizeSurface.mock.results[0]!.value).toBe(false);
    expect(harness.engine.render).toHaveBeenCalledTimes(1);
  });

  it('tears everything down and reports when a resize fails', async () => {
    const harness = createHarness();
    await harness.start();

    harness.engine.resizeSurface.mockImplementationOnce(() => {
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

    expect(harness.engine.resizeSurface).toHaveBeenLastCalledWith(
      CubeSurface.Cube,
      { width: 60, height: 50 },
    );
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

    expect(harness.engine.resizeSurface).toHaveBeenLastCalledWith(
      CubeSurface.Cube,
      { width: 60, height: 50 },
    );
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

    harness.engine.resizeSurface.mockClear();
    controller.teardown();
    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();

    expect(harness.engine.resizeSurface).not.toHaveBeenCalled();
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

    harness.engine.resizeSurface.mockImplementationOnce(() => {
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

    expect(harness.engine.pointerDownOn).toHaveBeenCalledWith(
      CubeSurface.Cube,
      10,
      10,
    );
    expect(harness.hasPendingFrame()).toBe(true);
  });

  it('leaves the page alone when the engine declines the press', async () => {
    const harness = createHarness();
    harness.engine.pointerDownOn.mockReturnValueOnce(false);
    await harness.start();

    harness.dispatchPointer('pointerdown', {});

    expect(harness.canvasState.setPointerCapture).not.toHaveBeenCalled();
    expect(harness.hasPendingFrame()).toBe(false);
  });

  it('resizes nothing under a gesture, and catches up when it ends', async () => {
    const harness = createHarness();
    await harness.start();
    harness.engine.resizeSurface.mockClear();

    // A box that changes under a finger: the drag's directions were read off
    // the old size, and resizing the surface would drop it.
    harness.dispatchPointer('pointerdown', {});
    harness.canvasState.clientWidth = 60;
    harness.triggerObserver();
    harness.dispatch('resize', new Event('resize'));
    expect(harness.engine.resizeSurface).not.toHaveBeenCalled();

    // Let go, and the size it is owed arrives, with a frame at it.
    harness.dispatchPointer('pointerup', {});
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Cube, {
      width: 60,
      height: 50,
    });
    expect(harness.engine.render).toHaveBeenCalledTimes(2);

    // Nothing owed, nothing done at the end of the next one.
    harness.engine.resizeSurface.mockClear();
    harness.dispatchPointer('pointerdown', {});
    harness.dispatchPointer('pointerup', {});
    expect(harness.engine.resizeSurface).not.toHaveBeenCalled();
  });

  it('takes a frame the controller asked for as the next one, clock and all', async () => {
    const harness = createHarness();
    await harness.start();
    // A walk: every frame the controller puts its next step in, and asks for
    // a frame for it, while the engine is still asking for frames itself.
    harness.game.afterEngineFrame.mockImplementation(() =>
      harness.gameOptions()!.startFrameLoop(),
    );
    harness.engine.advance.mockReturnValue(true);

    harness.dispatchPointer('pointerdown', {});
    harness.requestFrame.mockClear();

    // One request a frame, not two -- two would run two frames a tick, each
    // advancing the clock -- and the second frame measures the time since
    // the first rather than starting over at nothing.
    harness.runFrame(1000);
    expect(harness.requestFrame).toHaveBeenCalledTimes(1);
    harness.runFrame(1016);
    expect(harness.requestFrame).toHaveBeenCalledTimes(2);
    expect(harness.engine.advance).toHaveBeenLastCalledWith(16);
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

  it('gives the gameplay controller the engine, the DOM, and the loop', async () => {
    const harness = createHarness();
    await harness.start();

    expect(harness.gameOptions()?.engine).toBe(harness.engine);
    expect(harness.gameOptions()?.ui).toBe(harness.gameUi);
    expect(harness.gameOptions()?.blocked).toBe(harness.blocked);

    // A command the engine accepted has to be able to animate.
    harness.gameOptions()?.startFrameLoop();
    expect(harness.hasPendingFrame()).toBe(true);
  });

  it('lets the gameplay controller observe every drawn frame', async () => {
    const harness = createHarness();
    await harness.start();

    expect(harness.game.afterEngineFrame).not.toHaveBeenCalled();

    harness.engine.advance.mockReturnValueOnce(true);
    harness.dispatchPointer('pointerdown', {});
    harness.runFrame(1000);
    harness.runFrame(1016);

    // Once per frame, and after the frame reached the canvas: what it reads
    // back from the engine is what the user is looking at.
    expect(harness.game.afterEngineFrame).toHaveBeenCalledTimes(2);
    expect(
      harness.game.afterEngineFrame.mock.invocationCallOrder.at(-1)!,
    ).toBeGreaterThan(harness.engine.render.mock.invocationCallOrder.at(-1)!);
  });

  it('releases the gameplay controller before the engine', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    controller.teardown();

    // Its listeners still call into the engine, so it has to go first.
    expect(harness.game.teardown).toHaveBeenCalledTimes(1);
    expect(harness.game.teardown.mock.invocationCallOrder[0]!).toBeLessThan(
      harness.engine.dispose.mock.invocationCallOrder[0]!,
    );
  });

  it('tears everything down when a gameplay command fails', async () => {
    const harness = createHarness();
    await harness.start();

    harness.gameOptions()?.onError(new Error('command failed'));

    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.game.teardown).toHaveBeenCalledTimes(1);
    expect(harness.listenerCount('resize')).toBe(0);
    expect(harness.onError).toHaveBeenCalledWith(
      expect.objectContaining({ message: 'command failed' }),
    );
  });

  it('unwinds the engine when the gameplay controller fails to attach', async () => {
    const harness = createHarness({
      createGameController: () => {
        throw new Error('no controls');
      },
    });

    await expect(harness.start()).rejects.toThrow('no controls');
    expect(harness.engine.dispose).toHaveBeenCalledTimes(1);
    expect(harness.observer.observe).not.toHaveBeenCalled();
    expect(harness.listenerCount('resize')).toBe(0);
  });

  it('stops the frame loop while the tab is out of sight and gives it back', async () => {
    const harness = createHarness();
    await harness.start();

    // Something that keeps asking for frames, which is what a watched pattern
    // is: a loop that would otherwise run for as long as the tab is away.
    harness.engine.advance.mockReturnValue(true);
    harness.dispatchPointer('pointerdown', {});
    harness.runFrame(1000);
    expect(harness.hasPendingFrame()).toBe(true);

    harness.setHidden(true);
    expect(harness.cancelFrame).toHaveBeenCalledTimes(1);
    expect(harness.hasPendingFrame()).toBe(false);

    const framesWhileAway = harness.engine.advance.mock.calls.length;
    harness.setHidden(false);
    expect(harness.hasPendingFrame()).toBe(true);

    // The hours the tab spent away are not an elapsed time: the first frame
    // back measures nothing, so nothing animating jumps to its end.
    harness.runFrame(9_999_000);
    expect(harness.engine.advance).toHaveBeenCalledTimes(framesWhileAway + 1);
    expect(harness.engine.advance).toHaveBeenLastCalledWith(0);
  });

  it('gives back no loop that was not taken away', async () => {
    const harness = createHarness();
    await harness.start();

    // A still cube costs no frames, and going away and coming back is not a
    // reason to start any.
    harness.setHidden(true);
    harness.setHidden(false);

    expect(harness.cancelFrame).not.toHaveBeenCalled();
    expect(harness.requestFrame).not.toHaveBeenCalled();
  });

  it('releases the visibility listener with everything else', async () => {
    const harness = createHarness();
    const controller = await harness.start();

    expect(harness.documentListenerCount('visibilitychange')).toBe(1);

    controller.teardown();
    expect(harness.documentListenerCount('visibilitychange')).toBe(0);

    // And a change arriving after that starts nothing.
    harness.setHidden(false);
    expect(harness.requestFrame).not.toHaveBeenCalled();
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

describe('startApp and a shared link', () => {
  /** One session encoded the way a share button would write it. */
  const SHARED = encodeSession({ size: 3, scramble: [0x44], user: [0x45] })!;

  it('starts fresh when the address carries nothing', async () => {
    const harness = createHarness();
    await harness.start();

    expect(harness.engine.restoreSession).not.toHaveBeenCalled();
    expect(harness.targetHistory.replaceState).not.toHaveBeenCalled();
    expect(harness.states.at(-1)).toEqual([
      'ready',
      'Ready. Scramble the cube to begin.',
    ]);
  });

  it('restores the cube before the first frame is drawn', async () => {
    const harness = createHarness({ hash: `#s=${SHARED}` });
    // Ordered against the render, because the order is the point: nothing
    // animates after a restore, so a frame drawn before it would be the only
    // frame, and it would be showing a solved cube.
    const order: string[] = [];
    harness.engine.restoreSession.mockImplementation(() => {
      order.push('restore');
      return true;
    });
    harness.engine.render.mockImplementation(() => {
      order.push('render');
    });

    await harness.start();

    expect(harness.engine.restoreSession).toHaveBeenCalledWith(
      3,
      [0x44],
      [0x45],
    );
    expect(order).toEqual(['restore', 'render']);
    expect(harness.states.at(-1)).toEqual([
      'ready',
      'Ready. This cube came from a shared link.',
    ]);
  });

  it('takes the fragment off the address whichever way it went', async () => {
    for (const hash of [`#s=${SHARED}`, '#s=not-a-payload']) {
      const harness = createHarness({ hash });
      await harness.start();

      // A fragment left behind would replay the same reading -- and, for a
      // damaged one, the same refusal -- on every reload.
      expect(harness.replaced).toEqual(['/cube/']);
    }
  });

  it('degrades a payload it cannot read to a fresh cube', async () => {
    const harness = createHarness({ hash: '#s=@@@not-base64@@@' });
    await harness.start();

    expect(harness.engine.restoreSession).not.toHaveBeenCalled();
    expect(harness.engine.resetCube).not.toHaveBeenCalled();
    expect(harness.states.at(-1)).toEqual([
      'ready',
      'That shared link could not be read. Ready with a fresh cube.',
    ]);
  });

  it('starts a clean session when the engine turns the record down', async () => {
    const harness = createHarness({ hash: `#s=${SHARED}` });
    harness.engine.restoreSession.mockReturnValue(false);

    await harness.start();

    // No rollback and no second attempt: a damaged link gets a new cube,
    // which is the only thing anybody could do about it.
    expect(harness.engine.resetCube).toHaveBeenCalledTimes(1);
    expect(harness.engine.restoreSession).toHaveBeenCalledTimes(1);
    expect(harness.states.at(-1)).toEqual([
      'ready',
      'That shared link could not be read. Ready with a fresh cube.',
    ]);
  });
});

/**
 * A canvas beside the cube's: its CSS box, which a test changes the way a
 * layout would, and its own listeners, so a press can land on it.
 */
function createViewCanvas(clientWidth: number, clientHeight: number, left: number) {
  const listeners = new Map<string, Set<(event: Event) => void>>();
  const state = {
    width: 0,
    height: 0,
    clientWidth,
    clientHeight,
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
    getBoundingClientRect: () => ({
      left,
      top: 0,
      width: state.clientWidth,
      height: state.clientHeight,
    }),
    hasPointerCapture: vi.fn(() => true),
    setPointerCapture: vi.fn(),
    releasePointerCapture: vi.fn(),
  };
  const dispatch = (type: string, clientX: number, clientY: number): void => {
    const event = {
      pointerId: 1,
      isPrimary: true,
      button: 0,
      clientX,
      clientY,
    } as PointerEvent;
    for (const listener of listeners.get(type) ?? []) {
      listener(event as unknown as Event);
    }
  };
  const press = (clientX: number, clientY: number): void =>
    dispatch('pointerdown', clientX, clientY);
  const release = (): void => dispatch('pointerup', 0, 0);
  const listenerCount = (): number => {
    let total = 0;
    for (const bucket of listeners.values()) total += bucket.size;
    return total;
  };
  return {
    canvas: state as unknown as HTMLCanvasElement,
    state,
    press,
    release,
    listenerCount,
  };
}

/** A net beside the cube and the axes badge, as the page lays them out. */
function createViews() {
  const net = createViewCanvas(80, 60, 110);
  const axes = createViewCanvas(20, 20, 0);
  const views: StageView[] = [
    { id: CubeSurface.Net, canvas: net.canvas, scenes: CubeScene.Net, pressable: true },
    {
      id: CubeSurface.Axes,
      canvas: axes.canvas,
      scenes: CubeScene.Axes,
      pressable: false,
    },
  ];
  return { net, axes, views };
}

describe('startApp with a canvas per view', () => {
  it('gives each view its canvas and scenes, and the cube the rest', async () => {
    const { net, axes, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    expect(harness.engine.presentSurface).toHaveBeenCalledWith(
      CubeSurface.Net,
      net.canvas,
    );
    expect(harness.engine.presentSurface).toHaveBeenCalledWith(
      CubeSurface.Axes,
      axes.canvas,
    );
    expect(harness.engine.setSurfaceScenes).toHaveBeenCalledWith(
      CubeSurface.Net,
      CubeScene.Net,
    );
    // Nobody took the rings, so they stay with the cube.
    expect(harness.engine.setSurfaceScenes).toHaveBeenCalledWith(
      CubeSurface.Cube,
      CubeScene.Cube | CubeScene.Rings,
    );
  });

  it('sizes every view before the first frame', async () => {
    const { views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Net, {
      width: 80,
      height: 60,
    });
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Axes, {
      width: 20,
      height: 20,
    });
    // The cube's canvas was made at the size of its box, so asked again it
    // has nothing to change.
    const { calls, results } = harness.engine.resizeSurface.mock;
    const cube = calls.findIndex(([id]) => id === CubeSurface.Cube);
    expect(results[cube]!.value).toBe(false);
    const lastResize = Math.max(
      ...harness.engine.resizeSurface.mock.invocationCallOrder,
    );
    expect(lastResize).toBeLessThan(
      harness.engine.render.mock.invocationCallOrder[0],
    );
    expect(harness.engine.render).toHaveBeenCalledTimes(1);
  });

  it('draws every view at one density once together they pass the budget', async () => {
    const { net, views } = createViews();
    net.state.clientWidth = 2000;
    net.state.clientHeight = 1500;
    const harness = createHarness({ views });
    await harness.start();

    const area = 100 * 50 + 2000 * 1500 + 20 * 20;
    const density = Math.sqrt(MAX_STAGE_PIXELS / area);
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Net, {
      width: Math.round(2000 * density),
      height: Math.round(1500 * density),
    });
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Cube, {
      width: Math.round(100 * density),
      height: Math.round(50 * density),
    });
  });

  it('puts a view away when its canvas leaves the page, and back', async () => {
    const { net, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();
    harness.engine.resizeSurface.mockClear();

    net.state.clientWidth = 0;
    net.state.clientHeight = 0;
    harness.triggerObserver();
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Net, null);
    expect(harness.engine.render).toHaveBeenCalledTimes(2);

    // Nothing moved since, so nothing changes and nothing is drawn.
    harness.engine.resizeSurface.mockClear();
    harness.triggerObserver();
    expect(
      harness.engine.resizeSurface.mock.results.every(
        (result) => result.value === false,
      ),
    ).toBe(true);
    expect(harness.engine.render).toHaveBeenCalledTimes(2);

    net.state.clientWidth = 80;
    net.state.clientHeight = 60;
    harness.engine.resizeSurface.mockClear();
    harness.triggerObserver();
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Net, {
      width: 80,
      height: 60,
    });
    expect(harness.engine.render).toHaveBeenCalledTimes(3);
  });

  it('watches every canvas for a new box', async () => {
    const { net, axes, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    expect(harness.observer.observe).toHaveBeenCalledTimes(3);
    expect(harness.observer.observe).toHaveBeenCalledWith(net.canvas);
    expect(harness.observer.observe).toHaveBeenCalledWith(axes.canvas);
  });

  it('presses a view on its own surface, and passes its empty part to the cube', async () => {
    const { net, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    net.press(130, 30);
    expect(harness.engine.pointerDownOn).toHaveBeenCalledWith(
      CubeSurface.Net,
      20,
      30,
    );
    expect(harness.engine.pointerDownOn).toHaveBeenCalledTimes(1);
    expect(harness.hasPendingFrame()).toBe(true);
    net.release();
    expect(harness.engine.pointerUp).toHaveBeenCalledTimes(1);

    // Turned down there, the same press turns the viewpoint: offered to the
    // cube's canvas in its own pixels, off its right-hand edge.
    harness.engine.pointerDownOn.mockReturnValueOnce(false);
    net.press(130, 30);
    expect(harness.engine.pointerDownOn).toHaveBeenLastCalledWith(
      CubeSurface.Cube,
      130,
      30,
    );
  });

  it('puts the cube away with its canvas, and a press beside it reaches nothing', async () => {
    const { net, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    // The 2D view: the cube's canvas has no box, so its surface is put away.
    harness.canvasState.clientWidth = 0;
    harness.canvasState.clientHeight = 0;
    harness.triggerObserver();
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(
      CubeSurface.Cube,
      null,
    );

    // A press on the net's empty part is offered to the cube's canvas, which
    // has no pixels to put it in: nothing begins, and nothing is captured.
    harness.engine.pointerDownOn.mockClear();
    harness.engine.pointerDownOn.mockReturnValueOnce(false);
    net.press(130, 30);
    expect(harness.engine.pointerDownOn).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerDownOn).toHaveBeenCalledWith(
      CubeSurface.Net,
      20,
      30,
    );
    expect(net.state.setPointerCapture).not.toHaveBeenCalled();
    expect(harness.hasPendingFrame()).toBe(false);
  });

  it('draws every view again at a screen of another density', async () => {
    const { views } = createViews();
    const harness = createHarness({ views });
    await harness.start();
    harness.engine.resizeSurface.mockClear();

    // Moving the window to a denser screen arrives as a resize of the window,
    // with every box the size it was.
    harness.setDevicePixelRatio(2);
    harness.dispatch('resize', new Event('resize'));

    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Cube, {
      width: 200,
      height: 100,
    });
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Net, {
      width: 160,
      height: 120,
    });
    expect(harness.engine.resizeSurface).toHaveBeenCalledWith(CubeSurface.Axes, {
      width: 40,
      height: 40,
    });
    expect(harness.engine.render).toHaveBeenCalledTimes(2);
  });

  it('leaves a view that is only looked at alone', async () => {
    const { axes, views } = createViews();
    const harness = createHarness({ views });
    await harness.start();

    expect(axes.listenerCount()).toBe(0);
  });

  it('releases every canvas on teardown', async () => {
    const { net, views } = createViews();
    const harness = createHarness({ views });
    const app = await harness.start();

    expect(net.listenerCount()).toBeGreaterThan(0);
    app.teardown();
    expect(net.listenerCount()).toBe(0);
  });
});

describe('stageDensity', () => {
  it('keeps the device ratio while the views fit the budget', () => {
    expect(stageDensity([{ width: 100, height: 50 }], 2)).toBe(2);
    expect(stageDensity([], 3)).toBe(3);
  });

  it('brings every view down together past the budget', () => {
    const boxes = [
      { width: 1000, height: 1000 },
      { width: 600, height: 400 },
    ];
    const density = stageDensity(boxes, 2);
    expect(density).toBeLessThan(2);
    const pixels = boxes.reduce(
      (sum, box) => sum + box.width * density * box.height * density,
      0,
    );
    expect(pixels).toBeCloseTo(MAX_STAGE_PIXELS, -2);
  });

  it('takes a budget of its own', () => {
    expect(stageDensity([{ width: 100, height: 100 }], 1, 2_500)).toBe(0.5);
  });

  it('takes a ratio that is not a positive number as one', () => {
    expect(stageDensity([{ width: 10, height: 10 }], Number.NaN)).toBe(1);
    expect(stageDensity([{ width: 10, height: 10 }], 0)).toBe(1);
  });
});
