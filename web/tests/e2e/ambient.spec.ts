import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  cubeDragFor,
  FRONT_RIGHT_COLUMN,
  pagePointInCube,
  probeCanvas,
} from './sceneContract.ts';
import { openPanel, pressInSettings, stageSettled } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Waits until the pattern has visibly turned the cube away from where it was. */
async function waitForMotion(page: Page, from: number[][]): Promise<void> {
  await expect
    .poll(
      async () => {
        const probe = await probeCanvas(page);
        return (
          JSON.stringify(probe.cubeGrid) !== JSON.stringify(from) ? 1 : 0
        );
      },
      { timeout: 5000 },
    )
    .toBe(1);
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('watching turns the cube by itself, and the toggle puts it back', async ({
  page,
}) => {
  const watch = page.locator('#ambient');
  const solved = await probeCanvas(page);
  assertSceneContract(solved);

  await watch.click();
  await expect(watch).toHaveAttribute('aria-pressed', 'true');
  await expect(page.locator('#status')).toContainText('Look around freely');

  // Nobody asked for these turns and nobody is timing them.
  await waitForMotion(page, solved.cubeGrid);
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'idle',
  );
  await expect(page.locator('#timer')).toHaveText('00:00.00');

  await watch.click();
  await expect(watch).toHaveAttribute('aria-pressed', 'false');
  await expect(page.locator('#status')).toHaveText('Watching stopped.');

  // Everything is where it was before watching began, down to the viewpoint.
  await expect
    .poll(async () => JSON.stringify((await probeCanvas(page)).cubeGrid))
    .toBe(JSON.stringify(solved.cubeGrid));
  assertSceneContract(await probeCanvas(page));
});

test('a drag looks around a watched pattern instead of ending it', async ({
  page,
}) => {
  const watch = page.locator('#ambient');
  const solved = await probeCanvas(page);

  await watch.click();
  await waitForMotion(page, solved.cubeGrid);

  // A grip on a sticker: at any other moment this takes hold of a layer, and
  // here it sweeps the viewpoint, exactly as it does over a scramble.
  const spinning = await probeCanvas(page);
  const grab = pagePointInCube(spinning, FRONT_RIGHT_COLUMN);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(
    grab.x - cubeDragFor(spinning, 1),
    grab.y,
    { steps: 12 },
  );
  await page.mouse.up();

  // Still watching, and still turning: the drag was a way of looking at it.
  await expect(watch).toHaveAttribute('aria-pressed', 'true');
  await expect(page.locator('#status')).toContainText('Look around freely');
  await waitForMotion(page, (await probeCanvas(page)).cubeGrid);

  // The viewpoint is where the drag left it, so the cube handed back is the
  // one from before -- seen from somewhere else.
  await watch.click();
  const swept = await probeCanvas(page);
  expect(swept.cubeGrid).not.toEqual(solved.cubeGrid);
  expect(swept.net).toEqual(solved.net);

  await pressInSettings(page, '#home-view');
  assertSceneContract(await probeCanvas(page));
});

test('switching views leaves a watched pattern running, as it does a scramble', async ({
  page,
}) => {
  const stage = page.locator('#stage');
  const watch = page.locator('#ambient');
  const solved = await probeCanvas(page);

  await watch.click();
  await waitForMotion(page, solved.cubeGrid);

  // The same controls, over the same kind of playback, twice: what they do to
  // a scramble part way through is what they have to do to a pattern.
  for (const selector of ['[data-view="3d"]', '[data-view="2d"]',
                          '[data-flat="rings"]', '[data-view="both"]']) {
    await page.locator(selector).click();
    await expect(watch).toHaveAttribute('aria-pressed', 'true');
  }

  // Home view is a presentation reset rather than a view mode, so it sits in
  // Settings now. It has to leave a watched pattern running all the same.
  await pressInSettings(page, '#home-view');
  await expect(watch).toHaveAttribute('aria-pressed', 'true');

  await expect(stage).toHaveAttribute('data-view-mode', 'both');
  await expect(stage).toHaveAttribute('data-flat-style', 'rings');

  // Still turning after all of it, and still with nothing of its own left
  // behind once it is stopped.
  await waitForMotion(page, (await probeCanvas(page)).cubeGrid);
  await watch.click();
  await page.locator('[data-flat="net"]').click();
  await expect
    .poll(async () => JSON.stringify((await probeCanvas(page)).cubeGrid))
    .toBe(JSON.stringify(solved.cubeGrid));
  assertSceneContract(await probeCanvas(page));
});

test('a scramble watched over is still the scramble that was made', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const watch = page.locator('#ambient');

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');

  // A solve that is armed is a solve under way, so there is nothing to press.
  await expect(watch).toBeDisabled();

  // Turning it into a finished one puts the offer back.
  await page.keyboard.press('r');
  await expect(shell).toHaveAttribute('data-game-state', 'running');
  await expect(watch).toBeDisabled();

  await pressInSettings(page, '#reset');
  await expect(shell).toHaveAttribute('data-game-state', 'idle');
  await expect(watch).toBeEnabled();
});

test('pressing Watch again stops it rather than starting it over', async ({
  page,
}) => {
  const watch = page.locator('#ambient');
  const solved = await probeCanvas(page);

  await watch.click();
  await waitForMotion(page, solved.cubeGrid);

  await watch.click();
  await expect(watch).toHaveAttribute('aria-pressed', 'false');

  // Stopped and stayed stopped: a cube that had been started again would be
  // turning, and this one has to be exactly the one from before.
  await expect
    .poll(async () => JSON.stringify((await probeCanvas(page)).cubeGrid))
    .toBe(JSON.stringify(solved.cubeGrid));
  await page.waitForTimeout(600);
  assertSceneContract(await probeCanvas(page));
});

test('a move button ends watching and turns the cube it gave back', async ({
  page,
}) => {
  const watch = page.locator('#ambient');

  // The panel the buttons are in first: opening it makes room beside the
  // stage, and the cube is read at the size it will be read at afterwards.
  await openPanel(page, 'turn');
  await stageSettled(page);
  const solved = await probeCanvas(page);

  await watch.click();
  await waitForMotion(page, solved.cubeGrid);

  // The move buttons stay live through watching, because pressing one is a
  // way out of it -- the same as the letter it carries on the keyboard.
  const right = page.locator('[data-face="r"][data-turn="1"]');
  await expect(right).toBeEnabled();
  await right.click();

  await expect(watch).toHaveAttribute('aria-pressed', 'false');
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'idle',
  );

  // One turn of the user's own on the restored cube, and nothing of the
  // pattern: taking that turn back leaves the cube it was watched from.
  await page.locator('[data-face="r"][data-turn="-1"]').click();
  await expect
    .poll(async () => JSON.stringify((await probeCanvas(page)).cubeGrid))
    .toBe(JSON.stringify(solved.cubeGrid));
  assertSceneContract(await probeCanvas(page));
});
