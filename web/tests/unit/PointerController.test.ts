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

  it('captures the pointer and starts the loop only when the cube is hit', () => {
    const harness = createHarness();
    harness.engine.pointerDown.mockReturnValueOnce(false);

    harness.dispatch('pointerdown');

    expect(harness.canvasState.setPointerCapture).not.toHaveBeenCalled();
    expect(harness.onGestureStart).not.toHaveBeenCalled();

    // Nothing was captured, so the following move belongs to no gesture.
    harness.dispatch('pointermove', { clientX: 70, clientY: 60 });
    expect(harness.engine.pointerMove).not.toHaveBeenCalled();
  });

  it('captures the pointer and starts the loop on a hit', () => {
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
