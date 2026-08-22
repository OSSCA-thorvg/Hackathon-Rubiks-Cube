import { expect, test, type Page } from '@playwright/test';

import {
  expectedNet,
  FRONT_BLOCK,
  netDragFor,
  pagePointInNet,
  probeCanvas,
} from './sceneContract.ts';
import { fillInSettings } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** The cube these tests put on the table, which has layers inside it. */
const SIZE = 5;

/** Half a quarter turn: past what commits, short of what commits two. */
const SHORT_TURN = 0.5;

/** Every move a 5x5 can be written as, numbered rather than named. */
const NOTATION = /^(\d+-)?(\d+)?[RLUDFB]w?(['2])?$/;

/** Puts a cube of `size` on the table and waits for it to be drawn. */
async function chooseSize(page: Page, size: number): Promise<void> {
  await fillInSettings(page, '#cube-size', String(size));
  await page.locator('#cube-size').dispatchEvent('change');
  await expect(page.locator('#cube-size')).toHaveValue(String(size));
}

/** Opens the panel the face buttons and the depth controls live in. */
async function openMoveControls(page: Page): Promise<void> {
  await page.locator('.advanced summary').click();
  await expect(page.locator('#turn-depth')).toBeVisible();
}

/** Sets how deep the face controls reach. */
async function setDepth(page: Page, depth: number): Promise<void> {
  await page.locator('#turn-depth').fill(String(depth));
  await page.locator('#turn-depth').dispatchEvent('change');
  await expect(page.locator('#turn-depth')).toHaveValue(String(depth));
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('a bigger cube is drawn, scrambled, and turned like any other', async ({
  page,
}) => {
  await chooseSize(page, SIZE);

  // Every one of the 150 net cells is its face's color, read at the cell
  // centres of a five-by-five face rather than a three-by-three one.
  const solved = await probeCanvas(page, SIZE);
  expect(solved.net).toEqual(expectedNet(SIZE));

  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  // The scramble is on the cube, and it reached the layers inside: the cells
  // away from a face's border are the ones only a move that turns more than
  // the surface can disturb, and some of them are no longer their own color.
  //
  // Some rather than all, and read as a set: the nine centre pieces of a face
  // share one color, so any single one of them may well have landed on a slot
  // of its own color again.
  const scrambled = await probeCanvas(page, SIZE);
  expect(scrambled.net).not.toEqual(expectedNet(SIZE));

  const inner = (cells: number[][]): string[] => {
    const read: string[] = [];
    for (let block = 0; block < 6; block += 1) {
      for (let row = 1; row < SIZE - 1; row += 1) {
        for (let col = 1; col < SIZE - 1; col += 1) {
          read.push(String(cells[block * SIZE * SIZE + row * SIZE + col]));
        }
      }
    }
    return read;
  };
  expect(inner(scrambled.net)).not.toEqual(inner(solved.net));

  // A drag on an inner row of the net turns that inner layer: nothing about
  // dragging asks whether a layer is on the outside.
  const before = await probeCanvas(page, SIZE);
  const grab = pagePointInNet(before, 'both', FRONT_BLOCK, 2, 2, SIZE);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(grab.x - netDragFor(before, 'both', SHORT_TURN), grab.y, {
    steps: 8,
  });
  await page.mouse.up();

  const entries = page.locator('#move-log li');
  await expect(entries).toHaveCount(1);
  await expect(entries.first()).toHaveText(NOTATION);

  const after = await probeCanvas(page, SIZE);
  expect(after.net).not.toEqual(before.net);
});

test('the face controls reach as deep as the depth box says', async ({
  page,
}) => {
  await chooseSize(page, SIZE);
  await openMoveControls(page);

  const entries = page.locator('#move-log li');

  // Depth one is the face itself, which is what the buttons have always done.
  await page.locator('[data-face="r"][data-turn="1"]').click();
  await expect(entries).toHaveCount(1);
  await expect(entries.nth(0)).toHaveText('R');

  // Depth two on its own is the slice behind it.
  await setDepth(page, 2);
  await page.locator('[data-face="r"][data-turn="1"]').click();
  await expect(entries).toHaveCount(2);
  await expect(entries.nth(1)).toHaveText('2R');

  // Wide brings the face along with it, and the keyboard uses the same
  // setting the buttons do.
  await page.locator('#turn-wide').click();
  await expect(page.locator('#turn-wide')).toHaveAttribute(
    'aria-pressed',
    'true',
  );
  await page.locator('canvas').press('r');
  await expect(entries).toHaveCount(3);
  await expect(entries.nth(2)).toHaveText('Rw');

  // Three deep and wide is written with its number.
  // Blurred first: a key pressed inside a box is somebody typing, and the
  // face letters deliberately stay out of it.
  await setDepth(page, 3);
  await page.locator('#turn-depth').blur();
  await page.keyboard.press('R');
  await expect(entries).toHaveCount(4);
  await expect(entries.nth(3)).toHaveText('3Rw');

  // Undo takes the last of them back, so the record and the cube agree about
  // moves that turn several layers at once.
  await page.locator('#undo').click();
  await expect(entries).toHaveCount(4);
  await expect(entries.nth(3)).toHaveAttribute('data-state', 'pending');
});

test('a link carries the cube it was made on', async ({ page, context }) => {
  await chooseSize(page, SIZE);

  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  await openMoveControls(page);
  await setDepth(page, 2);
  await page.locator('#turn-wide').click();
  await page.locator('[data-face="u"][data-turn="1"]').click();
  await expect(page.locator('#move-log li')).toHaveCount(1);

  const sent = await probeCanvas(page, SIZE);

  await context.grantPermissions(['clipboard-read', 'clipboard-write']);
  await page.locator('#share').click();
  await expect(page.locator('#status')).toHaveText(/Link copied/);
  const link = await page.evaluate(() => navigator.clipboard.readText());

  // Opened somewhere else: the cube arrives at the size it was made at, with
  // the same stickers and the same move written down.
  const opened = await context.newPage();
  await opened.setViewportSize(DESKTOP);
  await opened.goto(link);
  await expect(opened.locator('#app')).toHaveAttribute('data-state', 'ready');

  await expect(opened.locator('#cube-size')).toHaveValue(String(SIZE));
  const received = await probeCanvas(opened, SIZE);
  expect(received.net).toEqual(sent.net);
  await expect(opened.locator('#move-log li')).toHaveCount(1);
  await expect(opened.locator('#move-log li').first()).toHaveText('Uw');
});

test('the largest cube still draws and turns', async ({ page }) => {
  await chooseSize(page, 9);
  await openMoveControls(page);

  const solved = await probeCanvas(page, 9);
  expect(solved.net).toEqual(expectedNet(9));

  // The deepest layer the largest cube offers, which is one short of all of
  // them: the whole cube at once is a rotation and no control makes one.
  await setDepth(page, 8);
  await page.locator('#turn-wide').click();
  await page.locator('[data-face="r"][data-turn="1"]').click();

  await expect(page.locator('#move-log li')).toHaveCount(1);
  await expect(page.locator('#move-log li').first()).toHaveText('8Rw');

  const turned = await probeCanvas(page, 9);
  expect(turned.net).not.toEqual(solved.net);
});
