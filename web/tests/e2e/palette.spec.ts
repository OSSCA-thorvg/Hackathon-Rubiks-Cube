import { expect, test } from '@playwright/test';

import {
  assertSceneContract,
  assertVisibleFaces,
  CUBE_SIZE,
  NET_BLOCKS,
  probeCanvas,
} from './sceneContract.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

// The alternative set, as RGBA tuples. Written out here rather than imported
// from anywhere: this is the browser's copy of what the engine promises, and
// the point of the check is that the two agree.
const HC_WHITE = [255, 255, 255, 255]; // +Y up
const HC_YELLOW = [240, 153, 10, 255]; // -Y down, an amber
const HC_GREEN = [56, 224, 208, 255]; // +Z front, a turquoise
const HC_BLUE = [0, 0, 123, 255]; // -Z back, a navy
const HC_RED = [120, 0, 0, 255]; // +X right, a dark crimson
const HC_MAGENTA = [128, 64, 107, 255]; // -X left, the face called Orange

/** The net a solved cube shows in the alternative set, in probe order. */
function highContrastNet(): number[][] {
  const byBlock = [HC_WHITE, HC_MAGENTA, HC_GREEN, HC_RED, HC_BLUE, HC_YELLOW];
  const cells: number[][] = [];
  for (let block = 0; block < NET_BLOCKS.length; block += 1) {
    for (let i = 0; i < CUBE_SIZE * CUBE_SIZE; i += 1) {
      cells.push([...byBlock[block]!]);
    }
  }
  return cells;
}

test.use({ viewport: DESKTOP });

test('the palette changes both drawings at once and goes back', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  const classic = page.locator('[data-palette="classic"]');
  const highContrast = page.locator('[data-palette="high-contrast"]');

  await expect(classic).toHaveAttribute('aria-pressed', 'true');
  await expect(highContrast).toHaveAttribute('aria-pressed', 'false');

  // The standard cube, in both the 3D region and the net.
  assertSceneContract(await probeCanvas(page));

  await highContrast.click();
  await expect(highContrast).toHaveAttribute('aria-pressed', 'true');
  await expect(classic).toHaveAttribute('aria-pressed', 'false');

  const changed = await probeCanvas(page);

  // Both views, from one press and without waiting for anything to move: the
  // two drawings read the palette through the same function, and a change that
  // reached only one of them would mean they had stopped doing so.
  assertVisibleFaces(changed, HC_WHITE, HC_GREEN, HC_RED);
  expect(changed.net).toEqual(highContrastNet());

  // And back, so it is a toggle rather than a one-way door.
  await classic.click();
  await expect(classic).toHaveAttribute('aria-pressed', 'true');
  assertSceneContract(await probeCanvas(page));
});

test('the palette can be changed while a scramble is playing', async ({
  page,
}) => {
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  const highContrast = page.locator('[data-palette="high-contrast"]');

  await page.locator('#scramble-moves').fill('40');
  await page.locator('#scramble-moves').blur();
  await page.locator('#scramble').click();

  // Mid-sequence the controls that turn a layer are out and this one is not.
  // Scramble itself stays live throughout -- it is a command that replaces the
  // sequence rather than one the engine refuses -- so the face buttons are
  // what says the cube is busy.
  await expect(page.locator('[data-face="r"][data-turn="1"]')).toBeDisabled();
  await expect(highContrast).toBeEnabled();

  await highContrast.click();
  await expect(highContrast).toHaveAttribute('aria-pressed', 'true');

  // The sequence it was changed under still finishes, and the cube it leaves
  // behind is drawn in the set chosen halfway through.
  await expect(page.locator('[data-face="r"][data-turn="1"]')).toBeEnabled({
    timeout: 20_000,
  });

  const probe = await probeCanvas(page);
  const netColors = new Set(probe.net.map((cell) => cell.join(',')));
  for (const color of netColors) {
    expect(
      [HC_WHITE, HC_YELLOW, HC_GREEN, HC_BLUE, HC_RED, HC_MAGENTA].map((c) =>
        c.join(','),
      ),
    ).toContain(color);
  }
});
