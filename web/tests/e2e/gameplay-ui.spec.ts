import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  pagePointInCube,
  probeCanvas,
  QUARTER_TURN_DRAG,
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

test('Both is default and 3D, Net, and Home view controls preserve gameplay', async ({
  page,
}) => {
  const canvas = page.locator('#view');
  const both = page.locator('[data-view="both"]');
  const cube = page.locator('[data-view="3d"]');
  const net = page.locator('[data-view="net"]');

  await expect(both).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', 'both');

  await cube.click();
  await expect(cube).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', '3d');

  await net.click();
  await expect(net).toHaveAttribute('aria-pressed', 'true');
  await expect(canvas).toHaveAttribute('data-view-mode', 'net');

  // Net is display-only: a canvas drag is declined and changes no pixels.
  const before = await canvas.evaluate(
    (element) => (element as HTMLCanvasElement).toDataURL(),
  );
  const box = await canvas.boundingBox();
  expect(box).not.toBeNull();
  await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
  await page.mouse.down();
  await page.mouse.move(box!.x + box!.width * 0.75, box!.y + box!.height / 2);
  await page.mouse.up();
  expect(
    await canvas.evaluate(
      (element) => (element as HTMLCanvasElement).toDataURL(),
    ),
  ).toBe(before);

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

test('a fixed scramble solved through keyboard controls stops the timer', async ({
  page,
}) => {
  // Production uses Web Crypto. Pin its one-word seed before reloading so the
  // real WASM generator produces the native known-answer sequence for 42.
  await page.addInitScript(`
    const originalGetRandomValues = Crypto.prototype.getRandomValues;
    Crypto.prototype.getRandomValues = function(array) {
      if (array instanceof Uint32Array && array.length === 1) {
        array[0] = 42;
        return array;
      }
      return originalGetRandomValues.call(this, array);
    };
  `);
  await page.reload();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
  await page.locator('#scramble').click();

  // Reverse inverse of seed 42. A half turn is two same-direction keyboard
  // quarter turns because the visible controls intentionally expose ±90°.
  const solution = [
    'b', 'b', 'u', 'u', 'B', 'U', 'F', 'U', 'l', 'l', 'B', 'u', 'L',
    'f', 'r', 'b', 'b', 'D', 'B', 'l', 'b', 'l', 'D', 'L',
  ];
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
