import { expect, test, type Page } from '@playwright/test';

import {
  assertSceneContract,
  cssColour,
  cubeDragFor,
  CUT_SHARE_MARGIN,
  FRONT_RIGHT_COLUMN,
  pagePointInCube,
  probeCanvas,
  WHITE,
} from './sceneContract.ts';
import {
  fillInSettings,
  openPanel,
  openSettings,
  pressInSettings,
  tapInSettings,
} from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Drags the background left by one quarter-view turn. */
async function orbitOnce(page: Page): Promise<void> {
  const probe = await probeCanvas(page);
  const corner = pagePointInCube(probe, [0.94, 0.06]);
  await page.mouse.move(corner.x, corner.y);
  await page.mouse.down();
  await page.mouse.move(corner.x - cubeDragFor(probe, 1), corner.y, {
    steps: 12,
  });
  await page.mouse.up();
}

/**
 * Reloads the page with the one word production takes from Web Crypto pinned,
 * so the real WASM generator produces the same sequence every run.
 *
 * The reload is part of it: the script is installed for documents created from
 * here on, so the page already up was built without it.
 */
async function restartWithSeed(page: Page, seed: number): Promise<void> {
  await page.addInitScript(`
    const originalGetRandomValues = Crypto.prototype.getRandomValues;
    Crypto.prototype.getRandomValues = function(array) {
      if (array instanceof Uint32Array && array.length === 1) {
        array[0] = ${seed};
        return array;
      }
      return originalGetRandomValues.call(this, array);
    };
  `);
  await page.reload();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
}

/** Sets the scramble length, the way a person changing the box would. */
async function setScrambleMoves(page: Page, count: number): Promise<void> {
  await fillInSettings(page, '#scramble-moves', String(count));
}

test.beforeEach(async ({ page }) => {
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('scramble starts a session on first committed move and Reset restores it', async ({
  page,
}) => {
  const root = page.locator('.game-shell');
  const timer = page.locator('#timer');

  await expect(root).toHaveAttribute('data-game-state', 'idle');
  await expect(timer).toHaveText('00:00.00');
  const solved = await probeCanvas(page);
  assertSceneContract(solved);

  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'ready');
  await expect(timer).toHaveText('00:00.00');

  // The scramble reached both views, not just the gameplay state.
  const scrambled = await probeCanvas(page);
  expect(scrambled.net).not.toEqual(solved.net);
  expect(scrambled.cubeGrid).not.toEqual(solved.cubeGrid);

  // A programmatic R turn uses the same animation and commit path as a drag.
  await page.keyboard.press('r');
  await expect(root).toHaveAttribute('data-game-state', 'running');
  await expect.poll(async () => page.locator('#timer').textContent()).not.toBe(
    '00:00.00',
  );

  await pressInSettings(page, '#reset');
  await expect(root).toHaveAttribute('data-game-state', 'idle');
  await expect(timer).toHaveText('00:00.00');
  assertSceneContract(await probeCanvas(page));
});

test('Both is default and 3D, 2D, and Home view controls preserve gameplay', async ({
  page,
}) => {
  const stage = page.locator('#stage');
  const canvas = page.locator('#view');
  const flatCanvas = page.locator('#view-net');
  const both = page.locator('[data-view="both"]');
  const cube = page.locator('[data-view="3d"]');
  const net = page.locator('[data-view="2d"]');

  await expect(both).toHaveAttribute('aria-pressed', 'true');
  await expect(stage).toHaveAttribute('data-view-mode', 'both');
  await expect(canvas).toBeVisible();
  await expect(flatCanvas).toBeVisible();

  // Each mode is the canvases it shows, and the page lays them out.
  await cube.click();
  await expect(cube).toHaveAttribute('aria-pressed', 'true');
  await expect(stage).toHaveAttribute('data-view-mode', '3d');
  await expect(canvas).toBeVisible();
  await expect(flatCanvas).toBeHidden();

  await net.click();
  await expect(net).toHaveAttribute('aria-pressed', 'true');
  await expect(stage).toHaveAttribute('data-view-mode', '2d');
  await expect(canvas).toBeHidden();
  await expect(flatCanvas).toBeVisible();

  // The net is something to work on rather than to look at: a drag across a
  // cell turns a layer here just as one on the cube does.
  await probeCanvas(page);
  const before = await flatCanvas.evaluate(
    (element) => (element as HTMLCanvasElement).toDataURL(),
  );
  const box = await flatCanvas.boundingBox();
  expect(box).not.toBeNull();
  await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
  await page.mouse.down();
  await page.mouse.move(box!.x + box!.width * 0.75, box!.y + box!.height / 2);
  await page.mouse.up();
  await expect
    .poll(async () =>
      flatCanvas.evaluate((element) =>
        (element as HTMLCanvasElement).toDataURL(),
      ),
    )
    .not.toBe(before);

  // Put the cube back, so what follows is about the view controls alone.
  await pressInSettings(page, '#reset');

  await both.click();
  await orbitOnce(page);
  const orbited = await canvas.evaluate(
    (element) => (element as HTMLCanvasElement).toDataURL(),
  );
  await pressInSettings(page, '#home-view');
  expect(
    await canvas.evaluate(
      (element) => (element as HTMLCanvasElement).toDataURL(),
    ),
  ).not.toBe(orbited);
  assertSceneContract(await probeCanvas(page));
});

/** Where an element is on the page, which it has to be for this to ask. */
async function boxOf(
  page: Page,
  selector: string,
): Promise<{ x: number; y: number; width: number; height: number }> {
  const box = await page.locator(selector).boundingBox();
  expect(box, selector).not.toBeNull();
  return box!;
}

test('the desktop stage stands between two rails, with the record and the dock under it', async ({
  page,
}) => {
  const canvas = await boxOf(page, '#stage');
  const header = await boxOf(page, '.app-header');
  const timeline = await boxOf(page, '.timeline');
  const dock = await boxOf(page, '.action-dock');
  const viewRail = await boxOf(page, '.view-rail');
  const detailsRail = await boxOf(page, '.details-rail');

  // The header above; the timeline and then the commands below.
  expect(header.y + header.height).toBeLessThanOrEqual(canvas.y + 1);
  expect(timeline.y).toBeGreaterThanOrEqual(canvas.y + canvas.height - 1);
  expect(dock.y).toBeGreaterThanOrEqual(timeline.y + timeline.height - 1);

  // The two rails beside the canvas and clear of it: they float at the
  // edges of the window rather than framing the cube as toolbars.
  expect(viewRail.x + viewRail.width).toBeLessThanOrEqual(canvas.x);
  expect(detailsRail.x).toBeGreaterThanOrEqual(canvas.x + canvas.width);

  // One column, one centre. Half a pixel of rounding either way is the
  // browser's, not the layout's.
  const centre = (box: { x: number; width: number }): number =>
    box.x + box.width / 2;
  expect(Math.abs(centre(dock) - centre(canvas))).toBeLessThan(2);
  expect(Math.abs(centre(timeline) - centre(canvas))).toBeLessThan(2);

  // The page is the colour the engine clears the canvas to, so the stage
  // has no edge to it: the stylesheet's ground against a pixel the engine
  // drew, since the two are written in two languages.
  expect(await cssColour(page, 'body')).toEqual(
    (await probeCanvas(page)).corners[0],
  );
});

test('a detail panel stays open until its own button, and the stage makes room', async ({
  page,
}) => {
  const moves = page.locator('#panel-moves');
  await expect(moves).toBeHidden();

  await page.locator('#details-moves').click();
  await expect(moves).toBeVisible();
  await expect(page.locator('#details-moves')).toHaveAttribute(
    'aria-expanded',
    'true',
  );

  // Not a popup: a key and a press somewhere else leave it where it is.
  await page.keyboard.press('Escape');
  await page.locator('.clock').click({ force: true });
  await expect(moves).toBeVisible();

  // Beside the stage rather than over it, once the column has opened.
  await expect
    .poll(async () => {
      const canvas = await boxOf(page, '#stage');
      const panel = await boxOf(page, '#panel-moves');
      return canvas.x + canvas.width <= panel.x + 1;
    })
    .toBe(true);

  // A wide screen keeps any number of them open together.
  await page.locator('#details-session').click();
  await expect(page.locator('#panel-session')).toBeVisible();
  await expect(moves).toBeVisible();

  await page.locator('#details-moves').click();
  await expect(moves).toBeHidden();
  await expect(page.locator('#panel-session')).toBeVisible();
});

// The panels are shown in a different place on each, and are tabbed to from
// the same one: straight after the button that opened them.
for (const [screen, viewport] of [
  ['wide', DESKTOP],
  ['narrow', { width: 390, height: 844 }],
] as const) {
  test(`a panel opened from the keyboard is where Tab goes next, ${screen}`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport);
    const toggle = page.locator('#details-turn');

    await toggle.focus();
    await page.keyboard.press('Enter');
    await expect(page.locator('#panel-turn')).toBeVisible();

    await page.keyboard.press('Tab');
    await expect(page.locator('#panel-turn [data-step="-1"]')).toBeFocused();
    await page.keyboard.press('Shift+Tab');
    await expect(toggle).toBeFocused();
  });
}

test('the cube keys do not reach under a dialog', async ({ page }) => {
  const shell = page.locator('.game-shell');
  const settings = page.locator('#settings-panel');
  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');

  // With the focus on a button of the dialog, and after a press on a label
  // of it, which leaves the focus on the page rather than in the dialog.
  await page.locator('#settings-trigger').click();
  await expect(settings).toBeVisible();
  await page.keyboard.press('r');
  await settings.locator('.settings__label', { hasText: 'Appearance' }).click();
  await page.keyboard.press('u');
  await expect(page.locator('#move-log li')).toHaveCount(0);
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  await expect(page.locator('#timer')).toHaveText('00:00.00');

  // The command menu keeps its field focused through a press on a heading,
  // so a letter types and Tab does not reach what the menu covers.
  await page.keyboard.press('Escape');
  await expect(settings).toBeHidden();
  await page.keyboard.press('ControlOrMeta+k');
  await page.locator('.command-group__label').first().click();
  await expect(page.locator('#command-input')).toBeFocused();
  await page.keyboard.press('Tab');
  await expect(page.locator('#command-input')).toBeFocused();
  await page.keyboard.press('d');
  await expect(page.locator('#command-input')).toHaveValue('d');
  await expect(page.locator('#move-log li')).toHaveCount(0);

  // Closed, the same letter turns the cube.
  await page.keyboard.press('Escape');
  await page.keyboard.press('r');
  await expect(shell).toHaveAttribute('data-game-state', 'running');
  await expect(page.locator('#move-log li')).toHaveCount(1);
});

test('a scramble is turned into the cube where it can be watched', async ({
  page,
}) => {
  const root = page.locator('.game-shell');
  const moveButtons = page.locator('[data-face]');
  const solved = await probeCanvas(page);

  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'scrambling');

  // The cut face of a cubie is only ever drawn while a layer is part way
  // round, so finding it is finding the cube mid-turn rather than merely
  // changed. Nothing can be turned by hand for as long as that lasts. The
  // cut is wide for a fraction of each turn, so the frames are read often.
  await expect(moveButtons.first()).toBeDisabled();
  await expect(page.locator('#timer')).toHaveText('00:00.00');
  await expect
    .poll(
      async () => {
        const probe = await probeCanvas(page);
        return probe.bodyShare > solved.bodyShare + CUT_SHARE_MARGIN;
      },
      { timeout: 5000, intervals: [60] },
    )
    .toBe(true);

  await expect(root).toHaveAttribute('data-game-state', 'ready');
  await expect(moveButtons.first()).toBeEnabled();

  // And it arrived: both views moved, and the clock is waiting rather than
  // running.
  const scrambled = await probeCanvas(page);
  expect(scrambled.net).not.toEqual(solved.net);
  expect(scrambled.cubeGrid).not.toEqual(solved.cubeGrid);
  await expect(page.locator('#timer')).toHaveText('00:00.00');
});

test('a drag over the cube while a scramble plays only sweeps the view', async ({
  page,
}) => {
  await restartWithSeed(page, 42);

  const root = page.locator('.game-shell');

  // One scramble with a drag pulled straight across a sticker in the middle
  // of it -- the grip that takes hold of a layer at any other moment.
  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'scrambling');

  const probe = await probeCanvas(page);
  const grab = pagePointInCube(probe, FRONT_RIGHT_COLUMN);
  await page.mouse.move(grab.x, grab.y);
  await page.mouse.down();
  await page.mouse.move(grab.x - cubeDragFor(probe, 1), grab.y, {
    steps: 12,
  });
  await page.mouse.up();
  await expect(root).toHaveAttribute('data-game-state', 'ready');

  // The drag did something: the viewpoint is no longer where Home puts it.
  const swept = await probeCanvas(page);
  await pressInSettings(page, '#home-view');
  const dragged = await probeCanvas(page);
  expect(dragged.cubeGrid).not.toEqual(swept.cubeGrid);

  // And it did nothing else. The same seed again, untouched, has to land on
  // the same cube -- an extra turn taken from under the finger would not.
  await page.locator('#scramble').click();
  await expect(root).toHaveAttribute('data-game-state', 'ready');
  const clean = await probeCanvas(page);

  expect(dragged.net).toEqual(clean.net);
  expect(dragged.cubeGrid).toEqual(clean.cubeGrid);
});

test('a fixed scramble solved through keyboard controls stops the timer', async ({
  page,
}) => {
  await restartWithSeed(page, 42);

  // Three moves rather than twenty. This sequence has to be taken from the
  // generator by hand, and the length only decides how much of that there is
  // to redo whenever the generator changes -- what the test is here for is
  // the path from a scrambled cube to a stopped clock, which three moves
  // walk exactly as well as twenty.
  await setScrambleMoves(page, 3);
  await page.locator('#scramble').click();

  // The scramble is turned into the cube rather than applied, so the keys
  // below have to wait for it: a layer cannot be turned by hand until then.
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
  );

  // Seed 42 begins L, D, L'. Undoing it runs backwards: L, D', L'.
  const solution = ['l', 'D', 'L'];
  const moveButtons = page.locator('[data-face]');

  for (const key of solution) {
    const shortcut = key === key.toUpperCase() ? `Shift+${key}` : key;
    await page.keyboard.press(shortcut);
    await expect(moveButtons.first()).toBeDisabled();
    await expect(moveButtons.first()).toBeEnabled();
  }

  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'completed',
  );
  await expect(page.locator('#status')).toContainText('Solved in');
  await expect(page.locator('#timer')).not.toHaveText('00:00.00');
});

test.describe('on a phone', () => {
  test.use({ hasTouch: true });

  test('taps alone run scramble, a turn, and reset', async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    const root = page.locator('.game-shell');
    const timer = page.locator('#timer');

    await page.locator('#scramble').tap();
    await expect(root).toHaveAttribute('data-game-state', 'ready');

    // The move controls are in a panel until asked for, on every viewport.
    await openPanel(page, 'turn', { tap: true });
    await page.locator('[data-face="r"][data-turn="1"]').tap();

    await expect(root).toHaveAttribute('data-game-state', 'running');
    await expect
      .poll(async () => timer.textContent())
      .not.toBe('00:00.00');

    await tapInSettings(page, '#reset');
    await expect(root).toHaveAttribute('data-game-state', 'idle');
    await expect(timer).toHaveText('00:00.00');
    assertSceneContract(await probeCanvas(page));
  });
});

test('mobile layout moves actions below the canvas without horizontal overflow', async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 844 });

  const canvas = await page.locator('#stage').boundingBox();
  const actions = await page.locator('.action-dock').boundingBox();
  expect(canvas).not.toBeNull();
  expect(actions).not.toBeNull();
  expect(actions!.y).toBeGreaterThanOrEqual(canvas!.y + canvas!.height);

  const geometry = await page.evaluate(() => ({
    viewport: document.documentElement.clientWidth,
    content: document.documentElement.scrollWidth,
  }));
  expect(geometry.content).toBeLessThanOrEqual(geometry.viewport);
});

test('every control is a whole target across, wherever it is put', async ({
  page,
}) => {
  // 44 pixels both ways, the stylesheet's own --target, for everything that
  // can be pressed or typed into -- on the page, in each panel, in the drawer,
  // on the paint bar and in the command menu, on a phone and on a wide window
  // both tall and short. The sliders are the exception: a range input is a
  // track to drag along, and its thumb is the target.
  //
  // Measured as what a press lands on: the box, and an absolute ::after that
  // reaches past it -- which is how a move chip is a target taller than it is
  // drawn.
  const small = async (): Promise<string[]> =>
    page.evaluate(() =>
      [
        ...document.querySelectorAll<HTMLElement>(
          'button, input:not([type="range"]), [role="option"]',
        ),
      ]
        .map((element) => {
          const box = element.getBoundingClientRect();
          let { top, bottom, left, right } = box;
          const after = getComputedStyle(element, '::after');
          if (after.position === 'absolute' && after.content !== 'none') {
            const own = getComputedStyle(element);
            const edge = (side: string): number =>
              parseFloat(own.getPropertyValue(`border-${side}-width`));
            const inset = (side: string): number =>
              parseFloat(after.getPropertyValue(side));
            top = Math.min(top, box.top + edge('top') + inset('top'));
            bottom = Math.max(bottom, box.bottom - edge('bottom') - inset('bottom'));
            left = Math.min(left, box.left + edge('left') + inset('left'));
            right = Math.max(right, box.right - edge('right') - inset('right'));
          }
          return { element, width: right - left, height: bottom - top, box };
        })
        .filter(
          ({ box, width, height }) =>
            box.width > 0 && box.height > 0 && Math.min(width, height) < 43.99,
        )
        .map(({ element, width, height }) => {
          const name =
            element.id || element.getAttribute('aria-label') || element.textContent;
          return `${name?.trim()} ${Math.round(width)}x${Math.round(height)}`;
        }),
    );

  // One move of the user's own, so the chips on the timeline and in the
  // Moves list are there to be measured.
  await page.keyboard.press('r');
  await expect(page.locator('#timeline-moves .move-chip')).toHaveCount(1);

  for (const viewport of [
    { width: 390, height: 844 },
    { width: 1200, height: 1000 },
    { width: 1200, height: 700 },
  ]) {
    await page.setViewportSize(viewport);
    const at = `${viewport.width}x${viewport.height}`;
    expect(await small(), at).toEqual([]);

    for (const panel of ['moves', 'session', 'turn']) {
      await page.locator(`#details-${panel}`).click();
      expect(await small(), `${at} ${panel}`).toEqual([]);
      await page.locator(`#details-${panel}`).click();
    }

    await page.locator('#paint').click();
    expect(await small(), `${at} Paint`).toEqual([]);
    await page.locator('#paint-cancel').click();

    await openSettings(page);
    expect(await small(), `${at} Settings`).toEqual([]);
    await page.keyboard.press('Escape');

    await page.keyboard.press('ControlOrMeta+k');
    await page.locator('#command-input').fill('r');
    expect(await small(), `${at} menu`).toEqual([]);
    await page.keyboard.press('Escape');
  }
});

/**
 * The middles of every sticker of one solved face, in page coordinates.
 *
 * Connected components rather than anything looser: net cells sit a few pixels
 * apart and ring stickers a good deal further, so no single "near enough"
 * radius separates the one without splitting the other.
 */
async function faceStickers(
  page: Page,
  selector: string,
  color: readonly number[],
): Promise<{ x: number; y: number }[]> {
  // Whatever the canvas's box has just become, its buffer has caught up.
  await probeCanvas(page);
  return page.evaluate(([from, wanted]) => {
    const canvas = document.querySelector<HTMLCanvasElement>(from)!;
    const context = canvas.getContext('2d')!;
    const data = context.getImageData(0, 0, canvas.width, canvas.height).data;

    const step = 3;
    const cols = Math.floor(canvas.width / step);
    const rows = Math.floor(canvas.height / step);
    const on = new Uint8Array(cols * rows);
    for (let r = 0; r < rows; r += 1) {
      for (let c = 0; c < cols; c += 1) {
        const i = (r * step * canvas.width + c * step) * 4;
        on[r * cols + c] =
          data[i] === wanted[0] &&
          data[i + 1] === wanted[1] &&
          data[i + 2] === wanted[2]
            ? 1
            : 0;
      }
    }

    const found: { x: number; y: number }[] = [];
    for (let seed = 0; seed < on.length; seed += 1) {
      if (on[seed] === 0) continue;

      let n = 0;
      let sx = 0;
      let sy = 0;
      const stack = [seed];
      on[seed] = 0;
      while (stack.length > 0) {
        const at = stack.pop()!;
        const c = at % cols;
        const r = (at - c) / cols;
        n += 1;
        sx += c;
        sy += r;
        for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const nc = c + dc;
          const nr = r + dr;
          if (nc < 0 || nr < 0 || nc >= cols || nr >= rows) continue;
          if (on[nr * cols + nc] === 0) continue;
          on[nr * cols + nc] = 0;
          stack.push(nr * cols + nc);
        }
      }
      if (n < 12) continue;

      const box = canvas.getBoundingClientRect();
      found.push({
        x: box.left + (((sx / n) * step) * box.width) / canvas.width,
        y: box.top + (((sy / n) * step) * box.height) / canvas.height,
      });
    }
    return found;
  }, [selector, color] as const);
}

test('the flat view toggles between net and rings in every region mode', async ({
  page,
}) => {
  const stage = page.locator('#stage');
  const netCanvas = page.locator('#view-net');
  const ringsCanvas = page.locator('#view-rings');
  const both = page.locator('[data-view="both"]');
  const cube = page.locator('[data-view="3d"]');
  const flat = page.locator('[data-view="2d"]');
  const net = page.locator('[data-flat="net"]');
  const rings = page.locator('[data-flat="rings"]');

  const frame = async (): Promise<string> =>
    netCanvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());

  await expect(stage).toHaveAttribute('data-view-mode', 'both');
  await expect(stage).toHaveAttribute('data-flat-style', 'net');
  await expect(ringsCanvas).toBeHidden();
  await probeCanvas(page);
  const bothNet = await frame();

  // Both follows the toggle, so the cube keeps its place and the drawing
  // beside it changes.
  await rings.click();
  await expect(rings).toHaveAttribute('aria-pressed', 'true');
  await expect(stage).toHaveAttribute('data-view-mode', 'both');
  await expect(stage).toHaveAttribute('data-flat-style', 'rings');
  await expect(netCanvas).toBeHidden();
  await expect(ringsCanvas).toBeVisible();

  // The style is the other axis: it survives the flat region going away and
  // coming back, and the toggle is out of the way while it means nothing.
  await cube.click();
  await expect(rings).toBeHidden();
  await expect(ringsCanvas).toBeHidden();
  await flat.click();
  await expect(rings).toBeVisible();
  await expect(rings).toHaveAttribute('aria-pressed', 'true');
  await expect(stage).toHaveAttribute('data-flat-style', 'rings');
  await expect(ringsCanvas).toBeVisible();

  // And the net put away and brought back is drawn again as it was.
  await net.click();
  await expect(stage).toHaveAttribute('data-flat-style', 'net');
  await both.click();
  await expect(netCanvas).toBeVisible();
  await expect.poll(frame).toBe(bothNet);
});

test('the flat view can show the net over the rings, and both take drags', async ({
  page,
}) => {
  const stage = page.locator('#stage');
  const netCanvas = page.locator('#view-net');

  await page.locator('[data-view="2d"]').click();
  await page.locator('[data-flat="both"]').click();
  await expect(stage).toHaveAttribute('data-flat-style', 'both');

  // Nine white stickers in each drawing, each on a canvas of its own.
  expect(await faceStickers(page, '#view-net', WHITE)).toHaveLength(9);
  const onRings = (await faceStickers(page, '#view-rings', WHITE)).sort(
    (a, b) => a.x - b.x || a.y - b.y,
  );
  expect(onRings).toHaveLength(9);

  const frame = async (): Promise<string> =>
    netCanvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());
  const resting = await frame();

  // A drag on one drawing turns the cube, so the other moves too: one
  // gesture, one rotation, every view that is up showing it.
  await page.mouse.move(onRings[0]!.x, onRings[0]!.y);
  await page.mouse.down();
  await page.mouse.move(onRings[4]!.x, onRings[4]!.y, { steps: 16 });
  await page.mouse.up();

  await expect.poll(frame).not.toBe(resting);
  await expect
    .poll(async () => (await faceStickers(page, '#view-net', WHITE)).length)
    .toBe(9);
  await expect
    .poll(async () => (await faceStickers(page, '#view-rings', WHITE)).length)
    .toBe(9);

  await pressInSettings(page, '#reset');
});

test('dragging a sticker round its ring turns the cube', async ({ page }) => {
  const canvas = page.locator('#view-rings');

  await page.locator('[data-view="2d"]').click();
  await page.locator('[data-flat="rings"]').click();
  await expect(page.locator('#stage')).toHaveAttribute(
    'data-flat-style',
    'rings',
  );

  // The nine stickers of one face sit on a three-by-three patch of crossings,
  // so two of them a step apart share a ring: dragging between them is a drag
  // along that ring, and two slots is past the point a release commits.
  const white = await faceStickers(page, '#view-rings', WHITE);
  const frame = async (): Promise<string> =>
    canvas.evaluate((element) => (element as HTMLCanvasElement).toDataURL());
  const resting = await frame();
  expect(white).toHaveLength(9);
  white.sort((a, b) => a.x - b.x || a.y - b.y);

  await page.mouse.move(white[0]!.x, white[0]!.y);
  await page.mouse.down();
  await page.mouse.move(white[4]!.x, white[4]!.y, { steps: 16 });
  await page.mouse.up();

  await expect.poll(frame).not.toBe(resting);

  // Every sticker is back on a slot once it settles, which is what makes the
  // settled picture one the resting builder could have drawn: nine of each
  // color, none of them part way round a loop.
  await expect
    .poll(async () => (await faceStickers(page, '#view-rings', WHITE)).length)
    .toBe(9);

  await pressInSettings(page, '#reset');
});
