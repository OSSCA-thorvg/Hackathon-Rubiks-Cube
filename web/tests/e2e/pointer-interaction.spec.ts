import { expect, test, type Page } from '@playwright/test';

import {
  assertFacesAndCorners,
  assertSceneContract,
  assertVisibleFaces,
  BLUE,
  BODY,
  expectedNet,
  FRONT_RIGHT_COLUMN,
  GREEN,
  ORANGE,
  pagePointInCube,
  probeCanvas,
  QUARTER_TURN_DRAG,
  RED,
  WHITE,
  YELLOW,
  type CanvasProbe,
} from './sceneContract.ts';

// The net after R, block by block in probe order, written out rather than
// derived so the browser check does not lean on the same mapping the engine
// used to draw it. R lifts the front face's right column onto the top, so the
// top gains a green column, the front takes yellow from the bottom, the bottom
// takes blue from the back, and the back takes white from the top.
const NET_AFTER_R = [
  // Up
  WHITE, WHITE, GREEN,
  WHITE, WHITE, GREEN,
  WHITE, WHITE, GREEN,
  // Left, untouched
  ORANGE, ORANGE, ORANGE,
  ORANGE, ORANGE, ORANGE,
  ORANGE, ORANGE, ORANGE,
  // Front
  GREEN, GREEN, YELLOW,
  GREEN, GREEN, YELLOW,
  GREEN, GREEN, YELLOW,
  // Right, the turning face itself: still one solid color
  RED, RED, RED,
  RED, RED, RED,
  RED, RED, RED,
  // Back, whose left column in the drawing is the layer that moved
  WHITE, BLUE, BLUE,
  WHITE, BLUE, BLUE,
  WHITE, BLUE, BLUE,
  // Down
  YELLOW, YELLOW, BLUE,
  YELLOW, YELLOW, BLUE,
  YELLOW, YELLOW, BLUE,
];

/** A viewport tall enough that the canvas is comfortably large to aim at. */
const VIEWPORT = { width: 1200, height: 1200 };

function sameGrid(a: number[][], b: number[][]): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

/** Presses the front face's right column, ready to drag. */
async function grabRightColumn(
  page: Page,
  probe: CanvasProbe,
): Promise<{ x: number; y: number }> {
  const grab = pagePointInCube(probe, FRONT_RIGHT_COLUMN);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  return grab;
}

/**
 * Drag distance in CSS pixels for a given turn.
 *
 * The engine measures the drag in drawing buffer pixels against a region
 * given as a fraction of the buffer, so the device pixel ratio cancels and
 * the fraction applies directly to the canvas box.
 */
function dragFor(probe: CanvasProbe, quarterTurns: number): number {
  return QUARTER_TURN_DRAG * probe.box.width * quarterTurns;
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(VIEWPORT);
});

test('dragging the right column turns the cube and the net follows', async ({
  page,
}) => {
  const pageErrors: string[] = [];
  page.on('pageerror', (error) => pageErrors.push(String(error)));

  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);
  assertSceneContract(atRest);

  const grab = await grabRightColumn(page, atRest);
  await page.mouse.move(grab.x, grab.y - dragFor(atRest, 1), { steps: 12 });

  const midDrag = await probeCanvas(page);

  // The turn is only a picture until it is released: the net draws the
  // logical cube, and the logical cube has not changed yet.
  expect(midDrag.net).toEqual(expectedNet());
  expect(sameGrid(midDrag.cubeGrid, atRest.cubeGrid)).toBe(false);

  // The layer has swung away from the rest of the cube, so the cut it leaves
  // behind has to be filled rather than showing the background through it.
  expect(midDrag.cubeGrid).toContainEqual(BODY);

  await page.mouse.up();

  // The snap animates, so the committed state arrives a few frames later.
  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R,
  );

  const turned = await probeCanvas(page);

  // R moves no sticker that the three face samples read, so the rest of the
  // contract still holds exactly.
  assertFacesAndCorners(turned);
  expect(turned.cubeGrid).not.toContainEqual(BODY);

  expect(pageErrors).toEqual([]);
});

test('a drag too short to commit springs back', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);

  const grab = await grabRightColumn(page, atRest);
  // Past the dead zone, but not far enough to round up to a quarter turn.
  await page.mouse.move(grab.x, grab.y - dragFor(atRest, 0.3), { steps: 8 });

  expect(sameGrid((await probeCanvas(page)).cubeGrid, atRest.cubeGrid)).toBe(
    false,
  );

  await page.mouse.up();

  await expect
    .poll(async () => sameGrid((await probeCanvas(page)).cubeGrid, atRest.cubeGrid))
    .toBe(true);

  assertSceneContract(await probeCanvas(page));
});

test('dragging the background sweeps the viewpoint, not the cube', async ({
  page,
}) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);
  assertSceneContract(atRest);

  // A corner of the 3D region, clear of the cube's silhouette. Dragging left
  // by a quarter turn's worth brings the next corner of the cube round: the
  // right-hand face slides across and the back face takes its place.
  const corner = pagePointInCube(atRest, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(corner.x - dragFor(atRest, 1), corner.y, { steps: 12 });
  await page.mouse.up();

  await expect
    .poll(async () => (await probeCanvas(page)).left)
    .toEqual(RED);

  const turned = await probeCanvas(page);
  assertVisibleFaces(turned, WHITE, RED, BLUE);

  // The viewpoint moved; the cube did not.
  expect(turned.net).toEqual(expectedNet());
});

test('a layer still turns after the viewpoint comes back round', async ({
  page,
}) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);

  // Four quarter turns of the viewpoint land back where they started, so the
  // gesture that meant R before has to mean it again.
  const corner = pagePointInCube(atRest, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(corner.x - dragFor(atRest, 4), corner.y, { steps: 24 });
  await page.mouse.up();

  await expect
    .poll(async () => (await probeCanvas(page)).left)
    .toEqual(GREEN);

  const home = await probeCanvas(page);
  const grab = await grabRightColumn(page, home);
  await page.mouse.move(grab.x, grab.y - dragFor(home, 1), { steps: 12 });
  await page.mouse.up();

  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R,
  );
});
