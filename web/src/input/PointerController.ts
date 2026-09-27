/**
 * Browser pointer state for one canvas.
 *
 * The division of labour follows the rest of the boundary: the browser owns
 * the event plumbing, capture, and the conversion from CSS pixels to drawing
 * buffer pixels, while the engine decides what a drag means.
 */

/** The engine calls this module needs; CubeEngine satisfies it. */
export type PointerTarget = {
  /**
   * Begins a gesture: a layer drag over the cube, a viewpoint sweep
   * elsewhere.
   *
   * @returns true when a gesture began, which is what this module gates
   *          pointer capture and the frame loop on.
   */
  pointerDown(x: number, y: number): boolean;
  pointerMove(x: number, y: number): void;
  pointerUp(): void;
  pointerCancel(): void;
};

/**
 * Whether a gesture is in hand, shared by every canvas of one engine.
 *
 * The engine follows one gesture at a time, whichever canvas it began on, so
 * a press on a second canvas while the first is held is somebody else's.
 */
export type GestureLock = { held: boolean };

/**
 * Another canvas a press is offered to when this one's engine turns it down.
 *
 * Measured in that canvas's pixels, press and drag alike, since those are the
 * pixels its engine calls are in: the empty part of a flat view turns the
 * cube's viewpoint the way the empty space round the cube does, and the cube
 * is on a canvas of its own.
 */
export type PointerFallback = {
  readonly canvas: HTMLCanvasElement;
  readonly engine: PointerTarget;
};

export type PointerControllerOptions = {
  readonly canvas: HTMLCanvasElement;
  readonly engine: PointerTarget;
  /** Called when a press begins a gesture, so a frame loop can start. */
  readonly onGestureStart: () => void;
  /**
   * Called once a gesture has ended -- let go, cancelled, or its capture
   * taken away -- after the engine has been told. Not at teardown, which
   * ends everything anyway.
   */
  readonly onGestureEnd?: () => void;
  /** Where a press this canvas's engine turns down is offered next. */
  readonly fallback?: PointerFallback;
  /** Shared with the other canvases of the same engine; one of its own if absent. */
  readonly lock?: GestureLock;
};

export type PointerController = {
  /** Releases listeners and capture, cancelling any gesture in flight. */
  teardown(): void;
};

/**
 * Converts a pointer event to drawing buffer pixels.
 *
 * The ratio between the buffer and the CSS box already contains the device
 * pixel ratio, so reading it here keeps the two in step even mid-resize.
 * Returns null for a canvas with no layout box, where the ratio is undefined.
 */
function toBufferPixels(
  canvas: HTMLCanvasElement,
  event: PointerEvent,
): { x: number; y: number } | null {
  const box = canvas.getBoundingClientRect();
  if (box.width === 0 || box.height === 0) return null;

  return {
    x: ((event.clientX - box.left) * canvas.width) / box.width,
    y: ((event.clientY - box.top) * canvas.height) / box.height,
  };
}

/**
 * Routes pointer events on a canvas to the engine.
 *
 * One gesture at a time: the first primary pointer the engine accepts owns
 * the canvas until it ends, and every other pointer is ignored meanwhile.
 * Everything that ends a gesture without a deliberate release — a cancelled
 * pointer, capture lost while the button is held, teardown — goes to
 * pointerCancel(), so the engine can tell those apart from letting go.
 */
export function attachPointer(
  options: PointerControllerOptions,
): PointerController {
  const { canvas, engine, onGestureStart } = options;
  const lock: GestureLock = options.lock ?? { held: false };

  let activePointerId: number | null = null;
  // Whose pixels and whose engine the gesture in hand is in: this canvas's,
  // or the fallback's when the press was passed on to it.
  let frame: HTMLCanvasElement = canvas;
  let target: PointerTarget = engine;

  /** Ends the gesture on this side, before the engine is told how it ended. */
  const letGo = (): void => {
    activePointerId = null;
    lock.held = false;
  };

  const releaseCapture = (pointerId: number): void => {
    // Capture may already be gone, which is exactly the case that brings us
    // here through lostpointercapture.
    if (!canvas.hasPointerCapture(pointerId)) return;
    canvas.releasePointerCapture(pointerId);
  };

  /** Offers a press to one canvas's engine; true when a gesture began. */
  const offer = (
    on: HTMLCanvasElement,
    to: PointerTarget,
    event: PointerEvent,
  ): boolean => {
    const point = toBufferPixels(on, event);
    if (point === null || !to.pointerDown(point.x, point.y)) return false;
    frame = on;
    target = to;
    return true;
  };

  const onPointerDown = (event: PointerEvent): void => {
    if (activePointerId !== null || lock.held) return;
    // Secondary buttons and non-primary pointers are somebody else's gesture.
    if (!event.isPrimary || event.button !== 0) return;

    // Capture and the frame loop start only for a press the engine turned
    // into a gesture, so a press it declines leaves the page alone.
    const fallback = options.fallback;
    const began =
      offer(canvas, engine, event) ||
      (fallback !== undefined && offer(fallback.canvas, fallback.engine, event));
    if (!began) return;

    activePointerId = event.pointerId;
    lock.held = true;
    canvas.setPointerCapture(event.pointerId);
    onGestureStart();
  };

  const onPointerMove = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    const point = toBufferPixels(frame, event);
    if (point === null) return;

    target.pointerMove(point.x, point.y);
  };

  const onPointerUp = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    // Cleared before releasing capture, so the lostpointercapture that
    // release triggers is not mistaken for a cancellation.
    letGo();
    releaseCapture(event.pointerId);
    target.pointerUp();
    options.onGestureEnd?.();
  };

  const onPointerCancel = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    letGo();
    releaseCapture(event.pointerId);
    target.pointerCancel();
    options.onGestureEnd?.();
  };

  const onLostPointerCapture = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    // Capture can go ahead of the pointerup that should have ended the drag,
    // with the button already up: a browser embedded in another app has been
    // seen to send it in place of the release. A button that is up was let
    // go of, so that is a release. Only capture taken away while the button
    // is still held is a drag the user never finished, and commits nothing.
    letGo();
    if ((event.buttons & 1) === 0) target.pointerUp();
    else target.pointerCancel();
    options.onGestureEnd?.();
  };

  canvas.addEventListener('pointerdown', onPointerDown);
  canvas.addEventListener('pointermove', onPointerMove);
  canvas.addEventListener('pointerup', onPointerUp);
  canvas.addEventListener('pointercancel', onPointerCancel);
  canvas.addEventListener('lostpointercapture', onLostPointerCapture);

  return {
    teardown(): void {
      canvas.removeEventListener('pointerdown', onPointerDown);
      canvas.removeEventListener('pointermove', onPointerMove);
      canvas.removeEventListener('pointerup', onPointerUp);
      canvas.removeEventListener('pointercancel', onPointerCancel);
      canvas.removeEventListener('lostpointercapture', onLostPointerCapture);

      if (activePointerId === null) return;

      const pointerId = activePointerId;
      letGo();
      releaseCapture(pointerId);
      target.pointerCancel();
    },
  };
}
