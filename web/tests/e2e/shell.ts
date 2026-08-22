import { expect, type Page } from '@playwright/test';

/**
 * Reaching the controls that moved into Settings.
 *
 * The palette, the cube size, the scramble length, the speed, the turn sound,
 * the home view and the session reset used to sit on the same face as the
 * cube. They are all still there, one drawer away, and every spec that used to
 * press them straight off the page goes through here instead.
 *
 * Open-act-close rather than open-once-per-spec: what a spec is testing is the
 * command, and leaving a panel over the stage afterwards would change what
 * every assertion after it is looking at.
 */
export async function openSettings(page: Page): Promise<void> {
  const panel = page.locator('#settings-panel');
  if (await panel.isVisible()) return;

  await page.locator('#settings-trigger').click();
  await expect(panel).toBeVisible();
}

export async function closeSettings(page: Page): Promise<void> {
  const panel = page.locator('#settings-panel');
  if (!(await panel.isVisible())) return;

  await page.locator('#settings-close').click();
  await expect(panel).toBeHidden();
}

/** Presses one control that lives in Settings, and closes up again. */
export async function pressInSettings(
  page: Page,
  selector: string,
): Promise<void> {
  await openSettings(page);
  await page.locator(selector).click();
  await closeSettings(page);
}

/** The same, with a tap, for the specs that run with touch alone. */
export async function tapInSettings(
  page: Page,
  selector: string,
): Promise<void> {
  await openSettings(page);
  await page.locator(selector).tap();
  await closeSettings(page);
}

/**
 * Fills one field that lives in Settings and commits it.
 *
 * The blur is what the page reads as "done typing", so it belongs to the act
 * rather than to the spec: a field left focused has not been committed and a
 * drawer closed over it never would be.
 */
export async function fillInSettings(
  page: Page,
  selector: string,
  value: string,
): Promise<void> {
  await openSettings(page);
  const field = page.locator(selector);
  await field.fill(value);
  await field.blur();
  await closeSettings(page);
}
