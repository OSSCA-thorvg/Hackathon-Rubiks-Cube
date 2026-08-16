import { describe, expect, it } from 'vitest';

import {
  MoveAxis,
  moveNotation,
  unpackMove,
  type QuarterTurns,
} from '../../src/game/notation.ts';

/** The only cube this notation is for. */
const SIZE = 3;

/** Builds a packed word the way the engine's pack() does. */
function pack(axis: number, layerIndex: number, turns: QuarterTurns): number {
  const code = turns === -1 ? 0 : turns === 1 ? 1 : 2;
  return axis | (code << 2) | ((1 << layerIndex) << 4);
}

describe('unpackMove', () => {
  // The words below are written out rather than built, because they are half
  // of an agreement with C++ and the other half is written out over there. A
  // test that packed them with this module's own shifts would agree with
  // itself however far the two sides had drifted.
  it('reads the same words the engine writes', () => {
    expect(unpackMove(0x44)).toEqual({
      axis: MoveAxis.X,
      layers: 0b100,
      quarterTurns: 1,
    });
    expect(unpackMove(0x40)).toEqual({
      axis: MoveAxis.X,
      layers: 0b100,
      quarterTurns: -1,
    });
    expect(unpackMove(0x48)).toEqual({
      axis: MoveAxis.X,
      layers: 0b100,
      quarterTurns: 2,
    });
    expect(unpackMove(0x45)).toEqual({
      axis: MoveAxis.Y,
      layers: 0b100,
      quarterTurns: 1,
    });
    expect(unpackMove(0x46)).toEqual({
      axis: MoveAxis.Z,
      layers: 0b100,
      quarterTurns: 1,
    });
    expect(unpackMove(0x10)).toEqual({
      axis: MoveAxis.X,
      layers: 0b1,
      quarterTurns: -1,
    });
    expect(unpackMove(0x26)).toEqual({
      axis: MoveAxis.Z,
      layers: 0b10,
      quarterTurns: 1,
    });

    // Several layers at once is a word this reader understands perfectly
    // well; whether it can be written down is a later question.
    expect(unpackMove(0x74)).toEqual({
      axis: MoveAxis.X,
      layers: 0b111,
      quarterTurns: 1,
    });
  });

  it('reads a mask right up to the top bit', () => {
    // The engine packs 28 layers, so the mask reaches bit 31 and the shift
    // that reads it has to be the unsigned one.
    expect(unpackMove(0x8000_0004)?.layers).toBe(1 << 27);
  });

  it('has no move for a word that is not one', () => {
    // Nothing at that index, which is the engine's ordinary answer for an
    // index its record does not hold.
    expect(unpackMove(0)).toBeNull();

    // Turns with nothing to turn, and the fourth turns code -- which the
    // engine never writes, because every count normalizes to one of three.
    expect(unpackMove(0x4)).toBeNull();
    expect(unpackMove(0x4c)).toBeNull();

    // And the fourth axis code, which is the same kind of gap: two bits carry
    // three axes. The engine never writes it either, but a shared link comes
    // from outside, and a cast here would have turned it into an axis the
    // notation table has no row for.
    expect(unpackMove(0x47)).toBeNull();
    expect(moveNotation(0x47, SIZE)).toBeNull();

    expect(unpackMove(-1)).toBeNull();
    expect(unpackMove(1.5)).toBeNull();
    expect(unpackMove(Number.NaN)).toBeNull();
    expect(unpackMove(0x1_0000_0000)).toBeNull();
  });
});

describe('moveNotation', () => {
  /** Every letter a 3x3 has, by axis and layer, as the table lays them out. */
  const LETTERS: readonly (readonly string[])[] = [
    ['L', 'M', 'R'],
    ['D', 'E', 'U'],
    ['B', 'S', 'F'],
  ];

  /**
   * Which way each of them reads.
   *
   * A face at the negative end of its axis, and the slice that follows it,
   * are written the other way round from the turn the engine recorded.
   */
  const INVERTED: readonly (readonly boolean[])[] = [
    [true, true, false],
    [true, true, false],
    [true, false, false],
  ];

  it('writes all twenty-seven moves a 3x3 has', () => {
    const written: string[] = [];

    for (const axis of [MoveAxis.X, MoveAxis.Y, MoveAxis.Z]) {
      for (let index = 0; index < SIZE; index += 1) {
        for (const turns of [-1, 1, 2] as const) {
          const letter = LETTERS[axis][index];
          const read = INVERTED[axis][index] ? -turns : turns;
          const suffix = Math.abs(read) === 2 ? '2' : read > 0 ? '' : "'";

          expect(moveNotation(pack(axis, index, turns), SIZE)).toBe(
            `${letter}${suffix}`,
          );
          written.push(`${letter}${suffix}`);
        }
      }
    }

    // Nine letters, three ways each, and no two of them the same word: a
    // table that repeated a cell would still have passed every line above.
    expect(new Set(written).size).toBe(27);
  });

  it('writes the six faces the way a solver reads them', () => {
    // Spelled out at the words themselves, so the expectations above cannot
    // agree with a table that has drifted from standard notation.
    expect(moveNotation(0x44, SIZE)).toBe('R');
    expect(moveNotation(0x40, SIZE)).toBe("R'");
    expect(moveNotation(0x48, SIZE)).toBe('R2');
    expect(moveNotation(0x10, SIZE)).toBe('L');
    expect(moveNotation(0x14, SIZE)).toBe("L'");
    expect(moveNotation(0x45, SIZE)).toBe('U');
    expect(moveNotation(0x11, SIZE)).toBe('D');
    expect(moveNotation(0x46, SIZE)).toBe('F');
    expect(moveNotation(0x12, SIZE)).toBe('B');

    // The three middle slices, which is what a drag across the middle of a
    // face makes and the only thing that names them.
    expect(moveNotation(0x20, SIZE)).toBe('M');
    expect(moveNotation(0x24, SIZE)).toBe("M'");
    expect(moveNotation(0x21, SIZE)).toBe('E');
    expect(moveNotation(0x26, SIZE)).toBe('S');
    expect(moveNotation(0x22, SIZE)).toBe("S'");
  });

  it('has no notation for a mask no move can have', () => {
    // The whole cube at once, which is a rotation and not written in these
    // letters, and a layer past the edge of the cube.
    expect(moveNotation(0x74, SIZE)).toBeNull();
    expect(moveNotation(pack(MoveAxis.X, 3, 1), SIZE)).toBeNull();

    // Two layers with a still one between them.
    expect(moveNotation(0x54, SIZE)).toBeNull();

    // Nothing at that index.
    expect(moveNotation(0, SIZE)).toBeNull();

    // And a size no cube is built at.
    expect(moveNotation(0x44, 1)).toBeNull();
  });
});

/** Builds a packed word from a mask rather than a single layer. */
function packRun(axis: number, layers: number, turns: QuarterTurns): number {
  const code = turns === -1 ? 0 : turns === 1 ? 1 : 2;
  return axis | (code << 2) | (layers << 4);
}

describe('moveNotation on a cube with layers inside', () => {
  it('numbers the depths from the face the run is nearer', () => {
    const x = (layers: number, turns: QuarterTurns = 1): string | null =>
      moveNotation(packRun(MoveAxis.X, layers, turns), 5);

    // Counted from R, which is the layer at the far end of the axis.
    expect(x(0b10000)).toBe('R');
    expect(x(0b11000)).toBe('Rw');
    expect(x(0b11100)).toBe('3Rw');
    expect(x(0b01000)).toBe('2R');
    expect(x(0b01100)).toBe('2-3Rw');

    // And from L, for the runs that sit nearer it. The sign turns over with
    // the letter: a positive quarter about the axis is L read backwards,
    // because clockwise from outside L is the other way round.
    expect(x(0b00001)).toBe("L'");
    expect(x(0b00011)).toBe("Lw'");
    expect(x(0b00010)).toBe("2L'");
    expect(x(0b00110)).toBe("2-3Lw'");
    expect(x(0b00011, -1)).toBe('Lw');

    // The middle layer is three deep from either face; the tie goes to R.
    expect(x(0b00100)).toBe('3R');
  });

  it('writes the turns the way the face it is named after reads them', () => {
    const y = (layers: number, turns: QuarterTurns): string | null =>
      moveNotation(packRun(MoveAxis.Y, layers, turns), 4);

    expect(y(0b1000, 1)).toBe('U');
    expect(y(0b1000, -1)).toBe("U'");
    expect(y(0b1000, 2)).toBe('U2');

    // The negative face reads the same quarter turns backwards.
    expect(y(0b0001, 1)).toBe("D'");
    expect(y(0b0001, -1)).toBe('D');
    expect(y(0b0011, 2)).toBe('Dw2');
  });

  it('keeps M, E and S for the cube that has them, and only that one', () => {
    // A 3x3's middle layers are named rather than numbered, at that size.
    expect(moveNotation(packRun(MoveAxis.X, 0b010, 1), 3)).toBe("M'");
    expect(moveNotation(packRun(MoveAxis.Y, 0b010, 1), 3)).toBe("E'");
    expect(moveNotation(packRun(MoveAxis.Z, 0b010, 1), 3)).toBe('S');

    // A wide move of the same cube is numbered, because there is no letter
    // for it: `Rw` is standard 3x3 notation and `M` is not what it means.
    expect(moveNotation(packRun(MoveAxis.X, 0b110, 1), 3)).toBe('Rw');

    // On a 5x5 nobody writes M for the middle slice.
    expect(moveNotation(packRun(MoveAxis.X, 0b00100, 1), 5)).toBe('3R');
  });

  it('writes the two layers of a 2x2 as its two faces', () => {
    expect(moveNotation(packRun(MoveAxis.X, 0b10, 1), 2)).toBe('R');
    expect(moveNotation(packRun(MoveAxis.X, 0b01, 1), 2)).toBe("L'");

    // Both layers at once is the whole cube, which is a rotation.
    expect(moveNotation(packRun(MoveAxis.X, 0b11, 1), 2)).toBeNull();
  });
});
