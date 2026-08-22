import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  expectedNet,
  probeCanvas,
} from './sceneContract.ts';
import { fillInSettings } from './shell.ts';

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

test('a bigger cube is worked out too', async ({ page }) => {
  const shell = page.locator('.game-shell');
  const solve = page.locator('#solve');

  // Four is the smallest cube that has to be turned into a three by three
  // before it can be solved, so this is the reduction running in a browser.
  await fillInSettings(page, '#cube-size', '4');
  await expect(shell).toHaveAttribute('data-game-state', 'idle');

  // At the tempo a rewind uses, a reduction and the stages after it are
  // several hundred moves and the better part of a minute. The speed control
  // is the application's own answer to that, and it is what a person watching
  // this would reach for.
  await fillInSettings(page, '#speed', '4');
  await expect(page.locator('#speed-value')).toHaveText('4.00×');

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready', {
    timeout: 60_000,
  });
  const scrambled = await probeCanvas(page, 4);

  await expect(solve).toBeEnabled();
  await solve.click();
  await settled(page);

  // Read off the drawing rather than off the engine: sixteen cells of one
  // colour on each of the six faces of the net.
  const solvedNow = await probeCanvas(page, 4);
  expect(solvedNow.net).not.toEqual(scrambled.net);
  expect(solvedNow.net).toEqual(expectedNet(4));
  await expect(shell).toHaveAttribute('data-game-state', 'completed');
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

test('the biggest cube has a solver too, and Rewind is still its own', async ({
  page,
}) => {
  const note = page.locator('#solver-note');

  await expect(note).toBeHidden();

  // Nine used to be the size with nothing to solve it, which is what this
  // test was written for. Every size this application builds has a solver
  // now, so what is checked is that -- and that Rewind, which never needed
  // one, is where it was.
  await fillInSettings(page, '#cube-size', '9');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(note).toBeHidden();

  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
    { timeout: 60_000 },
  );

  await expect(page.locator('#solve')).toBeEnabled();
  await expect(page.locator('#rewind')).toBeEnabled();

  await page.locator('#rewind').click();
  await settled(page);
  expect((await probeCanvas(page, 9)).net).toEqual(expectedNet(9));

  await fillInSettings(page, '#cube-size', '3');
  await expect(note).toBeHidden();
});
