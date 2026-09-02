import { expect, test, type Page } from '@playwright/test';

import {
  assertFacesAndCorners,
  assertSceneContract,
  assertVisibleFaces,
  BACK_LIT,
  BLUE,
  FRONT_LIT,
  isBody,
  near,
  expectedNet,
  FRONT_BLOCK,
  FRONT_RIGHT_COLUMN,
  GREEN,
  netDragFor,
  ORANGE,
  pagePointInCube,
  pagePointInNet,
  probeCanvas,
  QUARTER_TURN_DRAG,
  RED,
  RIGHT_LIT,
  UP_LIT,
  WHITE,
  YELLOW,
  type CanvasProbe,
  type NetView,
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

// The same net after a second R. Each column the turn moves is one solid
// color in the grid above, so the cycle can be read straight off it: the top
// takes yellow from the front, the front blue from the bottom, the bottom
// white from the back, and the back green from the top.
const NET_AFTER_R2 = [
  // Up
  WHITE, WHITE, YELLOW,
  WHITE, WHITE, YELLOW,
  WHITE, WHITE, YELLOW,
  // Left, untouched
  ORANGE, ORANGE, ORANGE,
  ORANGE, ORANGE, ORANGE,
  ORANGE, ORANGE, ORANGE,
  // Front
  GREEN, GREEN, BLUE,
  GREEN, GREEN, BLUE,
  GREEN, GREEN, BLUE,
  // Right, the turning face itself
  RED, RED, RED,
  RED, RED, RED,
  RED, RED, RED,
  // Back
  GREEN, BLUE, BLUE,
  GREEN, BLUE, BLUE,
  GREEN, BLUE, BLUE,
  // Down
  YELLOW, YELLOW, WHITE,
  YELLOW, YELLOW, WHITE,
  YELLOW, YELLOW, WHITE,
];

// The net after U, written out the same way. U carries each side face's top
// row round onto the face to its left, so the left takes the front's green,
// the front takes the right's red, the right takes the back's blue and the
// back takes the left's orange. Up turns in place and stays one color.
const NET_AFTER_U = [
  // Up
  WHITE, WHITE, WHITE,
  WHITE, WHITE, WHITE,
  WHITE, WHITE, WHITE,
  // Left
  GREEN, GREEN, GREEN,
  ORANGE, ORANGE, ORANGE,
  ORANGE, ORANGE, ORANGE,
  // Front
  RED, RED, RED,
  GREEN, GREEN, GREEN,
  GREEN, GREEN, GREEN,
  // Right
  BLUE, BLUE, BLUE,
  RED, RED, RED,
  RED, RED, RED,
  // Back
  ORANGE, ORANGE, ORANGE,
  BLUE, BLUE, BLUE,
  BLUE, BLUE, BLUE,
  // Down, untouched
  YELLOW, YELLOW, YELLOW,
  YELLOW, YELLOW, YELLOW,
  YELLOW, YELLOW, YELLOW,
];

/**
 * A drag that commits one turn without going near a boundary.
 *
 * Half a quarter turn: well past the threshold that commits, and well short
 * of the one that would commit two. e2e goes through CSS sizes and rounded
 * pointer coordinates, so the boundaries themselves are pinned by the native
 * tests and left alone here.
 */
const SHORT_TURN = 0.5;

/** A viewport tall enough that the canvas is comfortably large to aim at. */
const VIEWPORT = { width: 1200, height: 1200 };

function sameGrid(a: number[][], b: number[][]): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

/** Whether a cut surface is showing, which only happens mid-turn. */
function showsBody(probe: CanvasProbe): boolean {
  return probe.cubeGrid.some(isBody);
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

  // Both views move together now, so the net has changed too. It is still
  // only a picture: the logical cube changes when the release settles.
  expect(sameGrid(midDrag.cubeGrid, atRest.cubeGrid)).toBe(false);
  expect(sameGrid(midDrag.net, expectedNet())).toBe(false);

  // The layer has swung away from the rest of the cube, so the cut it leaves
  // behind has to be filled rather than showing the background through it.
  expect(showsBody(midDrag)).toBe(true);

  await page.mouse.up();

  // The snap animates, so the committed state arrives a few frames later.
  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R,
  );

  // The cut in the 3D view closes when the snap actually stops, which is a
  // frame or two after the net has arrived at the settled drawing.
  await expect.poll(async () => showsBody(await probeCanvas(page))).toBe(false);

  // R moves no sticker that the three face samples read, so the rest of the
  // contract still holds exactly.
  assertFacesAndCorners(await probeCanvas(page));

  expect(pageErrors).toEqual([]);
});

test('a short drag past the threshold turns the cube', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);

  // Nothing like a full quarter turn of travel, but the release settles on
  // one all the same: carrying the face past the threshold is what commits.
  const grab = await grabRightColumn(page, atRest);
  await page.mouse.move(grab.x, grab.y - dragFor(atRest, SHORT_TURN), {
    steps: 8,
  });
  await page.mouse.up();

  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R,
  );
});

test('two drags in quick succession both turn the cube', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);

  // The second press arrives while the first release is still snapping. That
  // press used to be refused, and with it the whole stroke that followed.
  for (let turn = 0; turn < 2; turn += 1) {
    const grab = await grabRightColumn(page, atRest);
    await page.mouse.move(grab.x, grab.y - dragFor(atRest, SHORT_TURN), {
      steps: 4,
    });
    await page.mouse.up();
  }

  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R2,
  );
});

/** Drags the net's front block top middle cell to the left, which means U. */
async function dragNetTopRowLeft(
  page: Page,
  probe: CanvasProbe,
  view: NetView,
): Promise<void> {
  const grab = pagePointInNet(probe, view, FRONT_BLOCK, 1, 0);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(grab.x - netDragFor(probe, view, SHORT_TURN), grab.y, {
    steps: 8,
  });
  await page.mouse.up();
}

test('dragging a net cell turns the cube', async ({ page }) => {
  const pageErrors: string[] = [];
  page.on('pageerror', (error) => pageErrors.push(String(error)));

  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);
  assertSceneContract(atRest);

  await dragNetTopRowLeft(page, atRest, 'both');

  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_U,
  );

  // The gesture started in the net, but the turn is the cube's: the 3D view
  // shows it too, and comes back to rest along with the drawing below it.
  const turned = await probeCanvas(page);
  expect(showsBody(turned)).toBe(false);
  expect(sameGrid(turned.cubeGrid, atRest.cubeGrid)).toBe(false);

  expect(pageErrors).toEqual([]);
});

test('the net is draggable when it is the only view', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  await page.locator('button[data-view="2d"]').click();
  await expect(page.locator('button[data-view="2d"]')).toHaveAttribute(
    'aria-pressed',
    'true',
  );

  const netOnly = await probeCanvas(page);
  await dragNetTopRowLeft(page, netOnly, 'net');

  // Read the result back in the shared layout, which is where the probe's
  // sample points are; the net-only layout puts the same cells elsewhere.
  await page.locator('button[data-view="both"]').click();
  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_U,
  );
});

test('a drag too short to commit springs back', async ({ page }) => {
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const atRest = await probeCanvas(page);

  const grab = await grabRightColumn(page, atRest);
  // Past the dead zone, but nowhere near carrying the face far enough round.
  await page.mouse.move(grab.x, grab.y - dragFor(atRest, 0.15), { steps: 8 });

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
    .poll(async () => near((await probeCanvas(page)).left, RIGHT_LIT, 2))
    .toBe(true);

  const turned = await probeCanvas(page);
  assertVisibleFaces(turned, UP_LIT, RIGHT_LIT, BACK_LIT);

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
    .poll(async () => near((await probeCanvas(page)).left, FRONT_LIT, 2))
    .toBe(true);

  const home = await probeCanvas(page);
  const grab = await grabRightColumn(page, home);
  await page.mouse.move(grab.x, grab.y - dragFor(home, 1), { steps: 12 });
  await page.mouse.up();

  await expect.poll(async () => (await probeCanvas(page)).net).toEqual(
    NET_AFTER_R,
  );
});
