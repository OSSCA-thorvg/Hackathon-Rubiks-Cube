import { expect, test, type Page } from '@playwright/test';

import { FRONT_BLOCK, pagePointInNet, probeCanvas } from './sceneContract.ts';

/** A viewport large enough to keep the desktop layout. */
const DESKTOP = { width: 1200, height: 1200 };

/** The engine's number for each sticker colour, which the swatches carry. */
const RED = '0';
const GREEN = '4';

/** Puts the mouse on one cell of the net's front face. */
async function pressFrontCell(page: Page, col: number, row: number) {
  const probe = await probeCanvas(page);
  const point = pagePointInNet(probe, 'net', FRONT_BLOCK, col, row);
  await page.mouse.click(point.x, point.y);
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  // The net alone, so a cell of it is where this test says it is.
  await page.locator('[data-view="2d"]').click();
  await page.locator('[data-flat="net"]').click();
});

test('a cube coloured onto the net is taken and then solved', async ({
  page,
}) => {
  const bar = page.locator('#paint-bar');
  await expect(bar).toBeHidden();

  await page.locator('#paint').click();
  await expect(bar).toBeVisible();
  await expect(page.locator('#paint')).toHaveAttribute('aria-pressed', 'true');

  // The draft opens as the cube is, so every colour already has its nine.
  await expect(page.locator('[data-sticker-tally="0"]')).toHaveText('9/9');

  // One square copied down wrong, which is what a person does.
  await page.locator(`[data-sticker="${RED}"]`).click();
  await pressFrontCell(page, 1, 1);

  await expect(page.locator('[data-sticker-tally="0"]')).toHaveText('10/9');
  await expect(page.locator('[data-sticker-tally="4"]')).toHaveText('8/9');

  // Refused, and told why, with the draft still open to be mended.
  await page.locator('#paint-apply').click();
  await expect(page.locator('#paint-note')).toContainText('too many squares');
  await expect(bar).toBeVisible();

  // Mended, it is taken -- and the cube it becomes is one the solver finishes.
  await page.locator(`[data-sticker="${GREEN}"]`).click();
  await pressFrontCell(page, 1, 1);
  await expect(page.locator('[data-sticker-tally="0"]')).toHaveText('9/9');

  await page.locator('#paint-apply').click();
  await expect(bar).toBeHidden();
  await expect(page.locator('#status')).toContainText('your cube now');
});

test('colouring the net does not turn the cube', async ({ page }) => {
  await page.locator('#paint').click();
  await page.locator(`[data-sticker="${RED}"]`).click();

  const probe = await probeCanvas(page);
  const from = pagePointInNet(probe, 'net', FRONT_BLOCK, 0, 1);
  const to = pagePointInNet(probe, 'net', FRONT_BLOCK, 2, 1);

  // A drag across the net is a stroke of the brush and not a turn of a layer.
  await page.mouse.move(from.x, from.y);
  await page.mouse.down();
  await page.mouse.move((from.x + to.x) / 2, from.y);
  await page.mouse.move(to.x, to.y);
  await page.mouse.up();

  // More than one square, and how many more is the browser's business: how a
  // drag of a few hundred pixels is cut into moves is not a thing this
  // application decides, so pinning a number here would be pinning Chromium's
  // behaviour rather than ours. What is ours is that they coloured and that
  // nothing turned.
  await expect(page.locator('[data-sticker-tally="0"]')).toHaveText(
    /^1[0-9]\/9$/,
  );
  await expect(page.locator('#move-log li')).toHaveCount(0);

  // And backing out leaves the cube exactly as it was found.
  await page.locator('#paint-cancel').click();
  await expect(page.locator('#paint-bar')).toBeHidden();
  await expect(page.locator('#solve')).toBeDisabled();
});

test('a painted cube travels in a link and comes back as itself', async ({
  page,
  context,
}) => {
  await context.grantPermissions(['clipboard-read', 'clipboard-write']);

  await page.locator('#paint').click();
  await page.locator(`[data-sticker="${RED}"]`).click();

  // A colouring that is a cube, taken by scrambling and copying it down as it
  // stands -- a cube nobody could reach by turning is exactly what the gate
  // refuses, so a test of the link has to hand it a real one.
  //
  // Waited on by the shell's own state and not by the Stop button: a scramble
  // is one of the two sequences that cannot be stopped, so Stop never appears
  // for it and a test that waited for it to go would not wait at all.
  await page.locator('#paint-cancel').click();
  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
    { timeout: 60_000 },
  );

  await page.locator('#paint').click();
  await page.locator('#paint-apply').click();
  await expect(page.locator('#paint-bar')).toBeHidden();

  const before = await page.evaluate(
    () => document.querySelector('#move-log')?.childElementCount ?? -1,
  );
  expect(before).toBe(0);

  await page.locator('#share').click();
  await expect(page.locator('#status')).toContainText('Link copied');
  const link = await page.evaluate(() => navigator.clipboard.readText());
  expect(link).toContain('#');

  // Opened again, the same cube is on the screen -- which for a painted one
  // means the colours travelled, since no sequence of moves describes it.
  await page.goto(link);
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(page.locator('#solve')).toBeEnabled();
});
