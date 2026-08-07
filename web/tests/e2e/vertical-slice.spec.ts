import { expect, test, type Page } from '@playwright/test';

// Rendered scene contract v2 as RGBA tuples: the fixed camera shows three
// differently colored cube faces over a solid background, so a winding,
// culling, or channel-order regression shows up as the wrong color here.
const BACKGROUND = [32, 32, 32, 255];
const UP = [255, 255, 255, 255]; // +Y white
const FRONT = [0, 155, 72, 255]; // +Z green
const RIGHT = [183, 18, 52, 255]; // +X red

// Sample points as fractions of the drawing buffer, derived from the
// projected centroid of each visible face. Only valid for a square buffer,
// which the stylesheet guarantees with aspect-ratio: 1 / 1.
const UP_SAMPLE = [0.5, 0.29] as const;
const FRONT_SAMPLE = [0.31, 0.61] as const;
const RIGHT_SAMPLE = [0.69, 0.61] as const;

type CanvasProbe = {
  readonly width: number;
  readonly height: number;
  readonly cssWidth: number;
  readonly cssHeight: number;
  readonly devicePixelRatio: number;
  readonly up: number[];
  readonly front: number[];
  readonly right: number[];
  readonly corners: number[][];
};

/** Reads the drawing buffer size and the contract pixels from the page. */
async function probeCanvas(page: Page): Promise<CanvasProbe> {
  return page.evaluate(
    ([upSample, frontSample, rightSample]) => {
      const canvas = document.querySelector('canvas');
      if (!canvas) throw new Error('Canvas element is missing.');

      const context = canvas.getContext('2d');
      if (!context) throw new Error('2D context is missing.');

      const read = (x: number, y: number): number[] =>
        Array.from(context.getImageData(x, y, 1, 1).data);

      const sample = ([fx, fy]: readonly number[]): number[] =>
        read(
          Math.round(fx * (canvas.width - 1)),
          Math.round(fy * (canvas.height - 1)),
        );

      return {
        width: canvas.width,
        height: canvas.height,
        cssWidth: canvas.clientWidth,
        cssHeight: canvas.clientHeight,
        devicePixelRatio: window.devicePixelRatio,
        up: sample(upSample),
        front: sample(frontSample),
        right: sample(rightSample),
        corners: [
          read(0, 0),
          read(canvas.width - 1, 0),
          read(0, canvas.height - 1),
          read(canvas.width - 1, canvas.height - 1),
        ],
      };
    },
    [UP_SAMPLE, FRONT_SAMPLE, RIGHT_SAMPLE],
  );
}

function assertSceneContract(probe: CanvasProbe): void {
  expect(probe.width, 'the contract is defined for a square buffer').toBe(
    probe.height,
  );
  expect(probe.up).toEqual(UP);
  expect(probe.front).toEqual(FRONT);
  expect(probe.right).toEqual(RIGHT);
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
