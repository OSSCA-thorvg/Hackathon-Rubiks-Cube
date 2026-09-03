import { expect, test, type Page } from '@playwright/test';

import { closeSettings, openSettings } from './shell.ts';
import { probeCanvas } from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1000 };

/** The engine's standard lights, as the panel writes them out. */
const STANDARD = '0.75,1,1.25,2.6,7,4,0.38,0.6,12';

test.use({ viewport: DESKTOP });

async function ready(page: Page): Promise<void> {
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
}

/**
 * Moves a range slider the way a hand would.
 *
 * `fill` refuses a range input, so the value is set and the `input` event the
 * controls listen for is raised in its place.
 */
async function slide(page: Page, selector: string, value: string): Promise<void> {
  await page.locator(selector).evaluate((element, next) => {
    const input = element as HTMLInputElement;
    input.value = next;
    input.dispatchEvent(new Event('input', { bubbles: true }));
  }, value);
}

test('the panel shows the lights the cube is drawn under', async ({ page }) => {
  await page.goto('/');
  await ready(page);

  await openSettings(page);
  await expect(page.locator('[data-lighting-value="ambient"]')).toHaveText(
    '0.75',
  );
  await expect(page.locator('[data-lighting-value="specular"]')).toHaveText(
    '0.60',
  );
  await expect(page.locator('[data-lighting-value="shininess"]')).toHaveText(
    '12',
  );
  await expect(page.locator('#lighting-values')).toHaveText(STANDARD);
  await expect(page.locator('#lighting-ambient')).toBeEnabled();
});

test('a list in the address bar is what the panel opens with', async ({
  page,
}) => {
  await page.goto('/?lighting=0.5,1,1,2.6,7,4,0.38,0.6,12');
  await ready(page);

  await openSettings(page);
  await expect(page.locator('[data-lighting-value="ambient"]')).toHaveText(
    '0.50',
  );
  await expect(page.locator('[data-lighting-value="saturation"]')).toHaveText(
    '1.00',
  );
  await expect(page.locator('#lighting-values')).toHaveText(
    '0.5,1,1,2.6,7,4,0.38,0.6,12',
  );
});

test('a slider relights the still cube at once and reset puts it back', async ({
  page,
}) => {
  await page.goto('/');
  await ready(page);
  const lit = (await probeCanvas(page)).left;

  await openSettings(page);
  await slide(page, '#lighting-ambient', '0.3');
  await expect(page.locator('[data-lighting-value="ambient"]')).toHaveText(
    '0.30',
  );
  await expect(page.locator('#lighting-values')).toHaveText(
    '0.3,1,1.25,2.6,7,4,0.38,0.6,12',
  );
  await closeSettings(page);

  // The green face is darker with less ambient light, and it is darker now,
  // with nothing else having asked for a frame.
  const dim = (await probeCanvas(page)).left;
  expect(dim[1]!).toBeLessThan(lit[1]! - 40);

  await openSettings(page);
  await page.locator('#lighting-reset').click();
  await expect(page.locator('[data-lighting-value="ambient"]')).toHaveText(
    '0.75',
  );
  await expect(page.locator('#lighting-values')).toHaveText(STANDARD);
  await closeSettings(page);

  expect((await probeCanvas(page)).left).toEqual(lit);
});
