import { expect, test, type Page } from '@playwright/test';

import {
  assertDrawingBufferMatchesCss,
  assertFacesAndCorners,
  assertVisibleFaces,
  BACK_LIT,
  cubeDragFor,
  FRONT_RIGHT_COLUMN,
  near,
  pagePointInCube,
  probeCanvas,
  QUARTER_TURN_FRACTION,
  RIGHT_LIT,
  UP_LIT,
} from './sceneContract.ts';

/**
 * The page the address opens without `?extended`: the cube alone, Scramble,
 * Reset and Solve, and a light and dark switch. Nothing on it links to the
 * full app; every other spec is the full app's and opens `./?extended`.
 *
 * With no net on the page, what the cube is in is read off the cube itself:
 * the grid of sticker samples the probe takes on its three visible faces,
 * held against the same grid on a solved cube.
 */

const DESKTOP = { width: 1200, height: 1200 };
const PHONE = { width: 375, height: 812 };

/** The ids of every button and link a person can see on the page. */
async function visibleControls(page: Page): Promise<string[]> {
  return page.evaluate(() =>
    [...document.querySelectorAll<HTMLElement>('#app button, #app a[href]')]
      .filter((control) => control.checkVisibility())
      .map((control) => control.id),
  );
}

/** Waits for whatever is playing -- a scramble, a solution -- to end. */
async function settled(page: Page): Promise<void> {
  await expect(page.locator('#stop')).toBeHidden({ timeout: 60_000 });
}

/** Scrambles, and waits for the scramble to have played out. */
async function scramble(page: Page): Promise<void> {
  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );
}

async function open(page: Page, path = './'): Promise<void> {
  await page.goto(path);
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
}

for (const [name, viewport] of [
  ['desktop', DESKTOP],
  ['phone', PHONE],
] as const) {
  test(`shows the cube alone and one row of controls (${name})`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport);
    await open(page);

    await expect(page.locator('.game-shell')).toHaveAttribute(
      'data-mode',
      'minimal',
    );
    await expect(page.locator('#game-title')).toBeVisible();
    await expect(page.locator('.brand__byline')).toHaveText('powered by ThorVG');
    expect(await visibleControls(page)).toEqual([
      'theme-toggle',
      'scramble',
      'minimal-reset',
      'solve',
    ]);

    // Everything else the full app has is off the screen.
    for (const selector of [
      '#status',
      '#timer',
      '#command-trigger',
      '#share',
      '#settings-trigger',
      '#cube-size-chip',
      '.view-rail',
      '.timeline',
      '.details-rail',
      '#view-rings',
      '#view-axes',
    ]) {
      await expect(page.locator(selector), selector).toBeHidden();
    }

    // The cube alone, at the home view, and no net beside it.
    await expect(page.locator('#view')).toBeVisible();
    await expect(page.locator('#view-net')).toBeHidden();
    const probe = await probeCanvas(page);
    assertDrawingBufferMatchesCss(probe);
    assertFacesAndCorners(probe);

    // Nothing runs off the side of a phone.
    expect(
      await page.evaluate(
        () => document.documentElement.scrollWidth <= window.innerWidth,
      ),
    ).toBe(true);
  });
}

test('the menu shortcut and the face-turn keys do nothing', async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await open(page);
  const before = await probeCanvas(page);

  // A corner of the canvas away from the header, which lies over its top.
  const box = (await page.locator('#view').boundingBox())!;
  await page.locator('#view').click({ position: { x: 5, y: box.height - 5 } });
  for (const key of ['Meta+k', 'Control+k', 'r', 'Shift+U', 'f']) {
    await page.keyboard.press(key);
  }

  await expect(page.locator('#command-palette')).toBeHidden();
  await expect(page.locator('#settings-panel')).toBeHidden();
  await expect(page.locator('#status')).toHaveText(
    'Ready. Scramble the cube to begin.',
  );
  const after = await probeCanvas(page);
  expect(after.cubeGrid).toEqual(before.cubeGrid);
});

test('Scramble, Solve and Reset do what the full app does', async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await open(page);
  const shell = page.locator('.game-shell');
  const solve = page.locator('#solve');

  await expect(solve).toBeDisabled();
  const solved = (await probeCanvas(page)).cubeGrid;
  await scramble(page);
  const scrambled = await probeCanvas(page);
  expect(scrambled.cubeGrid).not.toEqual(solved);

  // Solve is the full app's, busy and all, and leaves the cube solved.
  await expect(solve).toBeEnabled();
  await solve.click();
  await expect(page.locator('#stop')).toBeVisible();
  await settled(page);
  const worked = await probeCanvas(page);
  assertFacesAndCorners(worked);
  expect(worked.cubeGrid).toEqual(solved);
  await expect(solve).toBeDisabled();

  // Scramble again, look at it from somewhere else, and Reset: the solved
  // cube from the home view.
  await scramble(page);
  const atRest = await probeCanvas(page);
  const corner = pagePointInCube(atRest, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(corner.x - cubeDragFor(atRest, 1), corner.y, {
    steps: 12,
  });
  await page.mouse.up();
  await expect
    .poll(async () => (await probeCanvas(page)).cubeGrid)
    .not.toEqual(atRest.cubeGrid);

  await page.locator('#minimal-reset').click();
  await expect(shell).toHaveAttribute('data-game-state', 'idle');
  await expect
    .poll(async () => (await probeCanvas(page)).cubeGrid)
    .toEqual(solved);
  assertFacesAndCorners(await probeCanvas(page));
});

test('a drag turns a layer and a drag on empty space orbits', async ({
  page,
}) => {
  await page.setViewportSize(DESKTOP);
  await open(page);
  const atRest = await probeCanvas(page);

  // Empty space: the viewpoint goes round, the cube stays as it is.
  const corner = pagePointInCube(atRest, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(corner.x - cubeDragFor(atRest, 1), corner.y, {
    steps: 12,
  });
  await page.mouse.up();
  await expect
    .poll(async () => near((await probeCanvas(page)).left, RIGHT_LIT, 2))
    .toBe(true);
  assertVisibleFaces(await probeCanvas(page), UP_LIT, RIGHT_LIT, BACK_LIT);

  // A sticker: its layer turns.
  await page.locator('#minimal-reset').click();
  await expect
    .poll(async () => (await probeCanvas(page)).cubeGrid)
    .toEqual(atRest.cubeGrid);
  const grab = pagePointInCube(atRest, FRONT_RIGHT_COLUMN);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(
    grab.x,
    grab.y - cubeDragFor(atRest, QUARTER_TURN_FRACTION),
    { steps: 8 },
  );
  await page.mouse.up();
  await expect
    .poll(async () => (await probeCanvas(page)).cubeGrid)
    .not.toEqual(atRest.cubeGrid);
});

test('the sun and moon switch the theme, and the full app keeps it', async ({
  page,
}) => {
  await page.emulateMedia({ colorScheme: 'light' });
  await page.setViewportSize(DESKTOP);
  await open(page);
  const toggle = page.locator('#theme-toggle');
  const background = (): Promise<string> =>
    page.evaluate(() => getComputedStyle(document.body).backgroundColor);

  // The machine is light, so the sun shows and nothing is chosen yet.
  await expect(toggle).toHaveAttribute('aria-pressed', 'false');
  await expect(page.locator('html')).not.toHaveAttribute('data-theme');
  const light = await background();

  await toggle.click();
  await expect(toggle).toHaveAttribute('aria-pressed', 'true');
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark');
  expect(await background()).not.toBe(light);

  // The full app opens dark, with Dark chosen in its Settings.
  await open(page, './?extended');
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark');
  await expect(page.locator('[data-theme-choice="dark"]')).toHaveAttribute(
    'aria-pressed',
    'true',
  );
});

test('a shared link without ?extended opens its cube on the minimal page', async ({
  page,
  context,
}) => {
  await context.grantPermissions(['clipboard-read', 'clipboard-write']);
  await page.setViewportSize(DESKTOP);
  await open(page);
  const solved = (await probeCanvas(page)).cubeGrid;

  await open(page, './?extended');
  await scramble(page);
  await page.locator('#share').click();
  await expect(page.locator('#status')).toHaveText(
    'Link copied. It opens this cube.',
  );
  const link = new URL(await page.evaluate(() => navigator.clipboard.readText()));
  expect(link.search).toBe('?extended');

  // The same link with the query taken off.
  link.search = '';
  const other = await context.newPage();
  await other.setViewportSize(DESKTOP);
  await open(other, link.toString());

  await expect(other.locator('.game-shell')).toHaveAttribute(
    'data-mode',
    'minimal',
  );
  // The page says so to a screen reader, though the line is not drawn.
  await expect(other.locator('#status')).toHaveText(
    'Ready. This cube came from a shared link.',
  );
  expect(new URL(other.url()).hash).toBe('');
  expect(new URL(other.url()).search).toBe('');

  // The cube the link carried rather than a solved one, and a cube like any
  // other here: Solve works it out.
  const opened = await probeCanvas(other);
  assertDrawingBufferMatchesCss(opened);
  expect(opened.cubeGrid).not.toEqual(solved);
  await other.locator('#solve').click();
  await settled(other);
  const worked = await probeCanvas(other);
  assertFacesAndCorners(worked);
  expect(worked.cubeGrid).toEqual(solved);
});
