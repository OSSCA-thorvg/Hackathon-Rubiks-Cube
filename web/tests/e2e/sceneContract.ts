import { expect, type Page } from '@playwright/test';

// Rendered scene contract v4 as RGBA tuples: the cube's canvas holds a square
// 3D region showing three differently colored faces of a cube under one
// light, and the net's canvas beside it all six faces flat and unlit, each
// over a solid background. A winding, culling, channel-order, or domain regression shows up
// as the wrong color here rather than as a plausible picture -- and since v4
// a normal the wrong way round shows up as the wrong brightness too.
export const BACKGROUND = [32, 32, 32, 255];
export const WHITE = [255, 255, 255, 255]; // +Y up
/** The classic white as the 3D view lights it: the net keeps WHITE. */
export const PAPER_WHITE = [216, 216, 216, 255];
export const YELLOW = [255, 213, 0, 255]; // -Y down
export const GREEN = [0, 155, 72, 255]; // +Z front
export const BLUE = [0, 70, 173, 255]; // -Z back
export const RED = [183, 18, 52, 255]; // +X right
export const ORANGE = [255, 88, 0, 255]; // -X left

/**
 * The cubie body: the cut surfaces a turn opens, and what shows between the
 * stickers, painted under them as each slab's silhouette.
 */
export const BODY = [70, 74, 82, 255];

// The brightness of each plane of the cube under the light, a byte out of
// 255, worked out by hand in docs/tasks/04-rubiks-cube-domain.md (contract
// v4). The three planes turned away from the light sit at the ambient floor.
export const LIT_UP = 275; // +Y (white clamps to 255)
export const LIT_FRONT = 230; // +Z
export const LIT_RIGHT = 211; // +X
export const LIT_AWAY = 191; // -Y, -X, -Z: the ambient floor

/**
 * The cut faces a turn opens lie one layer in, on planes of their own: at
 * x, y or z = +1/3 or -1/3, facing +X, +Y or +Z. During a scramble any of the
 * six may be the one on show.
 */
export const LIT_CUTS = [218, 224, 273, 270, 235, 239];

/**
 * A colour at a brightness, in the integer arithmetic the engine uses. The
 * brightness may pass 255 and each channel stops there.
 */
export function lit(color: readonly number[], brightness: number): number[] {
  const channel = (value: number): number =>
    Math.min(255, Math.floor((value * brightness + 127) / 255));
  return [channel(color[0]!), channel(color[1]!), channel(color[2]!), color[3]!];
}

/**
 * A sticker's colour with its chroma raised, as the engine does before it
 * lights it: gray = (2126 r + 7152 g + 722 b + 5000) / 10000, each channel
 * gray + ((c - gray) * 125 + 50) / 100 rounded away from grey, clamped.
 */
export function saturated(color: readonly number[]): number[] {
  const gray = Math.floor(
    (2126 * color[0]! + 7152 * color[1]! + 722 * color[2]! + 5000) / 10000,
  );
  const channel = (value: number): number => {
    const offset = (value - gray) * 125;
    const rounded =
      offset >= 0
        ? Math.floor((offset + 50) / 100)
        : -Math.floor((-offset + 50) / 100);
    return Math.min(255, Math.max(0, gray + rounded));
  };
  return [channel(color[0]!), channel(color[1]!), channel(color[2]!), color[3]!];
}

/** The six faces as the lit 3D view shows them on a solved cube. */
export const UP_LIT = lit(saturated(PAPER_WHITE), LIT_UP);
export const FRONT_LIT = lit(saturated(GREEN), LIT_FRONT);
export const RIGHT_LIT = lit(saturated(RED), LIT_RIGHT);
export const BACK_LIT = lit(saturated(BLUE), LIT_AWAY);

/**
 * Whether a pixel is a colour to within `slack` per channel.
 *
 * The lit stickers are gradients following their plane's brightness, and
 * the gradient's rounding can move a byte by one; a wrong face or a flipped
 * normal is tens of units away.
 */
export function near(
  pixel: readonly number[],
  color: readonly number[],
  slack = 1,
): boolean {
  return (
    Math.abs(pixel[0]! - color[0]!) <= slack &&
    Math.abs(pixel[1]! - color[1]!) <= slack &&
    Math.abs(pixel[2]! - color[2]!) <= slack &&
    pixel[3] === color[3]
  );
}

/**
 * Whether a pixel is the cubie body at any brightness it can have: the body
 * in a seam as it is, or a cut face lit by its plane. Read at one of the cut
 * probes it says whether a layer is part way round.
 */
export function isBody(pixel: readonly number[]): boolean {
  // The body by its hue rather than by a table of shades: BODY is
  // (70, 74, 82), and every brightness of it, and every white glint laid
  // over it, keeps the channels in that order with the blue-green step twice
  // the green-red step. A glint on a cut face can be as strong as the lamp's
  // whole specular, so its amount is not bounded here; what is is the spread,
  // which shrinks towards white and is zero for a white sticker, and the
  // darkness, which rules out the ground.
  const [r, g, b, a] = pixel as [number, number, number, number];
  if (a !== BODY[3]) return false;
  if (r > g || g > b) return false;
  const spread = b - r;
  if (spread < 2 || spread > 16) return false;
  if (Math.abs(b - g - 2 * (g - r)) > 3) return false;
  return r >= 40 && r <= 220;
}

/**
 * How much more of the 3D region shows the body than a resting cube does
 * before a layer counts as part way round.
 *
 * The seams are a few percent of the region and move a little as the cube
 * turns (about 0.3% either way, measured); a cut face opened by a turn is a
 * solid patch worth about 1% of the region at 45 degrees. The margin sits
 * between the two.
 */
export const CUT_SHARE_MARGIN = 0.005;

// Sample points as fractions of the square 3D region, derived from the
// projected centroid of each visible face. They are region-relative rather
// than canvas-relative, which is what makes them independent of the canvas
// aspect ratio, and named for where they sit on screen rather than for a face,
// because which face shows there depends on the viewpoint.
export const TOP_SAMPLE = [0.5, 0.29] as const;
export const LEFT_SAMPLE = [0.31, 0.61] as const;
export const RIGHT_SAMPLE = [0.69, 0.61] as const;

/**
 * Center of the front face's right column, which is what a person grabs to
 * turn the right-hand layer. Also region-relative; the value is the projected
 * centroid of that sticker, the same way the samples above were derived.
 */
export const FRONT_RIGHT_COLUMN = [0.433, 0.694] as const;

// Gaps between neighbouring stickers. The cubie body shows there, painted
// under the stickers, so they read as BODY — but only once the gap is
// comfortably wider than the anti-aliased edges around it, which it is from
// a region this many pixels across.
export const SEAM_SAMPLES = [
  [0.377, 0.645],
  [0.313, 0.534],
  [0.564, 0.321],
] as const;
export const SEAM_MIN_REGION = 594;

/**
 * Where a cut face shows part way through a turn, as fractions of the 3D
 * region: a point on the still layers' cut, clear of the turned layer, that
 * at rest lies behind a front-face sticker. The world points and the
 * ray-casting behind them are in docs/tasks/04-rubiks-cube-domain.md; the
 * fractions are their projection from the home viewpoint.
 */
export const CUT_R_SAMPLE = [0.3927, 0.474] as const; // (1/3, 0.70, 0.85)
export const CUT_U_SAMPLE = [0.4688, 0.6059] as const; // (0.70, 1/3, 0.85)

// The solved net, written out by hand rather than derived from the engine's
// own cell mapping, in the order the probe reads it.
export const NET_BLOCKS = [
  { column: 1, row: 0, color: WHITE },
  { column: 0, row: 1, color: ORANGE },
  { column: 1, row: 1, color: GREEN },
  { column: 2, row: 1, color: RED },
  { column: 3, row: 1, color: BLUE },
  { column: 1, row: 2, color: YELLOW },
] as const;
export const CUBE_SIZE = 3;

/** Where each face sits in the cross, by name, for aiming at a net cell. */
export const FRONT_BLOCK = { column: 1, row: 1 } as const;

export const NET_COLUMNS = 4;
export const NET_ROWS = 3;
/** Fractions the net-only layout grows to before it runs out of canvas. */
export const NET_ONLY_WIDTH = 0.88;
export const NET_ONLY_HEIGHT = 0.78;

/**
 * Side of the 3D region as a fraction of its canvas's shorter side, centred
 * both ways: the cube is alone on a canvas of its own. Matching
 * engine/src/graphics/Layout.hpp, and repeated here on purpose so a layout
 * change has to be made deliberately in both places.
 */
export const CUBE_REGION_SIDE = 0.84;

/**
 * Fraction of the 3D region's width a drag has to cover for a quarter turn,
 * mirroring the engine's own drag sensitivity.
 */
export const QUARTER_TURN_FRACTION = 0.5;

/** Resolution of the coarse grid used to tell two renderings apart. */
const SIGNATURE_STEPS = 12;

/** A canvas's drawing buffer and where its box is on the page. */
export type CanvasFrame = {
  readonly width: number;
  readonly height: number;
  readonly cssWidth: number;
  readonly cssHeight: number;
  readonly box: { left: number; top: number; width: number; height: number };
};

/**
 * What the contract reads: the cube's canvas and what is on it, and the net's.
 *
 * The frame fields at the top level are the cube's canvas, which is what
 * most of the contract is about; the net's canvas has its own.
 */
export type CanvasProbe = CanvasFrame & {
  readonly devicePixelRatio: number;
  /** The net's canvas, all zeros while the net is not on show. */
  readonly netCanvas: CanvasFrame;
  readonly top: number[];
  readonly left: number[];
  readonly right: number[];
  readonly seams: number[][];
  /** The two cut-face probes: a lit body mid-turn, a sticker at rest. */
  readonly cutR: number[];
  readonly cutU: number[];
  /** The net's cells in NET_BLOCKS order, or none while it is not on show. */
  readonly net: number[][];
  readonly corners: number[][];
  /** A coarse sweep of the 3D region, for comparing two frames. */
  readonly cubeGrid: number[][];
  /**
   * The share of the 3D region's pixels that show the body, by the same hue
   * rule as isBody(). At rest that is the seams, a few percent; a cut face
   * opened by a turn is a solid patch and lifts it well clear of them.
   */
  readonly bodyShare: number;
};

/**
 * Reads the drawing buffer size and the contract pixels from the page.
 *
 * `size` is how many cells one net face is read as, which is the cube on the
 * table rather than a setting: everything else in the contract is a layout
 * fraction and does not move when the cube is replaced.
 */
export async function probeCanvas(
  page: Page,
  size: number = CUBE_SIZE,
): Promise<CanvasProbe> {
  // Two frames first. A canvas whose box has just changed -- a view switched
  // on, a panel opened beside the stage -- gets its new buffer in the
  // rendering step after, and a reading taken before it would measure the
  // new box against the old buffer.
  await page.evaluate(
    () =>
      new Promise<void>((resolve) =>
        requestAnimationFrame(() => requestAnimationFrame(() => resolve())),
      ),
  );
  return page.evaluate(
    (config) => {
      const canvasOf = (selector: string) => {
        const element = document.querySelector<HTMLCanvasElement>(selector);
        if (!element) throw new Error(`${selector} is missing.`);
        const context = element.getContext('2d');
        if (!context) throw new Error(`${selector} has no 2D context.`);
        const read = (x: number, y: number): number[] => {
          const column = Math.min(Math.max(Math.round(x), 0), element.width - 1);
          const row = Math.min(Math.max(Math.round(y), 0), element.height - 1);
          return Array.from(context.getImageData(column, row, 1, 1).data);
        };
        const box = element.getBoundingClientRect();
        const frame = {
          width: element.width,
          height: element.height,
          cssWidth: element.clientWidth,
          cssHeight: element.clientHeight,
          box: {
            left: box.left,
            top: box.top,
            width: box.width,
            height: box.height,
          },
        };
        return { canvas: element, context, read, frame };
      };

      const cube = canvasOf('#view');
      const { canvas, context, read } = cube;

      const unit = Math.min(canvas.width, canvas.height);

      const cubeSide = config.cubeRegionSide * unit;
      const cubeX = (canvas.width - cubeSide) / 2;
      const cubeY = (canvas.height - cubeSide) / 2;
      const inCube = ([fx, fy]: readonly number[]): number[] =>
        read(cubeX + fx * cubeSide, cubeY + fy * cubeSide);

      // The net alone on its canvas, grown until one extent runs out first.
      const flat = canvasOf('#view-net');
      const shown = flat.frame.cssWidth > 0 && flat.frame.cssHeight > 0;
      const faceSide = Math.min(
        (config.netOnlyWidth * flat.canvas.width) / 4,
        (config.netOnlyHeight * flat.canvas.height) / 3,
      );
      const netX = (flat.canvas.width - faceSide * 4) / 2;
      const netY = (flat.canvas.height - faceSide * 3) / 2;
      const cell = faceSide / config.cubeSize;

      const net: number[][] = [];
      for (const block of shown ? config.netBlocks : []) {
        const originX = netX + block.column * faceSide;
        const originY = netY + block.row * faceSide;
        for (let row = 0; row < config.cubeSize; row += 1) {
          for (let col = 0; col < config.cubeSize; col += 1) {
            net.push(
              flat.read(
                originX + (col + 0.5) * cell,
                originY + (row + 0.5) * cell,
              ),
            );
          }
        }
      }

      // Every third pixel of the region, so a cut face counts by its area
      // rather than by whichever coarse cells happen to land on it.
      const region = context.getImageData(
        Math.round(cubeX),
        Math.round(cubeY),
        Math.round(cubeSide),
        Math.round(cubeSide),
      );
      let bodyPixels = 0;
      let sampled = 0;
      for (let y = 0; y < region.height; y += 3) {
        for (let x = 0; x < region.width; x += 3) {
          const i = (y * region.width + x) * 4;
          const r = region.data[i]!;
          const g = region.data[i + 1]!;
          const b = region.data[i + 2]!;
          sampled += 1;
          if (r > g || g > b) continue;
          const spread = b - r;
          if (spread < 2 || spread > 16) continue;
          if (Math.abs(b - g - 2 * (g - r)) > 3) continue;
          if (r >= 40 && r <= 220) bodyPixels += 1;
        }
      }
      const bodyShare = sampled > 0 ? bodyPixels / sampled : 0;

      const cubeGrid: number[][] = [];
      for (let row = 0; row < config.signatureSteps; row += 1) {
        for (let col = 0; col < config.signatureSteps; col += 1) {
          cubeGrid.push(
            inCube([
              (col + 0.5) / config.signatureSteps,
              (row + 0.5) / config.signatureSteps,
            ]),
          );
        }
      }

      const zero = {
        width: 0,
        height: 0,
        cssWidth: 0,
        cssHeight: 0,
        box: { left: 0, top: 0, width: 0, height: 0 },
      };

      return {
        ...cube.frame,
        devicePixelRatio: window.devicePixelRatio,
        netCanvas: shown ? flat.frame : zero,
        top: inCube(config.topSample),
        left: inCube(config.leftSample),
        right: inCube(config.rightSample),
        seams: config.seamSamples.map(inCube),
        cutR: inCube(config.cutRSample),
        cutU: inCube(config.cutUSample),
        net,
        corners: [
          read(0, 0),
          read(canvas.width - 1, 0),
          read(0, canvas.height - 1),
          read(canvas.width - 1, canvas.height - 1),
        ],
        cubeGrid,
        bodyShare,
      };
    },
    {
      topSample: TOP_SAMPLE,
      leftSample: LEFT_SAMPLE,
      rightSample: RIGHT_SAMPLE,
      seamSamples: SEAM_SAMPLES,
      cutRSample: CUT_R_SAMPLE,
      cutUSample: CUT_U_SAMPLE,
      netBlocks: NET_BLOCKS,
      cubeSize: size,
      cubeRegionSide: CUBE_REGION_SIDE,
      netOnlyWidth: NET_ONLY_WIDTH,
      netOnlyHeight: NET_ONLY_HEIGHT,
      signatureSteps: SIGNATURE_STEPS,
    },
  );
}

/** The 6N^2 net cells of a solved cube, in probe order. */
export function expectedNet(size: number = CUBE_SIZE): number[][] {
  const cells: number[][] = [];
  for (const block of NET_BLOCKS) {
    for (let i = 0; i < size * size; i += 1) {
      cells.push([...block.color]);
    }
  }
  return cells;
}

/** Side of the 3D region in drawing buffer pixels. */
export function cubeRegionSide(probe: CanvasProbe): number {
  return CUBE_REGION_SIDE * Math.min(probe.width, probe.height);
}

/** Turns a point in the 3D region into page coordinates for the mouse. */
export function pagePointInCube(
  probe: CanvasProbe,
  [fx, fy]: readonly number[],
): { x: number; y: number } {
  const side = cubeRegionSide(probe);
  const bufferX = (probe.width - side) / 2 + fx * side;
  const bufferY = (probe.height - side) / 2 + fy * side;

  return {
    x: probe.box.left + (bufferX * probe.box.width) / probe.width,
    y: probe.box.top + (bufferY * probe.box.height) / probe.height,
  };
}

/**
 * Drag distance in CSS pixels for a number of quarter turns on the cube, or
 * of quarter sweeps of the viewpoint: a fraction of the 3D region, measured
 * in the canvas box rather than in the drawing buffer.
 */
export function cubeDragFor(probe: CanvasProbe, quarterTurns: number): number {
  const side = (cubeRegionSide(probe) * probe.box.width) / probe.width;
  return side * QUARTER_TURN_FRACTION * quarterTurns;
}

/**
 * The net's rectangle in its canvas's drawing buffer pixels.
 *
 * The net is alone on its canvas in every mode that shows it, so it grows
 * until one extent runs out first.
 */
export function netLayout(probe: CanvasProbe): {
  x: number;
  y: number;
  faceSide: number;
} {
  const { width, height } = probe.netCanvas;
  const faceSide = Math.min(
    (NET_ONLY_WIDTH * width) / NET_COLUMNS,
    (NET_ONLY_HEIGHT * height) / NET_ROWS,
  );
  return {
    x: (width - faceSide * NET_COLUMNS) / 2,
    y: (height - faceSide * NET_ROWS) / 2,
    faceSide,
  };
}

/** Turns a net cell into page coordinates for the mouse. */
export function pagePointInNet(
  probe: CanvasProbe,
  block: { readonly column: number; readonly row: number },
  col: number,
  row: number,
  size: number = CUBE_SIZE,
): { x: number; y: number } {
  const net = netLayout(probe);
  const frame = probe.netCanvas;
  const cell = net.faceSide / size;
  const bufferX = net.x + block.column * net.faceSide + (col + 0.5) * cell;
  const bufferY = net.y + block.row * net.faceSide + (row + 0.5) * cell;

  return {
    x: frame.box.left + (bufferX * frame.box.width) / frame.width,
    y: frame.box.top + (bufferY * frame.box.height) / frame.height,
  };
}

/**
 * Drag distance in CSS pixels for a quarter turn on the net.
 *
 * The net's sensitivity is one face across, so this is that face measured in
 * the canvas box rather than in the drawing buffer.
 */
export function netDragFor(probe: CanvasProbe, quarterTurns: number): number {
  const net = netLayout(probe);
  const frame = probe.netCanvas;
  return (net.faceSide * frame.box.width * quarterTurns) / frame.width;
}

/** The faces showing at the three screen positions, whatever the viewpoint. */
export function assertVisibleFaces(
  probe: CanvasProbe,
  top: number[],
  left: number[],
  right: number[],
): void {
  // Within two units per channel (contract v4): the samples read a gradient
  // that runs a byte per fifteen pixels or so, a few dozen pixels off the
  // middle of the sticker.
  expect(near(probe.top, top, 2), `top ${probe.top} vs ${top}`).toBe(true);
  expect(near(probe.left, left, 2), `left ${probe.left} vs ${left}`).toBe(true);
  expect(near(probe.right, right, 2), `right ${probe.right} vs ${right}`).toBe(
    true,
  );
}

export function assertFacesAndCorners(probe: CanvasProbe): void {
  // The home viewpoint, looking down the (1, 1, 1) diagonal, each face at
  // its plane's brightness. No glint reaches these three from here.
  assertVisibleFaces(probe, UP_LIT, FRONT_LIT, RIGHT_LIT);

  for (const corner of probe.corners) {
    expect(corner).toEqual(BACKGROUND);
  }

  // A seam is only a few pixels wide, so below this size anti-aliasing
  // reaches the sample point and the check is not meaningful.
  if (cubeRegionSide(probe) >= SEAM_MIN_REGION) {
    for (const seam of probe.seams) {
      expect(seam).toEqual(BODY);
    }
  }
}

export function assertSceneContract(probe: CanvasProbe): void {
  assertFacesAndCorners(probe);

  // All 54 cells at once, so a failure names every wrong sticker rather than
  // stopping at the first.
  expect(probe.net).toEqual(expectedNet());
}

/** Every canvas on show is drawn at the page's density, box for box. */
export function assertDrawingBufferMatchesCss(probe: CanvasProbe): void {
  for (const frame of [probe, probe.netCanvas]) {
    if (frame.cssWidth === 0) continue;
    expect(frame.width).toBe(
      Math.max(1, Math.round(frame.cssWidth * probe.devicePixelRatio)),
    );
    expect(frame.height).toBe(
      Math.max(1, Math.round(frame.cssHeight * probe.devicePixelRatio)),
    );
  }
}
