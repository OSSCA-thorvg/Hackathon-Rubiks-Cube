import { expect, test } from '@playwright/test';

import {
  assertDrawingBufferMatchesCss,
  assertSceneContract,
  probeCanvas,
  SEAM_MIN_SIZE,
} from './sceneContract.ts';

test('renders the scene contract and re-verifies it after resize', async ({
  page,
}) => {
  const consoleErrors: string[] = [];
  page.on('console', (message) => {
    if (message.type() === 'error') consoleErrors.push(message.text());
  });

  const pageErrors: string[] = [];
  page.on('pageerror', (error) => pageErrors.push(String(error)));

  const failedRequests: string[] = [];
  page.on('requestfailed', (request) => failedRequests.push(request.url()));

  // Every JavaScript and WASM response must succeed; at least one of each
  // proves the engine actually travelled over the network.
  const assetResponses: { url: string; ok: boolean }[] = [];
  page.on('response', (response) => {
    if (/\.(js|wasm)(\?|$)/.test(response.url())) {
      assetResponses.push({ url: response.url(), ok: response.ok() });
    }
  });

  // Relative to baseURL so the Pages subpath is preserved; '/' would
  // resolve to the host root and dodge base path regressions.
  await page.goto('./');

  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  const initial = await probeCanvas(page);
  assertDrawingBufferMatchesCss(initial);
  assertSceneContract(initial);

  await page.setViewportSize({ width: 480, height: 360 });
  await expect
    .poll(async () => (await probeCanvas(page)).width)
    .not.toBe(initial.width);

  const resized = await probeCanvas(page);
  assertDrawingBufferMatchesCss(resized);
  assertSceneContract(resized);

  // The canvas is min(80vw, 70vh), so a tall viewport is what makes the
  // seams wide enough to sample. Asserting the size first keeps that branch
  // from silently skipping if the stylesheet changes.
  await page.setViewportSize({ width: 1600, height: 1600 });
  await expect
    .poll(async () => (await probeCanvas(page)).width)
    .toBeGreaterThanOrEqual(SEAM_MIN_SIZE);

  const enlarged = await probeCanvas(page);
  assertDrawingBufferMatchesCss(enlarged);
  assertSceneContract(enlarged);

  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');

  expect(consoleErrors).toEqual([]);
  expect(pageErrors).toEqual([]);
  expect(failedRequests).toEqual([]);
  expect(assetResponses.filter((r) => !r.ok)).toEqual([]);
  expect(
    assetResponses.some((r) => r.url.endsWith('.wasm')),
    'a WASM asset response is expected',
  ).toBe(true);
  expect(
    assetResponses.some((r) => r.url.endsWith('.js')),
    'a JavaScript asset response is expected',
  ).toBe(true);
});
