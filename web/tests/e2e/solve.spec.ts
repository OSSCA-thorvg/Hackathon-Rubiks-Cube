import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  expectedNet,
  probeCanvas,
} from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/**
 * Waits for whatever is playing to reach its end.
 *
 * Generous, because a whole solution is the longest thing this application
 * ever plays: a hundred and something moves at the tempo a rewind uses is the
 * better part of half a minute.
 */
async function settled(page: Page): Promise<void> {
  await expect(page.locator('#stop')).toBeHidden({ timeout: 60_000 });
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('a scrambled cube is worked out and played back solved', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const solve = page.locator('#solve');

  // Nothing to solve on a cube nobody has touched.
  await expect(solve).toBeDisabled();

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  const scrambled = await probeCanvas(page);

  await expect(solve).toBeEnabled();
  await solve.click();
  await expect(page.locator('#status')).toContainText('Press Stop');
  await expect(page.locator('#stop')).toBeVisible();

  await settled(page);

  // The cube on screen is the solved one, read off the drawing rather than
  // off the engine: six faces of one colour each.
  const solvedNow = await probeCanvas(page);
  expect(solvedNow.net).not.toEqual(scrambled.net);
  assertSceneContract(solvedNow);

  await expect(shell).toHaveAttribute('data-game-state', 'completed');
  await expect(page.locator('#status')).toHaveText(
    'Solved by the solver. Not a solve of your own.',
  );

  // A sitting the solver finished is not one the records keep.
  await expect(page.locator('#record-best')).toHaveText('No solves yet.');
  await expect(page.locator('#record-list').locator('li')).toHaveCount(0);

  // And the moves it made are on the record like any others, so they can be
  // taken back off one at a time.
  await expect(page.locator('#undo')).toBeEnabled();
});

test('a solve broken off is picked up again by redo', async ({ page }) => {
  const stop = page.locator('#stop');

  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  await page.locator('#solve').click();
  await expect(stop).toBeVisible();
  await stop.click();
  await expect(page.locator('#status')).toHaveText('Stopped.');
  await settled(page);

  // Stopped part way, so the cube is not solved and the rest of the solution
  // is still written above the cursor waiting to be replayed.
  expect((await probeCanvas(page)).net).not.toEqual(expectedNet());
  await expect(page.locator('#redo')).toBeEnabled();

  const redo = page.locator('#redo');
  for (let step = 0; step < 3; step += 1) {
    await redo.click();
    await settled(page);
  }
  await expect(redo).toBeEnabled();
});

test('a cube with no solver says so, and can still be rewound', async ({
  page,
}) => {
  const note = page.locator('#solver-note');
  const size = page.locator('#cube-size');

  await expect(note).toBeHidden();

  await size.fill('4');
  await size.blur();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  await expect(note).toBeVisible();
  await expect(page.locator('#solve')).toBeDisabled();

  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  // Still no solver for this cube, and the one command that never needed one
  // is exactly where it was.
  await expect(page.locator('#solve')).toBeDisabled();
  await expect(page.locator('#rewind')).toBeEnabled();

  await page.locator('#rewind').click();
  await settled(page);
  expect((await probeCanvas(page, 4)).net).toEqual(expectedNet(4));

  await size.fill('3');
  await size.blur();
  await expect(note).toBeHidden();
});
