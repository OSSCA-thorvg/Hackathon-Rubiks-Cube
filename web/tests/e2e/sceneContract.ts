import { expect, type Page } from '@playwright/test';

// Rendered scene contract v4 as RGBA tuples: the canvas holds a square 3D
// region showing three differently colored faces of a cube under one light,
// and below it a net showing all six faces flat and unlit, over a solid
// background. A winding, culling, channel-order, or domain regression shows up
// as the wrong color here rather than as a plausible picture -- and since v4
// a normal the wrong way round shows up as the wrong brightness too.
export const BACKGROUND = [32, 32, 32, 255];
export const WHITE = [255, 255, 255, 255]; // +Y up
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
 * gray + ((c - gray) * 115 + 50) / 100 rounded away from grey, clamped.
 */
export function saturated(color: readonly number[]): number[] {
  const gray = Math.floor(
    (2126 * color[0]! + 7152 * color[1]! + 722 * color[2]! + 5000) / 10000,
  );
  const channel = (value: number): number => {
    const offset = (value - gray) * 115;
    const rounded =
      offset >= 0
        ? Math.floor((offset + 50) / 100)
        : -Math.floor((-offset + 50) / 100);
    return Math.min(255, Math.max(0, gray + rounded));
  };
  return [channel(color[0]!), channel(color[1]!), channel(color[2]!), color[3]!];
}

/** The six faces as the lit 3D view shows them on a solved cube. */
export const UP_LIT = lit(saturated(WHITE), LIT_UP);
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
  for (let b = 190; b <= 300; b += 1) {
    if (near(pixel, lit(BODY, b), 2)) return true;
  }
  return false;
}

/**
 * How many cells of the coarse 3D sweep show the body.
 *
 * The seams show it at rest, a few cells' worth; a cut face opened by a turn
 * shows it over many more. The count, against the resting count, is what
 * says a layer is part way round.
 */
export function bodyCells(probe: CanvasProbe): number {
  return probe.cubeGrid.filter(isBody).length;
}

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
// comfortably wider than the anti-aliased edges around it.
export const SEAM_SAMPLES = [
  [0.377, 0.645],
  [0.313, 0.534],
  [0.564, 0.321],
] as const;
export const SEAM_MIN_SIZE = 1024;

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

// Layout fractions of the shorter canvas side, matching
// docs/tasks/04-rubiks-cube-domain.md. Repeated here on purpose so a layout
// change has to be made deliberately in both places.
export const CUBE_REGION_SIDE = 0.58;
export const CUBE_REGION_TOP = 0.01;
export const NET_FACE_SIDE = 0.12;
export const NET_TOP = 0.62;

/**
 * Fraction of the 3D region's width a drag has to cover for a quarter turn,
 * mirroring the engine's own drag sensitivity. On a square canvas this works
 * out to the same fraction of the canvas in CSS pixels, because the device
 * pixel ratio cancels between the drag and the region.
 */
export const QUARTER_TURN_FRACTION = 0.5;
export const QUARTER_TURN_DRAG = CUBE_REGION_SIDE * QUARTER_TURN_FRACTION;

/** Resolution of the coarse grid used to tell two renderings apart. */
const SIGNATURE_STEPS = 12;

export type CanvasProbe = {
  readonly width: number;
  readonly height: number;
  readonly cssWidth: number;
  readonly cssHeight: number;
  readonly devicePixelRatio: number;
  readonly box: { left: number; top: number; width: number; height: number };
  readonly top: number[];
  readonly left: number[];
  readonly right: number[];
  readonly seams: number[][];
  /** The two cut-face probes: a lit body mid-turn, a sticker at rest. */
  readonly cutR: number[];
  readonly cutU: number[];
  readonly net: number[][];
  readonly corners: number[][];
  /** A coarse sweep of the 3D region, for comparing two frames. */
  readonly cubeGrid: number[][];
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

      const box = canvas.getBoundingClientRect();

      return {
        width: canvas.width,
        height: canvas.height,
        cssWidth: canvas.clientWidth,
        cssHeight: canvas.clientHeight,
        devicePixelRatio: window.devicePixelRatio,
        box: {
          left: box.left,
          top: box.top,
          width: box.width,
          height: box.height,
        },
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
      cubeRegionTop: CUBE_REGION_TOP,
      netFaceSide: NET_FACE_SIDE,
      netTop: NET_TOP,
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

/** Turns a point in the 3D region into page coordinates for the mouse. */
export function pagePointInCube(
  probe: CanvasProbe,
  [fx, fy]: readonly number[],
): { x: number; y: number } {
  const unit = Math.min(probe.width, probe.height);
  const side = CUBE_REGION_SIDE * unit;
  const bufferX = (probe.width - side) / 2 + fx * side;
  const bufferY = CUBE_REGION_TOP * unit + fy * side;

  return {
    x: probe.box.left + (bufferX * probe.box.width) / probe.width,
    y: probe.box.top + (bufferY * probe.box.height) / probe.height,
  };
}

/** Which layout the net is drawn with; the two view modes place it apart. */
export type NetView = 'both' | 'net';

/** The net's rectangle in drawing buffer pixels. */
export function netLayout(
  probe: CanvasProbe,
  view: NetView,
): { x: number; y: number; faceSide: number } {
  if (view === 'net') {
    // Alone on the canvas the net grows until one extent runs out first.
    const faceSide = Math.min(
      (NET_ONLY_WIDTH * probe.width) / NET_COLUMNS,
      (NET_ONLY_HEIGHT * probe.height) / NET_ROWS,
    );
    return {
      x: (probe.width - faceSide * NET_COLUMNS) / 2,
      y: (probe.height - faceSide * NET_ROWS) / 2,
      faceSide,
    };
  }

  const faceSide = NET_FACE_SIDE * Math.min(probe.width, probe.height);
  return {
    x: (probe.width - faceSide * NET_COLUMNS) / 2,
    y: NET_TOP * Math.min(probe.width, probe.height),
    faceSide,
  };
}

/** Turns a net cell into page coordinates for the mouse. */
export function pagePointInNet(
  probe: CanvasProbe,
  view: NetView,
  block: { readonly column: number; readonly row: number },
  col: number,
  row: number,
  size: number = CUBE_SIZE,
): { x: number; y: number } {
  const net = netLayout(probe, view);
  const cell = net.faceSide / size;
  const bufferX = net.x + block.column * net.faceSide + (col + 0.5) * cell;
  const bufferY = net.y + block.row * net.faceSide + (row + 0.5) * cell;

  return {
    x: probe.box.left + (bufferX * probe.box.width) / probe.width,
    y: probe.box.top + (bufferY * probe.box.height) / probe.height,
  };
}

/**
 * Drag distance in CSS pixels for a quarter turn on the net.
 *
 * The net's sensitivity is one face across, so this is that face measured in
 * the canvas box rather than in the drawing buffer.
 */
export function netDragFor(
  probe: CanvasProbe,
  view: NetView,
  quarterTurns: number,
): number {
  const net = netLayout(probe, view);
  return (net.faceSide * probe.box.width * quarterTurns) / probe.width;
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
  if (Math.min(probe.width, probe.height) >= SEAM_MIN_SIZE) {
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

export function assertDrawingBufferMatchesCss(probe: CanvasProbe): void {
  expect(probe.width).toBe(
    Math.max(1, Math.round(probe.cssWidth * probe.devicePixelRatio)),
  );
  expect(probe.height).toBe(
    Math.max(1, Math.round(probe.cssHeight * probe.devicePixelRatio)),
  );
}
