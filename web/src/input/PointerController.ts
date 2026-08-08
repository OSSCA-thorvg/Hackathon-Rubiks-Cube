/**
 * Browser pointer state for one canvas.
 *
 * The division of labour follows the rest of the boundary: the browser owns
 * the event plumbing, capture, and the conversion from CSS pixels to drawing
 * buffer pixels, while the engine decides what a drag means.
 */

/** The engine calls this module needs; CubeEngine satisfies it. */
export type PointerTarget = {
  /** @returns true when the press grabbed the cube. */
  pointerDown(x: number, y: number): boolean;
  pointerMove(x: number, y: number): void;
  pointerUp(): void;
  pointerCancel(): void;
};

export type PointerControllerOptions = {
  readonly canvas: HTMLCanvasElement;
  readonly engine: PointerTarget;
  /** Called when a press grabs the cube, so a frame loop can start. */
  readonly onGestureStart: () => void;
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
 * One gesture at a time: the first primary pointer that grabs the cube owns
 * the canvas until it ends, and every other pointer is ignored meanwhile.
 * Everything that ends a gesture without a deliberate release — a cancelled
 * pointer, lost capture, teardown — goes to pointerCancel(), so the engine
 * can tell those apart from letting go.
 */
export function attachPointer(
  options: PointerControllerOptions,
): PointerController {
  const { canvas, engine, onGestureStart } = options;

  let activePointerId: number | null = null;

  const releaseCapture = (pointerId: number): void => {
    // Capture may already be gone, which is exactly the case that brings us
    // here through lostpointercapture.
    if (!canvas.hasPointerCapture(pointerId)) return;
    canvas.releasePointerCapture(pointerId);
  };

  const onPointerDown = (event: PointerEvent): void => {
    if (activePointerId !== null) return;
    // Secondary buttons and non-primary pointers are somebody else's gesture.
    if (!event.isPrimary || event.button !== 0) return;

    const point = toBufferPixels(canvas, event);
    if (point === null) return;

    // Capture and the frame loop start only for a press that actually landed
    // on the cube, so pressing the background leaves the page alone.
    if (!engine.pointerDown(point.x, point.y)) return;

    activePointerId = event.pointerId;
    canvas.setPointerCapture(event.pointerId);
    onGestureStart();
  };

  const onPointerMove = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    const point = toBufferPixels(canvas, event);
    if (point === null) return;

    engine.pointerMove(point.x, point.y);
  };

  const onPointerUp = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    // Cleared before releasing capture, so the lostpointercapture that
    // release triggers is not mistaken for a cancellation.
    activePointerId = null;
    releaseCapture(event.pointerId);
    engine.pointerUp();
  };

  const onPointerCancel = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    activePointerId = null;
    releaseCapture(event.pointerId);
    engine.pointerCancel();
  };

  const onLostPointerCapture = (event: PointerEvent): void => {
    if (event.pointerId !== activePointerId) return;

    // Capture taken away mid-drag: the user never let go, so nothing commits.
    activePointerId = null;
    engine.pointerCancel();
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
      activePointerId = null;
      releaseCapture(pointerId);
      engine.pointerCancel();
    },
  };
}
