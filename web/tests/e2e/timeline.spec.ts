import { expect, test, type Page } from '@playwright/test';

import { probeCanvas } from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Turns one face from the keyboard and waits for it to settle. */
async function turn(page: Page, key: string): Promise<void> {
  const moveButtons = page.locator('[data-face]');
  await page.keyboard.press(key);
  await expect(moveButtons.first()).toBeDisabled();
  await expect(moveButtons.first()).toBeEnabled();
}

/** Waits for a walk along the record to arrive where it was going. */
async function walked(page: Page, progress: string): Promise<void> {
  await expect(page.locator('#timeline-progress')).toHaveText(progress);
  await expect(page.locator('#stop')).toBeHidden();
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('a move pressed on the timeline takes the cube back to it, and forward again', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const strip = page.locator('#timeline-moves button');

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  await expect(page.locator('#timeline-scramble')).toHaveText('Scramble · 20');

  await turn(page, 'r');
  const afterFirst = await probeCanvas(page);
  await turn(page, 'u');
  await turn(page, 'f');
  const afterThird = await probeCanvas(page);
  await expect(page.locator('#timeline-progress')).toHaveText('3 / 3');
  await expect(strip).toHaveCount(3);

  // Back to the first: the two after it come off one at a time, and the
  // cube is exactly the one that first move left.
  await strip.nth(0).click();
  await walked(page, '1 / 3');
  expect((await probeCanvas(page)).net).toEqual(afterFirst.net);
  await expect(page.locator('#move-log li')).toHaveText(['R', 'U', 'F']);
  await expect(page.locator('#move-log li').nth(0)).toHaveAttribute(
    'data-state',
    'current',
  );
  await expect(
    page.locator('#move-log li[data-state="pending"]'),
  ).toHaveCount(2);

  // And forward to the last, putting both back.
  await strip.nth(2).click();
  await walked(page, '3 / 3');
  expect((await probeCanvas(page)).net).toEqual(afterThird.net);
});

test('typed moves play from the command menu, one turn after another', async ({
  page,
}) => {
  const palette = page.locator('#command-palette');

  await page.keyboard.press('ControlOrMeta+k');
  await expect(palette).toBeVisible();
  await expect(page.locator('#command-input')).toBeFocused();

  // Read back in the log's own spelling before anything is played.
  await page.locator('#command-input').fill("r u r' u'");
  const play = page.locator('#command-list [role="option"]').first();
  await expect(play).toContainText("R U R' U'");
  await expect(play).toContainText('4 moves');

  await page.keyboard.press('Enter');
  await expect(palette).toBeHidden();
  await expect(page.locator('#move-log li')).toHaveText(['R', 'U', "R'", "U'"]);
  await walked(page, '4 / 4');
});

test('the command menu runs the page commands and says what it cannot read', async ({
  page,
}) => {
  await page.locator('#command-trigger').click();
  await expect(page.locator('#command-palette')).toBeVisible();

  // A move the cube in hand does not have is refused with the reason.
  await page.locator('#command-input').fill('4R');
  await expect(page.locator('#command-message')).toHaveText(
    '"4R" reaches past a 3×3.',
  );

  // A command is found by its words and run as a press on its button --
  // "2d" is a slice of D as well, and still finds the view first.
  await page.locator('#command-input').fill('2d');
  await expect(
    page.locator('#command-list [role="option"]').first(),
  ).toContainText('View: 2D');
  await page.keyboard.press('Enter');
  await expect(page.locator('#command-palette')).toBeHidden();
  await expect(page.locator('#stage')).toHaveAttribute('data-view-mode', '2d');

  // Escape closes it without running anything.
  await page.keyboard.press('ControlOrMeta+k');
  await page.locator('#command-input').fill('scramble');
  await page.keyboard.press('Escape');
  await expect(page.locator('#command-palette')).toBeHidden();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'idle',
  );
});
