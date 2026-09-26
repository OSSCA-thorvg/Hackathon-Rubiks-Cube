import { describe, expect, it, vi } from 'vitest';

import {
  attachPointer,
  type PointerTarget,
} from '../../src/input/PointerController.ts';

/**
 * Builds a canvas whose CSS box is half the drawing buffer, which is what a
 * device pixel ratio of two produces. Pointer coordinates arrive in CSS
 * pixels, so every conversion here has to double.
 */
function createHarness() {
  const listeners = new Map<string, Set<(event: Event) => void>>();
  let captured: number | null = null;

  const canvasState = {
    width: 400,
    height: 400,
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
    getBoundingClientRect: () => ({ left: 20, top: 10, width: 200, height: 200 }),
    hasPointerCapture: vi.fn((pointerId: number) => captured === pointerId),
    setPointerCapture: vi.fn((pointerId: number) => {
      captured = pointerId;
    }),
    releasePointerCapture: vi.fn(() => {
      captured = null;
    }),
  };
  const canvas = canvasState as unknown as HTMLCanvasElement;

  const engine = {
    pointerDown: vi.fn(() => true),
    pointerMove: vi.fn(),
    pointerUp: vi.fn(),
    pointerCancel: vi.fn(),
  } satisfies PointerTarget;

  const onGestureStart = vi.fn();
  const controller = attachPointer({ canvas, engine, onGestureStart });

  const dispatch = (type: string, event: Partial<PointerEvent> = {}): void => {
    const full = {
      pointerId: 1,
      isPrimary: true,
      button: 0,
      clientX: 20,
      clientY: 10,
      ...event,
    } as PointerEvent;
    for (const listener of listeners.get(type) ?? []) {
      listener(full as unknown as Event);
    }
  };

  const listenerCount = (): number => {
    let total = 0;
    for (const bucket of listeners.values()) total += bucket.size;
    return total;
  };

  return {
    canvasState,
    engine,
    onGestureStart,
    controller,
    dispatch,
    listenerCount,
  };
}

describe('attachPointer', () => {
  it('converts client coordinates into drawing buffer pixels', () => {
    const harness = createHarness();

    harness.dispatch('pointerdown', { clientX: 70, clientY: 60 });

    // 50 CSS pixels into a 200 wide box that backs a 400 wide buffer.
    expect(harness.engine.pointerDown).toHaveBeenCalledWith(100, 100);
  });

  it('stays out of the way when the engine declines the press', () => {
    const harness = createHarness();
    harness.engine.pointerDown.mockReturnValueOnce(false);

    harness.dispatch('pointerdown');

    expect(harness.canvasState.setPointerCapture).not.toHaveBeenCalled();
    expect(harness.onGestureStart).not.toHaveBeenCalled();

    // Nothing was captured, so the following move belongs to no gesture.
    harness.dispatch('pointermove', { clientX: 70, clientY: 60 });
    expect(harness.engine.pointerMove).not.toHaveBeenCalled();
  });

  it('captures the pointer and starts the loop once a gesture begins', () => {
    // Whether the press landed on the cube is the engine's business: a miss
    // sweeps the viewpoint, which needs capture and frames just the same.
    const harness = createHarness();

    harness.dispatch('pointerdown');

    expect(harness.canvasState.setPointerCapture).toHaveBeenCalledWith(1);
    expect(harness.onGestureStart).toHaveBeenCalledTimes(1);
  });

  it('ignores non-primary pointers and secondary buttons', () => {
    const harness = createHarness();

    harness.dispatch('pointerdown', { isPrimary: false });
    harness.dispatch('pointerdown', { button: 2 });

    expect(harness.engine.pointerDown).not.toHaveBeenCalled();
  });

  it('ignores a second pointer while a gesture is running', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown', { pointerId: 1 });

    harness.dispatch('pointerdown', { pointerId: 2 });
    harness.dispatch('pointermove', { pointerId: 2, clientX: 70 });
    harness.dispatch('pointerup', { pointerId: 2 });

    expect(harness.engine.pointerDown).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerMove).not.toHaveBeenCalled();
    expect(harness.engine.pointerUp).not.toHaveBeenCalled();
  });

  it('forwards moves and the release of the active pointer', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown');

    harness.dispatch('pointermove', { clientX: 120, clientY: 110 });
    harness.dispatch('pointerup');

    expect(harness.engine.pointerMove).toHaveBeenCalledWith(200, 200);
    expect(harness.engine.pointerUp).toHaveBeenCalledTimes(1);
    expect(harness.canvasState.releasePointerCapture).toHaveBeenCalledWith(1);
  });

  it('does not cancel when releasing capture follows a real release', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown');

    // Browsers fire lostpointercapture as part of the release, and that must
    // not read as a cancellation of the turn the user just asked for.
    harness.dispatch('pointerup');
    harness.dispatch('lostpointercapture');

    expect(harness.engine.pointerUp).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerCancel).not.toHaveBeenCalled();
  });

  it('cancels the gesture when the pointer is cancelled', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown');

    harness.dispatch('pointercancel');

    expect(harness.engine.pointerCancel).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerUp).not.toHaveBeenCalled();

    // The gesture is over, so later events for that pointer go nowhere.
    harness.dispatch('pointermove', { clientX: 120 });
    expect(harness.engine.pointerMove).not.toHaveBeenCalled();
  });

  it('cancels the gesture when capture is taken away mid-drag', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown');

    harness.dispatch('lostpointercapture');

    expect(harness.engine.pointerCancel).toHaveBeenCalledTimes(1);
    expect(harness.engine.pointerUp).not.toHaveBeenCalled();
  });

  it('releases everything on teardown, cancelling a gesture in flight', () => {
    const harness = createHarness();
    harness.dispatch('pointerdown');

    harness.controller.teardown();

    expect(harness.engine.pointerCancel).toHaveBeenCalledTimes(1);
    expect(harness.canvasState.releasePointerCapture).toHaveBeenCalledWith(1);
    expect(harness.listenerCount()).toBe(0);
  });

  it('tears down quietly when no gesture is running', () => {
    const harness = createHarness();

    harness.controller.teardown();

    expect(harness.engine.pointerCancel).not.toHaveBeenCalled();
    expect(harness.listenerCount()).toBe(0);
  });
});

/**
 * A canvas of its own, for the tests where more than one takes presses: its
 * box on the page and its drawing buffer, and a way to press it.
 */
function createCanvas(
  box: { left: number; top: number; width: number; height: number },
  buffer: { width: number; height: number },
) {
  const listeners = new Map<string, Set<(event: Event) => void>>();
  let captured: number | null = null;
  const state = {
    ...buffer,
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
    getBoundingClientRect: () => box,
    hasPointerCapture: vi.fn((pointerId: number) => captured === pointerId),
    setPointerCapture: vi.fn((pointerId: number) => {
      captured = pointerId;
    }),
    releasePointerCapture: vi.fn(() => {
      captured = null;
    }),
  };
  const dispatch = (type: string, event: Partial<PointerEvent> = {}): void => {
    const full = {
      pointerId: 1,
      isPrimary: true,
      button: 0,
      clientX: 0,
      clientY: 0,
      ...event,
    } as PointerEvent;
    for (const listener of listeners.get(type) ?? []) {
      listener(full as unknown as Event);
    }
  };
  return { canvas: state as unknown as HTMLCanvasElement, state, dispatch };
}

function createTarget(accepts = true) {
  return {
    pointerDown: vi.fn(() => accepts),
    pointerMove: vi.fn(),
    pointerUp: vi.fn(),
    pointerCancel: vi.fn(),
  } satisfies PointerTarget;
}

describe('attachPointer across canvases', () => {
  it('offers a press its engine turns down to the fallback, in its pixels', () => {
    // The cube's canvas on the left at twice the density, the net's beside it.
    const cube = createCanvas(
      { left: 0, top: 0, width: 100, height: 100 },
      { width: 200, height: 200 },
    );
    const net = createCanvas(
      { left: 110, top: 0, width: 100, height: 100 },
      { width: 100, height: 100 },
    );
    const netTarget = createTarget(false);
    const cubeTarget = createTarget();
    const onGestureStart = vi.fn();
    attachPointer({
      canvas: net.canvas,
      engine: netTarget,
      fallback: { canvas: cube.canvas, engine: cubeTarget },
      onGestureStart,
    });

    net.dispatch('pointerdown', { clientX: 120, clientY: 50 });

    // Asked on its own canvas first, then on the cube's -- where the same
    // point is off the right-hand edge.
    expect(netTarget.pointerDown).toHaveBeenCalledWith(10, 50);
    expect(cubeTarget.pointerDown).toHaveBeenCalledWith(240, 100);
    expect(onGestureStart).toHaveBeenCalledTimes(1);
    // The pointer stays with the canvas it was pressed on.
    expect(net.state.setPointerCapture).toHaveBeenCalledWith(1);

    // And the drag is measured where the gesture is, and ends there too.
    net.dispatch('pointermove', { clientX: 130, clientY: 40 });
    expect(cubeTarget.pointerMove).toHaveBeenCalledWith(260, 80);
    expect(netTarget.pointerMove).not.toHaveBeenCalled();
    net.dispatch('pointerup');
    expect(cubeTarget.pointerUp).toHaveBeenCalledTimes(1);
    expect(netTarget.pointerUp).not.toHaveBeenCalled();
  });

  it('keeps a press its own engine takes on its own canvas', () => {
    const cube = createCanvas(
      { left: 0, top: 0, width: 100, height: 100 },
      { width: 100, height: 100 },
    );
    const net = createCanvas(
      { left: 110, top: 0, width: 100, height: 100 },
      { width: 100, height: 100 },
    );
    const netTarget = createTarget();
    const cubeTarget = createTarget();
    attachPointer({
      canvas: net.canvas,
      engine: netTarget,
      fallback: { canvas: cube.canvas, engine: cubeTarget },
      onGestureStart: vi.fn(),
    });

    net.dispatch('pointerdown', { clientX: 150, clientY: 50 });
    net.dispatch('pointermove', { clientX: 160, clientY: 50 });
    net.dispatch('pointercancel');

    expect(cubeTarget.pointerDown).not.toHaveBeenCalled();
    expect(netTarget.pointerMove).toHaveBeenCalledWith(50, 50);
    expect(netTarget.pointerCancel).toHaveBeenCalledTimes(1);
  });

  it('holds one gesture at a time across the canvases sharing a lock', () => {
    const first = createCanvas(
      { left: 0, top: 0, width: 100, height: 100 },
      { width: 100, height: 100 },
    );
    const second = createCanvas(
      { left: 110, top: 0, width: 100, height: 100 },
      { width: 100, height: 100 },
    );
    const firstTarget = createTarget();
    const secondTarget = createTarget();
    const lock = { held: false };
    attachPointer({
      canvas: first.canvas,
      engine: firstTarget,
      onGestureStart: vi.fn(),
      lock,
    });
    const secondController = attachPointer({
      canvas: second.canvas,
      engine: secondTarget,
      onGestureStart: vi.fn(),
      lock,
    });

    first.dispatch('pointerdown', { pointerId: 1, clientX: 50, clientY: 50 });
    expect(lock.held).toBe(true);
    // A pen on the second canvas is primary for its own kind of pointer, and
    // is somebody else's gesture all the same.
    second.dispatch('pointerdown', { pointerId: 2, clientX: 150, clientY: 50 });
    expect(secondTarget.pointerDown).not.toHaveBeenCalled();

    first.dispatch('pointerup', { pointerId: 1 });
    expect(lock.held).toBe(false);
    second.dispatch('pointerdown', { pointerId: 2, clientX: 150, clientY: 50 });
    expect(secondTarget.pointerDown).toHaveBeenCalledTimes(1);

    // Letting go by teardown gives the lock back as well.
    secondController.teardown();
    expect(lock.held).toBe(false);
  });
});
