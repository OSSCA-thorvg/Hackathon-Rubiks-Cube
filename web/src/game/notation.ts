/**
 * How a packed move is read, and how it is written down.
 *
 * Pure functions over numbers: nothing here touches the DOM and nothing here
 * asks the engine anything. Notation is a way of writing a move rather than a
 * property of it, so this module is where a change of notation stops -- the
 * engine hands over the same word whichever letters end up on the screen.
 */

/** Axis values, in the same stable order as the C++ cube domain. */
export const MoveAxis = {
  X: 0,
  Y: 1,
  Z: 2,
} as const;

/** One axis value. */
export type MoveAxis = (typeof MoveAxis)[keyof typeof MoveAxis];

/**
 * The packed format, mirrored from `cube/PackedMove.hpp`.
 *
 * ```text
 * bits 0-1   axis        0 = X, 1 = Y, 2 = Z
 * bits 2-3   turns code  0 = -1, 1 = +1, 2 = +2
 * bits 4-31  layer mask  bit 4 is layer 0
 * ```
 *
 * Written out on both sides rather than shared, because there is no way to
 * share a constant across the boundary -- and fixed on both sides by the same
 * known answers, which is what would catch the two drifting apart.
 */
export const PACKED_AXIS_MASK = 0x3;
export const PACKED_TURNS_SHIFT = 2;
export const PACKED_TURNS_MASK = 0x3;
export const PACKED_LAYER_SHIFT = 4;

/** The quarter turns a code stands for, indexed by the code itself. */
const TURNS_BY_CODE: readonly (QuarterTurns | undefined)[] = [-1, 1, 2];

/** The turns a written move can have; every other count normalizes to one. */
export type QuarterTurns = -1 | 1 | 2;

/** One move as the engine recorded it, with its turns already normalized. */
export type DecodedMove = {
  readonly axis: MoveAxis;
  /** Bitmask over layer indices along the axis; never zero. */
  readonly layers: number;
  readonly quarterTurns: QuarterTurns;
};

/**
 * One cell of the notation table: which letter, and which way round it reads.
 *
 * `inverted` is where every sign convention in the notation lives. A face at
 * the negative end of its axis turns clockwise by going counter-clockwise
 * about that axis, and the two middle slices that follow such a face -- M with
 * L, E with D -- take their direction from it. S follows F and does not.
 */
type NotationCell = {
  readonly letter: string;
  readonly inverted: boolean;
};

/**
 * Every move a 3x3 can be turned into, by axis and layer.
 *
 * A table rather than an algorithm, because on a 3x3 the whole of the input is
 * nine possibilities: three axes with three single layers each. Deciding
 * which letter and which sign by rule would be a general answer to a question
 * that has nine cases, and the sign conventions would be spread across the
 * branches of it instead of sitting in one column.
 */
const NOTATION_3: Readonly<Record<MoveAxis, readonly NotationCell[]>> = {
  [MoveAxis.X]: [
    { letter: 'L', inverted: true },
    { letter: 'M', inverted: true },
    { letter: 'R', inverted: false },
  ],
  [MoveAxis.Y]: [
    { letter: 'D', inverted: true },
    { letter: 'E', inverted: true },
    { letter: 'U', inverted: false },
  ],
  [MoveAxis.Z]: [
    { letter: 'B', inverted: true },
    { letter: 'S', inverted: false },
    { letter: 'F', inverted: false },
  ],
};

/** The size of cube this notation table describes. */
const TABLE_SIZE = 3;

/**
 * Reads a packed move, or returns null for a word that is not one.
 *
 * Zero is the engine's answer for an index its record does not hold, so it
 * arrives here as a matter of course and leaves as null rather than as an
 * error: asking for a move that is not there is a question, not a fault.
 */
export function unpackMove(packed: number): DecodedMove | null {
  if (!Number.isSafeInteger(packed) || packed < 0 || packed > 0xffffffff) {
    return null;
  }

  const axis = (packed & PACKED_AXIS_MASK) as MoveAxis;
  const quarterTurns =
    TURNS_BY_CODE[(packed >>> PACKED_TURNS_SHIFT) & PACKED_TURNS_MASK];
  const layers = packed >>> PACKED_LAYER_SHIFT;

  // A move turns at least one layer and turns it by something, so a word
  // failing either of those is not a move -- which is what makes zero, the
  // engine's "no move here", fall out of the same check as a corrupt word.
  if (quarterTurns === undefined || layers === 0) return null;
  return { axis, layers, quarterTurns };
}

/** The index of the single layer a mask selects, or null for anything else. */
function singleLayer(layers: number): number | null {
  if ((layers & (layers - 1)) !== 0) return null;
  return 31 - Math.clz32(layers);
}

/**
 * Writes a packed move in standard notation, or returns null.
 *
 * Null covers every mask the table has no entry for -- several layers at once,
 * a layer past the edge of the cube, a word that is not a move -- with no case
 * of its own for any of them. On a 3x3 nothing can produce one: a drag turns a
 * single layer and so does every command, which is why nothing draws this null
 * and it exists only in the return type. A cube with more layers is what would
 * bring the first one about, and the numbered notation it needs comes with it.
 *
 * `size` is taken rather than assumed so that the caller who has a bigger cube
 * is the one who has to say so, instead of quietly getting `R` for a move that
 * is not R.
 */
export function moveNotation(packed: number, size: number): string | null {
  if (size !== TABLE_SIZE) return null;

  const move = unpackMove(packed);
  if (move === null) return null;

  const index = singleLayer(move.layers);
  if (index === null || index >= size) return null;

  // In range of a table with a row for every axis, so there is a cell here
  // and no failure left: the two ways of missing were the mask and the size.
  const cell = NOTATION_3[move.axis][index];
  const turns = cell.inverted ? -move.quarterTurns : move.quarterTurns;
  // A half turn is a half turn whichever way it was made, so it is the one
  // suffix the sign never reaches.
  const suffix = Math.abs(turns) === 2 ? '2' : turns > 0 ? '' : "'";
  return `${cell.letter}${suffix}`;
}
