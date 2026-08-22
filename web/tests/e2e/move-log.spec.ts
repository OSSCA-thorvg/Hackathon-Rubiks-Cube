import { expect, test, type Page } from '@playwright/test';

import {
  FRONT_BLOCK,
  netDragFor,
  pagePointInNet,
  probeCanvas,
  type CanvasProbe,
  type NetView,
} from './sceneContract.ts';
import { pressInSettings } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Half a quarter turn: past what commits, short of what commits two. */
const SHORT_TURN = 0.5;

/** Every move a 3x3 can be written as, and nothing else. */
const NOTATION = /^[RLUDFBMES](['2])?$/;

/**
 * Drags one row of the net's front block to the left.
 *
 * Row 0 is the top of the face and turns the layer U turns. Row 1 is the
 * middle, which no button and no key can reach: a slice is something only a
 * drag makes, and it is the whole reason the log has letters for one.
 */
async function dragNetRowLeft(
  page: Page,
  probe: CanvasProbe,
  view: NetView,
  row: number,
): Promise<void> {
  const grab = pagePointInNet(probe, view, FRONT_BLOCK, 1, row);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(grab.x - netDragFor(probe, view, SHORT_TURN), grab.y, {
    steps: 8,
  });
  await page.mouse.up();
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('your own moves are written out in notation as you make them', async ({
  page,
}) => {
  const shell = page.locator('.game-shell');
  const entries = page.locator('#move-log li');

  // Nothing has been turned, so there is nothing written down.
  await expect(entries).toHaveCount(0);

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');

  // A whole scramble is on the cube and the list is still empty: it is the
  // cube you were handed rather than anything you did.
  await expect(entries).toHaveCount(0);
  await expect(page.locator('#move-log')).toBeEmpty();

  // A turn made from the keyboard is the first thing on it, written the
  // standard way.
  await page.keyboard.press('r');
  await expect(entries).toHaveCount(1);
  await expect(entries.nth(0)).toHaveText('R');
  await expect(entries.nth(0)).toHaveAttribute('data-state', 'current');

  await page.keyboard.press('Shift+U');
  await expect(entries).toHaveCount(2);
  await expect(entries.nth(1)).toHaveText("U'");

  // And a middle slice, which only a drag can make: the top row of the front
  // face turns the way U does, so the row below it is E the other way round.
  const probe = await probeCanvas(page);
  await dragNetRowLeft(page, probe, 'both', 1);
  await expect(entries).toHaveCount(3);
  await expect(entries.nth(2)).toHaveText("E'");
  await expect(entries.nth(2)).toHaveAttribute('data-state', 'current');

  // Every one of them reads as a move a solver would recognize.
  for (const text of await entries.allTextContents()) {
    expect(text).toMatch(NOTATION);
  }

  // Taken back: the move stays on the list, because it is still there to be
  // put back, and the mark moves down to what is left on the cube.
  await page.locator('#undo').click();
  await expect(entries.nth(2)).toHaveAttribute('data-state', 'pending');
  await expect(entries.nth(1)).toHaveAttribute('data-state', 'current');
  await expect(entries).toHaveCount(3);

  // Rewound through the scramble as well, which leaves all three waiting and
  // none of them marked -- and puts nothing of the scramble on the list.
  await page.locator('#rewind').click();
  await expect(shell).toHaveAttribute('data-game-state', 'completed');
  await expect(entries).toHaveCount(3);
  await expect(page.locator('#move-log li[data-state="current"]')).toHaveCount(
    0,
  );
  await expect(
    page.locator('#move-log li[data-state="pending"]'),
  ).toHaveCount(3);

  // A new cube has no record at all.
  await pressInSettings(page, '#reset');
  await expect(entries).toHaveCount(0);
});
