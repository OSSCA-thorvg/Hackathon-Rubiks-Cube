import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  BODY,
  FRONT_RIGHT_COLUMN,
  pagePointInCube,
  probeCanvas,
  QUARTER_TURN_DRAG,
  WHITE,
} from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Drags the background left by one quarter-view turn. */
async function orbitOnce(page: Page): Promise<void> {
  const probe = await probeCanvas(page);
  const corner = pagePointInCube(probe, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(
    corner.x - QUARTER_TURN_DRAG * probe.box.width,
    corner.y,
    { steps: 12 },
  );
  await page.mouse.up();
}

/**
 * Pins the one word production takes from Web Crypto, so the real WASM
 * generator produces the same sequence every run.
 *
 * The caller has to reload afterwards: the script is installed for documents
 * created from here on.
 */
async function pinSeed(page: Page, seed: number): Promise<void> {
  await page.addInitScript(`
    const originalGetRandomValues = Crypto.prototype.getRandomValues;
    Crypto.prototype.getRandomValues = function(array) {
      if (array instanceof Uint32Array && array.length === 1) {
        array[0] = ${seed};
        return array;
      }
      return originalGetRandomValues.call(this, array);
    };
  `);
}

/** Sets the scramble length, the way a person changing the box would. */
async function setScrambleMoves(page: Page, count: number): Promise<void> {
  const moves = page.locator('#scramble-moves');
  await moves.fill(String(count));
  await moves.dispatchEvent('change');
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('scramble starts a session on first committed move and Reset restores it', async ({
  page,
}) => {
  const root = page.locator('.game-shell');
  const timer = page.locator('#timer');

  await expect(root).toHaveAttribute('data-game-state', 'idle');
  await expect(timer).toHaveText('00:00.00');
  const solved = await probeCanvas(page);
  assertSceneContract(solved);

  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'ready');
  await expect(timer).toHaveText('00:00.00');

  // The scramble reached both views, not just the gameplay state.
  const scrambled = await probeCanvas(page);
  expect(scrambled.net).not.toEqual(solved.net);
  expect(scrambled.cubeGrid).not.toEqual(solved.cubeGrid);

  // A programmatic R turn uses the same animation and commit path as a drag.
  await page.keyboard.press('r');
  await expect(root).toHaveAttribute('data-game-state', 'running');
  await expect.poll(async () => page.locator('#timer').textContent()).not.toBe(
    '00:00.00',
  );

  await page.locator('#reset').click();
  await expect(root).toHaveAttribute('data-game-state', 'idle');
  await expect(timer).toHaveText('00:00.00');
  assertSceneContract(await probeCanvas(page));
});

test('Both is default and 3D, 2D, and Home view controls preserve gameplay', async ({
  page,
}) => {
  const canvas = page.locator('#view');
  const both = page.locator('[data-view="both"]');
  const cube = page.locator('[data-view="3d"]');
  const net = page.locator('[data-view="2d"]');

  await expect(both).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', 'both');

  await cube.click();
  await expect(cube).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', '3d');

  await net.click();
  await expect(net).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', '2d');

  // The net is something to work on rather than to look at: a drag across a
  // cell turns a layer here just as one on the cube does.
  const before = await canvas.evaluate(
    (element) => (element as HTMLCanvasElement).toDataURL(),
  );
  const box = await canvas.boundingBox();
  expect(box).not.toBeNull();
  await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
  await page.mouse.down();
  await page.mouse.move(box!.x + box!.width * 0.75, box!.y + box!.height / 2);
  await page.mouse.up();
  await expect
    .poll(async () =>
      canvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL()),
    )
    .not.toBe(before);

  // Put the cube back, so what follows is about the view controls alone.
  await page.locator('#reset').click();

  await both.click();
  await orbitOnce(page);
  const orbited = await canvas.evaluate(
    (element) => (element as HTMLCanvasElement).toDataURL(),
  );
  await page.locator('#home-view').click();
  expect(
    await canvas.evaluate(
      (element) => (element as HTMLCanvasElement).toDataURL(),
    ),
  ).not.toBe(orbited);
  assertSceneContract(await probeCanvas(page));
});

test('desktop action groups sit outside opposite canvas edges', async ({
  page,
}) => {
  const canvas = await page.locator('#view').boundingBox();
  const viewSwitch = await page.locator('.view-switch').boundingBox();
  const gameActions = await page.locator('.game-actions').boundingBox();

  expect(canvas).not.toBeNull();
  expect(viewSwitch).not.toBeNull();
  expect(gameActions).not.toBeNull();
  expect(viewSwitch!.x + viewSwitch!.width).toBeLessThan(canvas!.x);
  expect(gameActions!.x).toBeGreaterThan(canvas!.x + canvas!.width);
});

test('a scramble is turned into the cube where it can be watched', async ({
  page,
}) => {
  const root = page.locator('.game-shell');
  const moveButtons = page.locator('[data-face]');
  const solved = await probeCanvas(page);

  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'scrambling');

  // The cut face of a cubie is only ever drawn while a layer is part way
  // round, so finding it is finding the cube mid-turn rather than merely
  // changed. Nothing can be turned by hand for as long as that lasts.
  await expect(moveButtons.first()).toBeDisabled();
  await expect(page.locator('#timer')).toHaveText('00:00.00');
  await expect
    .poll(
      async () => {
        const probe = await probeCanvas(page);
        return probe.cubeGrid.some(
          (pixel) => pixel.join() === BODY.join(),
        );
      },
      { timeout: 5000 },
    )
    .toBe(true);

  await expect(root).toHaveAttribute('data-game-state', 'ready');
  await expect(moveButtons.first()).toBeEnabled();

  // And it arrived: both views moved, and the clock is waiting rather than
  // running.
  const scrambled = await probeCanvas(page);
  expect(scrambled.net).not.toEqual(solved.net);
  expect(scrambled.cubeGrid).not.toEqual(solved.cubeGrid);
  await expect(page.locator('#timer')).toHaveText('00:00.00');
});

test('a drag over the cube while a scramble plays only sweeps the view', async ({
  page,
}) => {
  await pinSeed(page, 42);
  await page.reload();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const root = page.locator('.game-shell');

  // One scramble with a drag pulled straight across a sticker in the middle
  // of it -- the grip that takes hold of a layer at any other moment.
  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'scrambling');

  const probe = await probeCanvas(page);
  const grab = pagePointInCube(probe, FRONT_RIGHT_COLUMN);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(
    grab.x - QUARTER_TURN_DRAG * probe.box.width,
    grab.y,
    { steps: 12 },
  );
  await page.mouse.up();
  await expect(root).toHaveAttribute('data-game-state', 'ready');

  // The drag did something: the viewpoint is no longer where Home puts it.
  const swept = await probeCanvas(page);
  await page.locator('#home-view').click();
  const dragged = await probeCanvas(page);
  expect(dragged.cubeGrid).not.toEqual(swept.cubeGrid);

  // And it did nothing else. The same seed again, untouched, has to land on
  // the same cube -- an extra turn taken from under the finger would not.
  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'ready');
  const clean = await probeCanvas(page);

  expect(dragged.net).toEqual(clean.net);
  expect(dragged.cubeGrid).toEqual(clean.cubeGrid);
});

test('a fixed scramble solved through keyboard controls stops the timer', async ({
  page,
}) => {
  await pinSeed(page, 42);
  await page.reload();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  // Three moves rather than twenty. This sequence has to be taken from the
  // generator by hand, and the length only decides how much of that there is
  // to redo whenever the generator changes -- what the test is here for is
  // the path from a scrambled cube to a stopped clock, which three moves
  // walk exactly as well as twenty.
  await setScrambleMoves(page, 3);
  await page.locator('#scramble').click();

  // The scramble is turned into the cube rather than applied, so the keys
  // below have to wait for it: a layer cannot be turned by hand until then.
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  // Seed 42 begins L, D, L'. Undoing it runs backwards: L, D', L'.
  const solution = ['l', 'D', 'L'];
  const moveButtons = page.locator('[data-face]');

  for (const key of solution) {
    const shortcut = key === key.toUpperCase() ? `Shift+${key}` : key;
    await page.keyboard.press(shortcut);
    await expect(moveButtons.first()).toBeDisabled();
    await expect(moveButtons.first()).toBeEnabled();
  }

  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'completed',
  );
  await expect(page.locator('#status')).toContainText('Solved in');
  await expect(page.locator('#timer')).not.toHaveText('00:00.00');
});

test.describe('on a phone', () => {
  test.use({ hasTouch: true });

  test('taps alone run scramble, a turn, and reset', async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    const root = page.locator('.game-shell');
    const timer = page.locator('#timer');

    await page.locator('#scramble').tap();
    await expect(root).toHaveAttribute('data-game-state', 'ready');

    // The move controls are collapsed until asked for, on every viewport.
    await page.locator('.move-controls summary').tap();
    await page.locator('[data-face="r"][data-turn="1"]').tap();

    await expect(root).toHaveAttribute('data-game-state', 'running');
    await expect
      .poll(async () => timer.textContent())
      .not.toBe('00:00.00');

    await page.locator('#reset').tap();
    await expect(root).toHaveAttribute('data-game-state', 'idle');
    await expect(timer).toHaveText('00:00.00');
    assertSceneContract(await probeCanvas(page));
  });
});

test('mobile layout moves actions below the canvas without horizontal overflow', async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 844 });

  const canvas = await page.locator('#view').boundingBox();
  const actions = await page.locator('.game-actions').boundingBox();
  expect(canvas).not.toBeNull();
  expect(actions).not.toBeNull();
  expect(actions!.y).toBeGreaterThanOrEqual(canvas!.y + canvas!.height);

  const geometry = await page.evaluate(() => ({
    viewport: document.documentElement.clientWidth,
    content: document.documentElement.scrollWidth,
  }));
  expect(geometry.content).toBeLessThanOrEqual(geometry.viewport);

  for (const id of ['#scramble', '#reset', '#home-view']) {
    const box = await page.locator(id).boundingBox();
    expect(box).not.toBeNull();
    expect(box!.height).toBeGreaterThanOrEqual(44);
  }
});

/**
 * The middles of every sticker of one solved face, in page coordinates.
 *
 * Connected components rather than anything looser: net cells sit a few pixels
 * apart and ring stickers a good deal further, so no single "near enough"
 * radius separates the one without splitting the other.
 */
async function faceStickers(
  page: Page,
  color: readonly number[],
): Promise<{ x: number; y: number }[]> {
  return page.evaluate((wanted) => {
    const canvas = document.querySelector('canvas')!;
    const context = canvas.getContext('2d')!;
    const data = context.getImageData(0, 0, canvas.width, canvas.height).data;

    const step = 3;
    const cols = Math.floor(canvas.width / step);
    const rows = Math.floor(canvas.height / step);
    const on = new Uint8Array(cols * rows);
    for (let r = 0; r < rows; r += 1) {
      for (let c = 0; c < cols; c += 1) {
        const i = (r * step * canvas.width + c * step) * 4;
        on[r * cols + c] =
          data[i] === wanted[0] &&
          data[i + 1] === wanted[1] &&
          data[i + 2] === wanted[2]
            ? 1
            : 0;
      }
    }

    const found: { x: number; y: number }[] = [];
    for (let seed = 0; seed < on.length; seed += 1) {
      if (on[seed] === 0) continue;

      let n = 0;
      let sx = 0;
      let sy = 0;
      const stack = [seed];
      on[seed] = 0;
      while (stack.length > 0) {
        const at = stack.pop()!;
        const c = at % cols;
        const r = (at - c) / cols;
        n += 1;
        sx += c;
        sy += r;
        for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const nc = c + dc;
          const nr = r + dr;
          if (nc < 0 || nr < 0 || nc >= cols || nr >= rows) continue;
          if (on[nr * cols + nc] === 0) continue;
          on[nr * cols + nc] = 0;
          stack.push(nr * cols + nc);
        }
      }
      if (n < 12) continue;

      const box = canvas.getBoundingClientRect();
      found.push({
        x: box.left + (((sx / n) * step) * box.width) / canvas.width,
        y: box.top + (((sy / n) * step) * box.height) / canvas.height,
      });
    }
    return found;
  }, color);
}

test('the flat view toggles between net and rings in every region mode', async ({
  page,
}) => {
  const canvas = page.locator('#view');
  const both = page.locator('[data-view="both"]');
  const cube = page.locator('[data-view="3d"]');
  const flat = page.locator('[data-view="2d"]');
  const net = page.locator('[data-flat="net"]');
  const rings = page.locator('[data-flat="rings"]');

  const frame = async (): Promise<string> =>
    canvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());

  await expect(canvas).toHaveAttribute('data-view-mode', 'both');
  await expect(canvas).toHaveAttribute('data-flat-style', 'net');
  const bothNet = await frame();

  // Both follows the toggle, so the cube keeps its place and the drawing
  // under it changes.
  await rings.click();
  await expect(rings).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', 'both');
  await expect(canvas).toHaveAttribute('data-flat-style', 'rings');
  const bothRings = await frame();
  expect(bothRings).not.toBe(bothNet);

  // The style is the other axis: it survives the flat region going away and
  // coming back, and the toggle is out of the way while it means nothing.
  await cube.click();
  await expect(rings).toBeHidden();
  await flat.click();
  await expect(rings).toBeVisible();
  await expect(rings).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-flat-style', 'rings');

  await net.click();
  await expect(canvas).toHaveAttribute('data-flat-style', 'net');
  await both.click();
  expect(await frame()).toBe(bothNet);
});

test('the flat view can show the net over the rings, and both take drags', async ({
  page,
}) => {
  const canvas = page.locator('#view');

  await page.locator('[data-view="2d"]').click();
  await page.locator('[data-flat="both"]').click();
  await expect(canvas).toHaveAttribute('data-flat-style', 'both');

  // Nine white stickers in each drawing, the net's above the rings'.
  const white = await faceStickers(page, WHITE);
  expect(white).toHaveLength(18);

  const split = Math.max(...white.map((s) => s.y)) / 2;
  const onRings = white
    .filter((s) => s.y > split)
    .sort((a, b) => a.x - b.x || a.y - b.y);
  expect(onRings).toHaveLength(9);

  const frame = async (): Promise<string> =>
    canvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());
  const resting = await frame();

  // A drag on the lower drawing turns the cube, so the upper one moves too:
  // one gesture, one rotation, every view that is up showing it.
  await page.mouse.move(onRings[0]!.x, onRings[0]!.y);
  await page.mouse.down();
  await page.mouse.move(onRings[4]!.x, onRings[4]!.y, { steps: 16 });
  await page.mouse.up();

  await expect.poll(frame).not.toBe(resting);
  await expect
    .poll(async () => (await faceStickers(page, WHITE)).length)
    .toBe(18);

  await page.locator('#reset').click();
});

test('dragging a sticker round its ring turns the cube', async ({ page }) => {
  const canvas = page.locator('#view');

  await page.locator('[data-view="2d"]').click();
  await page.locator('[data-flat="rings"]').click();
  await expect(canvas).toHaveAttribute('data-flat-style', 'rings');

  const frame = async (): Promise<string> =>
    canvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());
  const resting = await frame();

  // The nine stickers of one face sit on a three-by-three patch of crossings,
  // so two of them a step apart share a ring: dragging between them is a drag
  // along that ring, and two slots is past the point a release commits.
  const white = await faceStickers(page, WHITE);
  expect(white).toHaveLength(9);
  white.sort((a, b) => a.x - b.x || a.y - b.y);

  await page.mouse.move(white[0]!.x, white[0]!.y);
  await page.mouse.down();
  await page.mouse.move(white[4]!.x, white[4]!.y, { steps: 16 });
  await page.mouse.up();

  await expect.poll(frame).not.toBe(resting);

  // Every sticker is back on a slot once it settles, which is what makes the
  // settled picture one the resting builder could have drawn: nine of each
  // color, none of them part way round a loop.
  await expect
    .poll(async () => (await faceStickers(page, WHITE)).length)
    .toBe(9);

  await page.locator('#reset').click();
});
