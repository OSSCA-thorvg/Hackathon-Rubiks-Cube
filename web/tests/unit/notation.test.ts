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

  it('has no notation for a move the table does not describe', () => {
    // Several layers at once and a layer past the edge of the cube: a wide
    // move and a fourth layer, neither of which a 3x3 can be turned into.
    expect(moveNotation(0x74, SIZE)).toBeNull();
    expect(moveNotation(pack(MoveAxis.X, 3, 1), SIZE)).toBeNull();

    // Nothing at that index.
    expect(moveNotation(0, SIZE)).toBeNull();

    // A bigger cube says so rather than being handed a 3x3's letters: R on a
    // 4x4 is a different layer, and guessing would be worse than refusing.
    expect(moveNotation(0x44, 4)).toBeNull();
  });
});
