import { expect, test, type Page } from '@playwright/test';

// Rendered scene contract colors as RGBA tuples.
const BACKGROUND = [32, 32, 32, 255];
const RECTANGLE = [230, 57, 70, 255];

type CanvasProbe = {
  readonly width: number;
  readonly height: number;
  readonly cssWidth: number;
  readonly cssHeight: number;
  readonly devicePixelRatio: number;
  readonly center: number[];
  readonly corners: number[][];
};

/** Reads the drawing buffer size and the contract pixels from the page. */
async function probeCanvas(page: Page): Promise<CanvasProbe> {
  return page.evaluate(() => {
    const canvas = document.querySelector('canvas');
    if (!canvas) throw new Error('Canvas element is missing.');

    const context = canvas.getContext('2d');
    if (!context) throw new Error('2D context is missing.');

    const read = (x: number, y: number): number[] =>
      Array.from(context.getImageData(x, y, 1, 1).data);

    return {
      width: canvas.width,
      height: canvas.height,
      cssWidth: canvas.clientWidth,
      cssHeight: canvas.clientHeight,
      devicePixelRatio: window.devicePixelRatio,
      center: read(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2)),
      corners: [
        read(0, 0),
        read(canvas.width - 1, 0),
        read(0, canvas.height - 1),
        read(canvas.width - 1, canvas.height - 1),
      ],
    };
  });
}

function assertSceneContract(probe: CanvasProbe): void {
  expect(probe.center).toEqual(RECTANGLE);
  for (const corner of probe.corners) {
    expect(corner).toEqual(BACKGROUND);
  }
}

function assertDrawingBufferMatchesCss(probe: CanvasProbe): void {
  expect(probe.width).toBe(
    Math.max(1, Math.round(probe.cssWidth * probe.devicePixelRatio)),
  );
  expect(probe.height).toBe(
    Math.max(1, Math.round(probe.cssHeight * probe.devicePixelRatio)),
  );
}

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
