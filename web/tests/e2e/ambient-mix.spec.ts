import { expect, test } from '@playwright/test';

import { expectedNet, probeCanvas } from './sceneContract.ts';

/** The largest cube, which is where a narrow pattern showed up worst. */
const SIZE = 9;

test('a watched big cube is mixed all over rather than in a few bands', async ({
  page,
}) => {
  await page.setViewportSize({ width: 1200, height: 1200 });
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  await page.locator('#cube-size').fill(String(SIZE));
  await page.locator('#cube-size').dispatchEvent('change');
  // The quick end of the slider, so a watch worth measuring fits in a test.
  await page.locator('#speed').fill('4');
  await page.locator('#speed').dispatchEvent('input');

  const solved = await probeCanvas(page, SIZE);
  expect(solved.net).toEqual(expectedNet(SIZE));

  await page.locator('#ambient').click();
  await page.waitForTimeout(6000);

  // Read off the drawing rather than out of the engine: what was wrong with
  // the four-move pattern was not what it did to the cube but what it looked
  // like, and this is that measured.
  const watched = await probeCanvas(page, SIZE);
  const changed = watched.net.filter(
    (cell, index) => String(cell) !== String(solved.net[index]),
  ).length;

  // Most of the 486 cells, not a few bands of them.
  expect(changed).toBeGreaterThan(watched.net.length / 2);

  // And the cube that was borrowed is given back whole when watching stops.
  await page.locator('#ambient').click();
  await expect(page.locator('#ambient')).toHaveAttribute(
    'aria-pressed',
    'false',
  );
  const returned = await probeCanvas(page, SIZE);
  expect(returned.net).toEqual(expectedNet(SIZE));
});
