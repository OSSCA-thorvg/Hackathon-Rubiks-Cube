import { describe, expect, it } from 'vitest';

import {
  faceOfNotation,
  MAX_TYPED_MOVES,
  MoveAxis,
  moveNotation,
  parseMoves,
  unpackMove,
  type QuarterTurns,
  type TypedTurn,
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

/** The turns a line was read as, or the reason it was refused. */
function read(text: string, size = SIZE): readonly TypedTurn[] | string {
  const result = parseMoves(text, size);
  return result.ok ? result.turns : result.reason;
}

/** One typed turn, written the short way round. */
function turn(
  face: TypedTurn['face'],
  firstDepth: number,
  lastDepth: number,
  turns: QuarterTurns,
): TypedTurn {
  return { face, firstDepth, lastDepth, turns };
}

/**
 * The word the engine records for a typed turn.
 *
 * The inverse of what the engine's face command does, written out here so the
 * round trip below compares words rather than this module with itself: a
 * face at the positive end of its axis counts its depths down from the top
 * index and turns the way the axis does, and the face opposite counts up from
 * zero and turns against it.
 */
function packTyped(typed: TypedTurn, size: number): number {
  const axis = { R: 0, L: 0, U: 1, D: 1, F: 2, B: 2 }[typed.face];
  const positive = 'RUF'.includes(typed.face);
  let layers = 0;
  for (let depth = typed.firstDepth; depth <= typed.lastDepth; depth += 1) {
    layers |= 1 << (positive ? size - depth : depth - 1);
  }
  const quarter: QuarterTurns =
    typed.turns === 2 ? 2 : positive ? typed.turns : typed.turns === 1 ? -1 : 1;
  return packRun(axis, layers, quarter) >>> 0;
}

describe('parseMoves', () => {
  it('reads the six faces and their three turns', () => {
    expect(read("R U' F2 L D2 B'")).toEqual([
      turn('R', 1, 1, 1),
      turn('U', 1, 1, -1),
      turn('F', 1, 1, 2),
      turn('L', 1, 1, 1),
      turn('D', 1, 1, 2),
      turn('B', 1, 1, -1),
    ]);

    // A half turn is a half turn whichever way it is marked.
    expect(read("R2' R'2")).toEqual([turn('R', 1, 1, 2), turn('R', 1, 1, 2)]);
  });

  it('reads a line the way it arrives from a scramble sheet', () => {
    // No spaces, curly and typographic primes, commas: the same four moves.
    const plain = read("R U R' U'");
    expect(read("RUR'U'")).toEqual(plain);
    expect(read('R U R\u2019 U\u2032')).toEqual(plain);
    expect(read("R, U, R', U'")).toEqual(plain);
    expect(read("  R\tU  R'\nU'  ")).toEqual(plain);

    // The suffix is taken greedily, so R2R is a half turn and a quarter.
    expect(read('R2R')).toEqual([turn('R', 1, 1, 2), turn('R', 1, 1, 1)]);
  });

  it('reads wide and numbered moves as depths from the face', () => {
    expect(read('Rw 3Rw 2R 2-3Rw 2-3R', 5)).toEqual([
      turn('R', 1, 2, 1),
      turn('R', 1, 3, 1),
      turn('R', 2, 2, 1),
      turn('R', 2, 3, 1),
      turn('R', 2, 3, 1),
    ]);

    // Letters are read in either case, and lower case is not a wide move:
    // the log writes Rw, and r on a keyboard is R.
    expect(read("r u' 3f2 rw 2-3rW", 5)).toEqual([
      turn('R', 1, 1, 1),
      turn('U', 1, 1, -1),
      turn('F', 3, 3, 2),
      turn('R', 1, 2, 1),
      turn('R', 2, 3, 1),
    ]);
  });

  it('gives each move back in the spelling the log uses', () => {
    const typed = parseMoves("r u\u2019 2-3rw m2", 5);
    expect(typed.ok && typed.written).toEqual(['R', "U'", '2-3Rw', 'M2']);
  });

  it('reads a slice as the face it follows, one layer in', () => {
    expect(read("M E' S2")).toEqual([
      turn('L', 2, 2, 1),
      turn('D', 2, 2, -1),
      turn('F', 2, 2, 2),
    ]);

    // The middle of a bigger odd cube is further in, and an even cube has
    // no middle at all.
    expect(read('M', 5)).toEqual([turn('L', 3, 3, 1)]);
    expect(read('M', 4)).toBe(
      '"M" needs a middle layer, which a 4×4 does not have.',
    );
  });

  it('refuses the whole line for one move the cube cannot make', () => {
    expect(read('R Q U')).toBe(`Can't read "Q" as a move.`);
    expect(read('R x')).toBe(
      '"x" turns the whole cube. Only layers can be turned here.',
    );
    expect(read('4R')).toBe('"4R" reaches past a 3×3.');
    expect(read('3Rw')).toBe(
      '"3Rw" turns the whole cube. Only layers can be turned here.',
    );
    expect(read('Rw', 2)).toBe(
      '"Rw" turns the whole cube. Only layers can be turned here.',
    );

    // Things that look like moves and are not.
    expect(read('Rww')).toBe(`Can't read "w" as a move.`);
    expect(read('2M')).toBe(`Can't read "2M" as a move.`);
    expect(read('0R')).toBe(`Can't read "0R" as a move.`);
    expect(read('3-2Rw', 5)).toBe(`Can't read "3-2Rw" as a move.`);
    expect(read('R3')).toBe(`Can't read "3" as a move.`);
  });

  it('has no moves in an empty line, and a limit to a long one', () => {
    expect(read('')).toEqual([]);
    expect(read('   ')).toEqual([]);

    expect(read('R '.repeat(MAX_TYPED_MOVES))).toHaveLength(MAX_TYPED_MOVES);
    expect(read('R '.repeat(MAX_TYPED_MOVES + 1))).toBe(
      `That is more than ${MAX_TYPED_MOVES} moves at once.`,
    );
  });

  it('reads back every move the log writes, on every size it writes them', () => {
    let checked = 0;
    for (let size = 2; size <= 7; size += 1) {
      for (const axis of [MoveAxis.X, MoveAxis.Y, MoveAxis.Z]) {
        for (let from = 0; from < size; from += 1) {
          for (let to = from; to < size; to += 1) {
            if (to - from + 1 === size) continue;
            let layers = 0;
            for (let index = from; index <= to; index += 1) layers |= 1 << index;

            for (const turns of [-1, 1, 2] as const) {
              const packed = packRun(axis, layers, turns);
              const written = moveNotation(packed, size);
              expect(written, `${size} ${axis} ${layers} ${turns}`).not.toBeNull();

              const typed = parseMoves(written!, size);
              expect(typed.ok, written!).toBe(true);
              if (!typed.ok) continue;
              expect(typed.turns, written!).toHaveLength(1);
              expect(packTyped(typed.turns[0]!, size), written!).toBe(packed);
              checked += 1;
            }
          }
        }
      }
    }

    // Every one of them, so the loops above cannot have skipped their way to
    // green: n(n+1)/2 - 1 runs on each of three axes, three ways each, for
    // n from 2 to 7.
    expect(checked).toBe(693);
  });
});

describe('faceOfNotation', () => {
  it('names the face a move is written after, and none for a slice', () => {
    expect(faceOfNotation('R')).toBe('R');
    expect(faceOfNotation("U'")).toBe('U');
    expect(faceOfNotation("2-3Lw'")).toBe('L');
    expect(faceOfNotation('3Bw2')).toBe('B');
    expect(faceOfNotation('M')).toBeNull();
    expect(faceOfNotation("E'")).toBeNull();
    expect(faceOfNotation('S2')).toBeNull();
  });
});
