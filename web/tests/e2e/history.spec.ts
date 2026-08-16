import { expect, test, type Page } from '@playwright/test';

import { assertSceneContract, probeCanvas } from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Turns one face from the keyboard and waits for it to settle. */
async function turn(page: Page, key: string): Promise<void> {
  const moveButtons = page.locator('[data-face]');
  await page.keyboard.press(key);
  await expect(moveButtons.first()).toBeDisabled();
  await expect(moveButtons.first()).toBeEnabled();
}

/**
 * Waits for a rewind to finish playing.
 *
 * Rewind is the signal rather than Undo: it is offered for as long as anything
 * at all is applied, so it comes back whichever end of the record the rewind
 * stopped at, while Undo may legitimately stay out.
 */
async function rewindSettled(page: Page): Promise<void> {
  await expect(page.locator('#stop')).toBeHidden();
  await expect(page.locator('#rewind')).toBeEnabled();
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('moves are taken back one at a time, put back, and rewound away', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const undo = page.locator('#undo');
  const redo = page.locator('#redo');
  const rewind = page.locator('#rewind');

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  const scrambled = await probeCanvas(page);

  // A scramble is not the user's to take back, and there is nothing that has
  // been taken back to put again -- but the whole of it can be rewound.
  await expect(undo).toBeDisabled();
  await expect(redo).toBeDisabled();
  await expect(rewind).toBeEnabled();

  await turn(page, 'r');
  const afterFirst = await probeCanvas(page);
  await turn(page, 'u');
  const afterSecond = await probeCanvas(page);
  await turn(page, 'f');
  const afterThird = await probeCanvas(page);
  await expect(shell).toHaveAttribute('data-game-state', 'running');
  expect(afterThird.net).not.toEqual(afterSecond.net);

  await undo.click();
  await rewindSettled(page);
  await undo.click();
  await rewindSettled(page);

  // Exactly the cube from after the first move: the moves were turned back
  // rather than made again, which would have landed somewhere else entirely.
  const rewound = await probeCanvas(page);
  expect(rewound.net).toEqual(afterFirst.net);
  expect(rewound.cubeGrid).toEqual(afterFirst.cubeGrid);

  // And the two that came off are waiting, in the order they were made.
  await expect(redo).toBeEnabled();
  await redo.click();
  await rewindSettled(page);
  expect((await probeCanvas(page)).net).toEqual(afterSecond.net);

  // Down to the end of the scramble and no further: what is below that line
  // is not the user's own. Two moves are on the cube here, so it takes two.
  await undo.click();
  await rewindSettled(page);
  await undo.click();
  await rewindSettled(page);
  await expect(undo).toBeDisabled();
  expect((await probeCanvas(page)).net).toEqual(scrambled.net);

  await rewind.click();
  await expect(shell).toHaveAttribute('data-game-state', 'completed');
  await expect(rewind).toBeDisabled();

  // A cube the engine rewound rather than one the user finished, and the
  // status line says so -- the clock stops either way.
  await expect(page.locator('#status')).toContainText(
    'Not a solve of your own',
  );
  await expect(page.locator('#timer')).not.toHaveText('00:00.00');
  assertSceneContract(await probeCanvas(page));
});

test('a rewind can be broken off, and picked up again from there', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const stop = page.locator('#stop');
  const rewind = page.locator('#rewind');

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  const scrambled = await probeCanvas(page);

  // Nothing is being rewound, so there is nothing to break off.
  await expect(stop).toBeHidden();

  await rewind.click();
  await expect(stop).toBeVisible();
  await expect(page.locator('#status')).toContainText('Press Stop');

  // Once some of the scramble has visibly come off, so that what is being
  // broken off is a rewind under way rather than one that has not begun.
  await expect
    .poll(async () => JSON.stringify((await probeCanvas(page)).net))
    .not.toBe(JSON.stringify(scrambled.net));

  await stop.click();
  await expect(stop).toBeHidden();
  await expect(page.locator('#status')).toHaveText('Stopped.');

  // Part way: some of the scramble has come off and the rest has not, and it
  // stays where it stopped rather than carrying on underneath.
  //
  // The wait is for the drawing rather than the state: stopping confirms the
  // turn that was in flight at once, and the frame that draws it landed comes
  // after -- so a picture taken immediately would catch a layer part way
  // round and disagree with the settled one below.
  await rewindSettled(page);
  await page.waitForTimeout(300);
  const halted = await probeCanvas(page);
  expect(halted.net).not.toEqual(scrambled.net);
  await page.waitForTimeout(600);
  expect((await probeCanvas(page)).net).toEqual(halted.net);

  // The rest of the way from exactly there.
  await rewind.click();
  await expect(shell).toHaveAttribute('data-game-state', 'completed');
  assertSceneContract(await probeCanvas(page));
});
