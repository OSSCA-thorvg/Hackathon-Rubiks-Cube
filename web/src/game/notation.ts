/**
 * How a packed move is read, how it is written down, and how a written one is
 * read back.
 *
 * Pure functions over numbers and strings: nothing here touches the DOM and
 * nothing here asks the engine anything. Notation is a way of writing a move
 * rather than a property of it, so this module is where a change of notation
 * stops -- the engine hands over the same word whichever letters end up on the
 * screen, and takes the same face command whichever letters were typed.
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

/**
 * The axis a code stands for, indexed by the code itself.
 *
 * A table for the same reason the turns have one: two bits carry three values,
 * so one code names nothing, and the gap in the table is what says so. It
 * matters for a word that did not come from the engine -- a shared link is
 * free to contain that code, and a cast would have turned it into an axis.
 */
const AXIS_BY_CODE: readonly (MoveAxis | undefined)[] = [
  MoveAxis.X,
  MoveAxis.Y,
  MoveAxis.Z,
];

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
 * The letters a numbered move is written with, by axis.
 *
 * The face at each end of the axis, and whether counting from it reads the
 * turns backwards -- the same `inverted` convention as the table above, and
 * for the same reason: a face at the negative end turns clockwise by going
 * counter-clockwise about its axis.
 */
const FACES_BY_AXIS: Readonly<Record<MoveAxis, readonly [NotationCell, NotationCell]>> = {
  // The negative end first, so the pair is in index order like the layers.
  [MoveAxis.X]: [
    { letter: 'L', inverted: true },
    { letter: 'R', inverted: false },
  ],
  [MoveAxis.Y]: [
    { letter: 'D', inverted: true },
    { letter: 'U', inverted: false },
  ],
  [MoveAxis.Z]: [
    { letter: 'B', inverted: true },
    { letter: 'F', inverted: false },
  ],
};

/** The layers a mask holds as a run, or null when it is not one. */
export type LayerRun = {
  /** Lowest layer index in the run. */
  readonly from: number;
  /** Highest, which equals `from` for a single layer. */
  readonly to: number;
};

/**
 * Reads a mask as one unbroken run of a cube's layers, or returns null.
 *
 * Everything the notation cannot write comes back as null here rather than in
 * a case of its own downstream: a set with a gap in it, layers past the edge
 * of the cube, and the whole cube at once -- which is a rotation, and rotations
 * are not written in these letters.
 *
 * Exported because the same question is asked twice for different reasons:
 * here it decides how a move is written, and a shared payload asks it to
 * refuse the masks this cube has no way of making. One rule with one
 * implementation, so a link can never carry a move the log cannot write.
 */
export function layerRun(layers: number, size: number): LayerRun | null {
  if (layers === 0 || size < 2 || size > 28) return null;
  if (layers >>> size !== 0) return null;

  let from = 0;
  while ((layers >>> from & 1) === 0) from += 1;

  let to = from;
  while ((layers >>> (to + 1) & 1) === 1) to += 1;

  // Anything left above the run is a second run with a gap before it.
  if (layers >>> (to + 1) !== 0) return null;
  if (to - from + 1 === size) return null;
  return { from, to };
}

/**
 * Writes a run of layers the way big cubes are written: numbered from a face.
 *
 * Depths are counted from whichever of the axis's two faces the run is nearer,
 * so the numbers stay small and a move keeps the letter of the face it looks
 * like it belongs to. A run sitting exactly in the middle is counted from the
 * positive face, which is a convention and nothing more -- both readings name
 * the same layers, and one of them has to be chosen.
 */
function numberedNotation(move: DecodedMove, run: LayerRun, size: number): string {
  const fromPositive = size - run.to;
  const fromNegative = run.from + 1;
  const positive = fromPositive <= fromNegative;

  const cell = FACES_BY_AXIS[move.axis][positive ? 1 : 0];
  const first = positive ? fromPositive : fromNegative;
  const last = positive ? size - run.from : run.to + 1;

  // Five shapes, and the first three are what makes `R` and `Rw` read as
  // themselves rather than as `1R` and `1-2Rw`.
  const body =
    first === 1 && last === 1
      ? cell.letter
      : first === 1 && last === 2
        ? `${cell.letter}w`
        : first === 1
          ? `${last}${cell.letter}w`
          : first === last
            ? `${first}${cell.letter}`
            : `${first}-${last}${cell.letter}w`;

  return `${body}${turnSuffix(move.quarterTurns, cell.inverted)}`;
}

/** How a move's turns are written once the letter has decided the direction. */
function turnSuffix(quarterTurns: QuarterTurns, inverted: boolean): string {
  const turns = inverted ? -quarterTurns : quarterTurns;
  // A half turn is a half turn whichever way it was made, so it is the one
  // suffix the sign never reaches.
  return Math.abs(turns) === 2 ? '2' : turns > 0 ? '' : "'";
}

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

  const axis = AXIS_BY_CODE[packed & PACKED_AXIS_MASK];
  const quarterTurns =
    TURNS_BY_CODE[(packed >>> PACKED_TURNS_SHIFT) & PACKED_TURNS_MASK];
  const layers = packed >>> PACKED_LAYER_SHIFT;

  // A move turns at least one layer, about an axis, by some number of
  // quarters. A word failing any of the three is not a move -- which is what
  // makes zero, the engine's "no move here", fall out of the same check as a
  // word somebody wrote by hand.
  if (axis === undefined || quarterTurns === undefined || layers === 0) {
    return null;
  }
  return { axis, layers, quarterTurns };
}

export function moveNotation(packed: number, size: number): string | null {
  const move = unpackMove(packed);
  if (move === null) return null;

  const run = layerRun(move.layers, size);
  if (run === null) return null;

  if (size === TABLE_SIZE && run.from === run.to) {
    const cell = NOTATION_3[move.axis][run.from];
    return `${cell.letter}${turnSuffix(move.quarterTurns, cell.inverted)}`;
  }

  return numberedNotation(move, run, size);
}

/** The six faces by the letter a face turn is written with. */
export type FaceLetter = 'R' | 'L' | 'U' | 'D' | 'F' | 'B';

/**
 * One typed turn, in the terms the engine's face command takes.
 *
 * Depths count inwards from the face, which is how `turnFace` counts them and
 * how the numbered notation above reads: `R` is 1 to 1, `Rw` is 1 to 2 and
 * `2R` is 2 to 2. The turns are the face's own, so `L` is `+1` here even
 * though the engine records it as a negative quarter about its axis.
 */
export type TypedTurn = {
  readonly face: FaceLetter;
  readonly firstDepth: number;
  readonly lastDepth: number;
  readonly turns: QuarterTurns;
};

/** What a line of typed moves turned out to be. */
export type TypedMoves =
  | {
      readonly ok: true;
      readonly turns: readonly TypedTurn[];
      /**
       * Each move as it was read, tidied into the log's letters and suffixes.
       * Not always the log's own name for it: see readToken.
       */
      readonly written: readonly string[];
    }
  | { readonly ok: false; readonly reason: string };

/**
 * The most turns one line may ask for.
 *
 * Every one of them is played, so a line is a sequence somebody sits through.
 * Two hundred is ten scrambles of the longest kind, which is more than anyone
 * types and less than anyone would wait for.
 */
export const MAX_TYPED_MOVES = 200;

/**
 * The slices a 3x3 names, and the face each one follows.
 *
 * M turns the way L does, E the way D does and S the way F does -- the same
 * three the notation table above marks as reading their axis backwards or
 * not -- so a slice is that face's command, one layer further in.
 */
const SLICE_FACE: Readonly<Record<string, FaceLetter>> = {
  M: 'L',
  E: 'D',
  S: 'F',
};

/**
 * One token: an optional depth or range, a letter, a wide mark, and a suffix.
 *
 * Sticky rather than anchored, so a line typed without spaces -- `RUR'U'` --
 * is read the same as one typed with them. The suffix is greedy, which is
 * what settles `R2R` as a half turn and a quarter rather than as two moves
 * with a depth between them.
 *
 * Letters are read in either case. Lower case means a wide move in one
 * notation, but not in the one this log writes -- it writes `Rw` -- and a
 * person typing `r u r' u'` into a search box means the four moves on the
 * keys, which is also what those keys turn.
 */
const TOKEN = /(?:(\d+)(?:-(\d+))?)?([RLUDFBMES])([wW]?)(2'|'2|2|'|)/iy;

/** The primes a keyboard or a copied scramble may carry, all read as one. */
const PRIMES = /[\u2019\u2032`]/g;

/** A size said the way the rest of the page says it. */
function sizeName(size: number): string {
  return `${size}×${size}`;
}

/** A token that was read: the turn, and the token as the log would spell it. */
type ReadToken = {
  readonly turn: TypedTurn;
  readonly written: string;
};

/**
 * Reads one token that TOKEN matched, or says what is wrong with it.
 *
 * Everything a range can get wrong is checked here against the cube in hand,
 * with the same two rules the engine applies to a face command: a run stays
 * on the cube, and it is never the whole of it.
 */
function readToken(match: RegExpExecArray, size: number): ReadToken | string {
  // The two numbers are optional groups, so either may be missing even though
  // the array's type says every entry is a string.
  const token = match[0];
  const first: string | undefined = match[1];
  const last: string | undefined = match[2];
  const letter = (match[3] ?? '').toUpperCase();
  const wide = match[4] ?? '';
  const suffix = match[5] ?? '';

  const turns: QuarterTurns =
    suffix === '' ? 1 : suffix === "'" ? -1 : 2;

  // The token again, tidied the way the log writes: upper case, a lower-case
  // w, and one way of writing each turn. It keeps the name the person chose,
  // though, which is not always the one the log will give the move once it
  // is played -- 2R on a 3x3 is shown as 2R and logged as M' -- because what
  // is shown back before playing is what they typed, made legible.
  const range = first === undefined ? '' : last === undefined ? first : `${first}-${last}`;
  const written = `${range}${letter}${wide === '' ? '' : 'w'}${
    turns === 2 ? '2' : turns === -1 ? "'" : ''
  }`;

  const slice = SLICE_FACE[letter];
  if (slice !== undefined) {
    if (first !== undefined || wide !== '') {
      return `Can't read "${token}" as a move.`;
    }
    // Only a cube with a middle has one, and it is the same layer from
    // either face: halfway in, counted from the face the slice follows.
    if (size % 2 === 0 || size < 3) {
      return `"${token}" needs a middle layer, which a ${sizeName(size)} does not have.`;
    }
    const middle = (size + 1) / 2;
    return {
      turn: { face: slice, firstDepth: middle, lastDepth: middle, turns },
      written,
    };
  }

  const face = letter as FaceLetter;
  const isWide = wide !== '';

  const from = first === undefined ? null : Number(first);
  const to = last === undefined ? null : Number(last);

  let firstDepth: number;
  let lastDepth: number;
  if (to !== null) {
    // A numbered run, `2-3Rw`. The w is how the log writes it; without one
    // the range still says which layers, so it is read the same.
    firstDepth = from ?? 1;
    lastDepth = to;
  } else if (isWide) {
    // `Rw` is the face and the layer behind it; `3Rw` goes three deep.
    firstDepth = 1;
    lastDepth = from ?? 2;
  } else {
    // `R` is the face and `2R` is the one layer two deep.
    firstDepth = from ?? 1;
    lastDepth = firstDepth;
  }

  if (firstDepth < 1 || lastDepth < firstDepth) {
    return `Can't read "${token}" as a move.`;
  }
  if (lastDepth > size) {
    return `"${token}" reaches past a ${sizeName(size)}.`;
  }
  if (lastDepth - firstDepth + 1 >= size) {
    return `"${token}" turns the whole cube. Only layers can be turned here.`;
  }
  return { turn: { face, firstDepth, lastDepth, turns }, written };
}

/**
 * Reads a line of typed moves for a cube of `size` layers.
 *
 * Takes what the move log writes -- `R`, `U'`, `2-3Rw2`, `M'` -- and what a
 * person types from a scramble sheet: spaces or none, commas, curly primes,
 * either case. Every token is checked against the cube in hand, and
 * the whole line is refused for one bad token rather than played up to it: a
 * sequence stopped short is a cube nobody asked for.
 *
 * An empty line is a line with no moves, not an error.
 */
export function parseMoves(text: string, size: number): TypedMoves {
  const line = text.replace(PRIMES, "'");
  const turns: TypedTurn[] = [];
  const written: string[] = [];

  for (const chunk of line.split(/[\s,]+/)) {
    let at = 0;
    while (at < chunk.length) {
      TOKEN.lastIndex = at;
      const match = TOKEN.exec(chunk);
      if (match === null) {
        const rest = chunk.slice(at);
        // Rotations are the one thing somebody may reasonably type that has
        // no face command at all, so they get a sentence of their own.
        return /^[xyz]/i.test(rest)
          ? {
              ok: false,
              reason: `"${rest}" turns the whole cube. Only layers can be turned here.`,
            }
          : { ok: false, reason: `Can't read "${rest}" as a move.` };
      }

      const read = readToken(match, size);
      if (typeof read === 'string') return { ok: false, reason: read };
      turns.push(read.turn);
      written.push(read.written);
      if (turns.length > MAX_TYPED_MOVES) {
        return {
          ok: false,
          reason: `That is more than ${MAX_TYPED_MOVES} moves at once.`,
        };
      }
      at = TOKEN.lastIndex;
    }
  }

  return { ok: true, turns, written };
}

/**
 * The face whose colour a written move is shown in, or null for a slice.
 *
 * Read off the letter rather than the packed word, because the letter is
 * what a person sees next to the colour: `2R` is shown red because it says R.
 * M, E and S follow a face in direction only and sit between two colours, so
 * they are shown in neither.
 */
export function faceOfNotation(notation: string): FaceLetter | null {
  const letter = /[RLUDFBMES]/.exec(notation)?.[0];
  if (letter === undefined || SLICE_FACE[letter] !== undefined) return null;
  return letter as FaceLetter;
}
