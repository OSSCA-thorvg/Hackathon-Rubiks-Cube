import { expect, test, type Page } from '@playwright/test';
import { fillInSettings, pressInSettings } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Long enough that the difference between speeds is far outside the noise. */
const SCRAMBLE_MOVES = 20;

/** Sets the slider and waits for the reading beside it to agree. */
async function setSpeed(page: Page, scale: string): Promise<void> {
  await fillInSettings(page, '#speed', scale);
  await expect(page.locator('#speed-value')).toHaveText(
    `${Number(scale).toFixed(2)}×`,
  );
}

/** Runs one scramble and returns how long it took to finish playing. */
async function timeScramble(page: Page): Promise<number> {
  await fillInSettings(page, '#scramble-moves', String(SCRAMBLE_MOVES));

  const started = Date.now();
  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
    { timeout: 60_000 },
  );
  return Date.now() - started;
}

test.use({ viewport: DESKTOP });

test('the slider changes how long a played sequence takes', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  await expect(page.locator('#speed-value')).toHaveText('1.00×');

  const slow = await timeScramble(page);

  await pressInSettings(page, '#reset');
  await setSpeed(page, '4');
  const fast = await timeScramble(page);

  // Wall-clock in a browser is not a stopwatch, so the claim is only that four
  // times faster is plainly faster -- the ratio itself is pinned by the fixed
  // step native tests, where a frame is exactly a frame.
  expect(fast).toBeLessThan(slow * 0.75);
});

test('the slider holds the engine s range, and stays live while playing', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  // The ends of the range the engine clamps to, shown as the control's own.
  await expect(page.locator('#speed')).toHaveAttribute('min', '0.25');
  await expect(page.locator('#speed')).toHaveAttribute('max', '4');

  await setSpeed(page, '0.25');

  await fillInSettings(page, '#scramble-moves', '3');
  await page.locator('#scramble').click();

  // Changed mid-sequence: like the palette, this is not a command to the cube,
  // and the turn already running keeps the duration it began with.
  await expect(page.locator('#speed')).toBeEnabled();
  await setSpeed(page, '4');

  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
    { timeout: 60_000 },
  );
});
