import { expect, test, type Page } from '@playwright/test';

// Rendered scene contract v3 as RGBA tuples: the canvas holds a square 3D
// region showing three differently colored faces of a solved cube, and below
// it a net showing all six faces flat, over a solid background. A winding,
// culling, channel-order, or domain regression shows up as the wrong color
// here rather than as a plausible picture.
const BACKGROUND = [32, 32, 32, 255];
const WHITE = [255, 255, 255, 255]; // +Y up
const YELLOW = [255, 213, 0, 255]; // -Y down
const GREEN = [0, 155, 72, 255]; // +Z front
const BLUE = [0, 70, 173, 255]; // -Z back
const RED = [183, 18, 52, 255]; // +X right
const ORANGE = [255, 88, 0, 255]; // -X left

// Sample points as fractions of the square 3D region, derived from the
// projected centroid of each visible face. They are region-relative rather
// than canvas-relative, which is what makes them independent of the canvas
// aspect ratio.
const UP_SAMPLE = [0.5, 0.29] as const;
const FRONT_SAMPLE = [0.31, 0.61] as const;
const RIGHT_SAMPLE = [0.69, 0.61] as const;

// Gaps between neighbouring stickers. Nothing is drawn there, so they read as
// background — but only once the gap is comfortably wider than the
// anti-aliased edges around it.
const SEAM_SAMPLES = [
  [0.377, 0.645],
  [0.313, 0.534],
  [0.564, 0.321],
] as const;
const SEAM_MIN_SIZE = 1024;

// The solved net, written out by hand rather than derived from the engine's
// own cell mapping, in the order the probe reads it.
const NET_BLOCKS = [
  { column: 1, row: 0, color: WHITE },
  { column: 0, row: 1, color: ORANGE },
  { column: 1, row: 1, color: GREEN },
  { column: 2, row: 1, color: RED },
  { column: 3, row: 1, color: BLUE },
  { column: 1, row: 2, color: YELLOW },
] as const;
const CUBE_SIZE = 3;

// Layout fractions of the shorter canvas side, matching
// docs/tasks/04-rubiks-cube-domain.md. Repeated here on purpose so a layout
// change has to be made deliberately in both places.
const CUBE_REGION_SIDE = 0.58;
const CUBE_REGION_TOP = 0.01;
const NET_FACE_SIDE = 0.12;
const NET_TOP = 0.62;

type CanvasProbe = {
  readonly width: number;
  readonly height: number;
  readonly cssWidth: number;
  readonly cssHeight: number;
  readonly devicePixelRatio: number;
  readonly up: number[];
  readonly front: number[];
  readonly right: number[];
  readonly seams: number[][];
  readonly net: number[][];
  readonly corners: number[][];
};

/** Reads the drawing buffer size and the contract pixels from the page. */
async function probeCanvas(page: Page): Promise<CanvasProbe> {
  return page.evaluate(
    (config) => {
      const canvas = document.querySelector('canvas');
      if (!canvas) throw new Error('Canvas element is missing.');

      const context = canvas.getContext('2d');
      if (!context) throw new Error('2D context is missing.');

      const read = (x: number, y: number): number[] => {
        const column = Math.min(Math.max(Math.round(x), 0), canvas.width - 1);
        const row = Math.min(Math.max(Math.round(y), 0), canvas.height - 1);
        return Array.from(context.getImageData(column, row, 1, 1).data);
      };

      const unit = Math.min(canvas.width, canvas.height);

      const cubeSide = config.cubeRegionSide * unit;
      const cubeX = (canvas.width - cubeSide) / 2;
      const cubeY = config.cubeRegionTop * unit;
      const inCube = ([fx, fy]: readonly number[]): number[] =>
        read(cubeX + fx * cubeSide, cubeY + fy * cubeSide);

      const faceSide = config.netFaceSide * unit;
      const netX = (canvas.width - faceSide * 4) / 2;
      const netY = config.netTop * unit;
      const cell = faceSide / config.cubeSize;

      const net: number[][] = [];
      for (const block of config.netBlocks) {
        const originX = netX + block.column * faceSide;
        const originY = netY + block.row * faceSide;
        for (let row = 0; row < config.cubeSize; row += 1) {
          for (let col = 0; col < config.cubeSize; col += 1) {
            net.push(
              read(originX + (col + 0.5) * cell, originY + (row + 0.5) * cell),
            );
          }
        }
      }

      return {
        width: canvas.width,
        height: canvas.height,
        cssWidth: canvas.clientWidth,
        cssHeight: canvas.clientHeight,
        devicePixelRatio: window.devicePixelRatio,
        up: inCube(config.upSample),
        front: inCube(config.frontSample),
        right: inCube(config.rightSample),
        seams: config.seamSamples.map(inCube),
        net,
        corners: [
          read(0, 0),
          read(canvas.width - 1, 0),
          read(0, canvas.height - 1),
          read(canvas.width - 1, canvas.height - 1),
        ],
      };
    },
    {
      upSample: UP_SAMPLE,
      frontSample: FRONT_SAMPLE,
      rightSample: RIGHT_SAMPLE,
      seamSamples: SEAM_SAMPLES,
      netBlocks: NET_BLOCKS,
      cubeSize: CUBE_SIZE,
      cubeRegionSide: CUBE_REGION_SIDE,
      cubeRegionTop: CUBE_REGION_TOP,
      netFaceSide: NET_FACE_SIDE,
      netTop: NET_TOP,
    },
  );
}

/** The 54 net cells in probe order. */
function expectedNet(): number[][] {
  const cells: number[][] = [];
  for (const block of NET_BLOCKS) {
    for (let i = 0; i < CUBE_SIZE * CUBE_SIZE; i += 1) {
      cells.push([...block.color]);
    }
  }
  return cells;
}

function assertSceneContract(probe: CanvasProbe): void {
  expect(probe.up).toEqual(WHITE);
  expect(probe.front).toEqual(GREEN);
  expect(probe.right).toEqual(RED);

  // All 54 cells at once, so a failure names every wrong sticker rather than
  // stopping at the first.
  expect(probe.net).toEqual(expectedNet());

  for (const corner of probe.corners) {
    expect(corner).toEqual(BACKGROUND);
  }

  // A seam is only a few pixels wide, so below this size anti-aliasing
  // reaches the sample point and the check is not meaningful.
  if (Math.min(probe.width, probe.height) >= SEAM_MIN_SIZE) {
    for (const seam of probe.seams) {
      expect(seam).toEqual(BACKGROUND);
    }
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
