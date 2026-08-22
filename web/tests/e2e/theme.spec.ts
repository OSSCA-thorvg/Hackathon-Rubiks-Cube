import { expect, test } from '@playwright/test';

import { openSettings } from './shell.ts';
import { probeCanvas } from './sceneContract.ts';

/** The two grounds, as the engine paints them. */
const DARK_GROUND = [32, 32, 32, 255];
const LIGHT_GROUND = [231, 235, 240, 255];

const DESKTOP = { width: 1200, height: 1000 };

test.use({ viewport: DESKTOP });

test('the machine decides when nothing has been chosen', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  // The suite runs dark, so System means dark here -- on the page and on the
  // canvas alike, which is the whole point of carrying the theme through to
  // the engine rather than only through CSS.
  await expect(page.locator('html')).not.toHaveAttribute('data-theme');
  expect((await probeCanvas(page)).corners[0]).toEqual(DARK_GROUND);

  await openSettings(page);
  await expect(page.locator('[data-theme-choice="system"]')).toHaveAttribute(
    'aria-pressed',
    'true',
  );
});

test('a light machine gets a light page and a light canvas', async ({
  page,
}) => {
  await page.emulateMedia({ colorScheme: 'light' });
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  await expect(page.locator('html')).not.toHaveAttribute('data-theme');
  expect((await probeCanvas(page)).corners[0]).toEqual(LIGHT_GROUND);
});

test('an explicit choice paints both surfaces and survives a reload', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();
  await openSettings(page);

  await page.locator('[data-theme-choice="light"]').click();
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
  expect((await probeCanvas(page)).corners[0]).toEqual(LIGHT_GROUND);

  // The one value the page keeps. It has to be on the element before the
  // stylesheet is read, or a page saved as Light opens dark and then blinks.
  await page.reload();
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
  expect((await probeCanvas(page)).corners[0]).toEqual(LIGHT_GROUND);

  // And back to the machine, which clears the attribute rather than writing
  // the answer the machine happens to be giving right now.
  await openSettings(page);
  await page.locator('[data-theme-choice="system"]').click();
  await expect(page.locator('html')).not.toHaveAttribute('data-theme');
  expect((await probeCanvas(page)).corners[0]).toEqual(DARK_GROUND);
});

test('an explicit choice is not overruled by the machine', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();
  await openSettings(page);

  await page.locator('[data-theme-choice="dark"]').click();
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark');

  // The machine goes light underneath a page that was told to be dark. What
  // was chosen is a decision about this page, so it stands.
  await page.emulateMedia({ colorScheme: 'light' });
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark');
  expect((await probeCanvas(page)).corners[0]).toEqual(DARK_GROUND);
  await expect(page.locator('[data-theme-choice="dark"]')).toHaveAttribute(
    'aria-pressed',
    'true',
  );
});

test('the settings surface opens and closes from the keyboard', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  const trigger = page.locator('#settings-trigger');
  const panel = page.locator('#settings-panel');

  await trigger.click();
  await expect(panel).toBeVisible();
  await expect(trigger).toHaveAttribute('aria-expanded', 'true');

  await page.keyboard.press('Escape');
  await expect(panel).toBeHidden();
  await expect(trigger).toHaveAttribute('aria-expanded', 'false');
  // Focus comes back to where it went in from, so a keyboard is not left
  // standing at the top of the document.
  await expect(trigger).toBeFocused();
});
