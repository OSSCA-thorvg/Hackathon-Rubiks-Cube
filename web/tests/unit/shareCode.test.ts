import { describe, expect, it } from 'vitest';

import {
  decodePainting,
  decodeSession,
  encodePainting,
  encodeSession,
  MAX_ENCODED_LENGTH,
  MAX_PAINTED_ENCODED_LENGTH,
  SHARE_PAINTED_VERSION,
  SHARE_VERSION,
  type SharedPainting,
  type SharedSession,
} from '../../src/game/shareCode.ts';
import {
  MAX_CUBE_SIZE,
  MAX_SHARED_MOVES,
  MIN_CUBE_SIZE,
} from '../../src/wasm/CubeEngine.ts';

/** R, U and F packed, which are the words the engine writes for those turns. */
const R = 0x44;
const U = 0x45;
const F = 0x46;

/** Builds a payload byte by byte, so a test can say what is wrong with it. */
function payload(
  version: number,
  scramble: readonly number[],
  user: readonly number[],
  trailing: readonly number[] = [],
  size = 3,
): string {
  const bytes = new Uint8Array(
    2 + 4 + scramble.length * 4 + 4 + user.length * 4 + trailing.length,
  );
  const view = new DataView(bytes.buffer);

  view.setUint8(0, version);
  view.setUint8(1, size);
  let offset = 2;
  for (const section of [scramble, user]) {
    view.setUint32(offset, section.length, true);
    offset += 4;
    for (const packed of section) {
      view.setUint32(offset, packed, true);
      offset += 4;
    }
  }
  bytes.set(trailing, offset);

  return base64url(bytes);
}

/** The same encoding the module uses, written out so the tests do not share it. */
function base64url(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary)
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '');
}

/** Its inverse, for the tests that take a payload apart again. */
function bytesOf(encoded: string): Uint8Array {
  const padded = encoded
    .replace(/-/g, '+')
    .replace(/_/g, '/')
    .padEnd(Math.ceil(encoded.length / 4) * 4, '=');
  return Uint8Array.from(atob(padded), (character) => character.charCodeAt(0));
}

describe('encodeSession and decodeSession', () => {
  it('carries a session there and back', () => {
    const session: SharedSession = { size: 3, scramble: [R, U, F], user: [U, R] };
    const encoded = encodeSession(session);

    expect(encoded).not.toBeNull();
    expect(decodeSession(encoded!)).toEqual(session);
  });

  it('carries a session that never had a scramble', () => {
    // Turned straight from a solved cube. Both counts are in the payload, so
    // an empty stretch needs no case of its own on either side.
    const session: SharedSession = { size: 3, scramble: [], user: [R] };
    const encoded = encodeSession(session);

    expect(decodeSession(encoded!)).toEqual(session);
  });

  it('writes the layout a link is fixed to, byte for byte', () => {
    // A known answer over the bytes rather than over the string alone. What
    // it is guarding is that the payload holds the actual scramble moves: a
    // version that went back to carrying a seed would still produce some
    // string, and only the layout says which.
    const encoded = encodeSession({ size: 3, scramble: [R], user: [U] })!;
    expect(encoded).toBe('AgMBAAAARAAAAAEAAABFAAAA');

    // version | size | scramble_count | R | user_count | U, every multi-byte
    // number little-endian.
    expect([...bytesOf(encoded)]).toEqual([
      SHARE_VERSION,
      3,
      1, 0, 0, 0,
      0x44, 0, 0, 0,
      1, 0, 0, 0,
      0x45, 0, 0, 0,
    ]);
  });

  it('writes a cube nothing has happened to as its size alone', () => {
    // How an untouched cube of any but the opening size is sent: both counts,
    // no moves, and the size in front of them.
    const encoded = encodeSession({ size: 4, scramble: [], user: [] });
    expect(bytesOf(encoded!)).toEqual(
      new Uint8Array([SHARE_VERSION, 4, 0, 0, 0, 0, 0, 0, 0, 0]),
    );
    expect(decodeSession(encoded!)).toEqual({ size: 4, scramble: [], user: [] });
  });

  it('refuses a record longer than the far side would take back', () => {
    const tooMany = new Array<number>(MAX_SHARED_MOVES + 1).fill(R);
    expect(encodeSession({ size: 3, scramble: tooMany, user: [] })).toBeNull();

    // And the bound itself is reachable, so it is an edge rather than an
    // off-by-one that refuses everything near it.
    const exactly = new Array<number>(MAX_SHARED_MOVES).fill(R);
    const encoded = encodeSession({ size: 3, scramble: exactly, user: [] });
    expect(encoded).not.toBeNull();
    expect(encoded!.length).toBeLessThanOrEqual(MAX_ENCODED_LENGTH);
    expect(decodeSession(encoded!)?.scramble).toHaveLength(MAX_SHARED_MOVES);
  });
});

describe('decodeSession refusals', () => {
  it('refuses text that is not base64url at all', () => {
    expect(decodeSession('')).toBeNull();
    expect(decodeSession('not base64!')).toBeNull();
    expect(decodeSession('AQ==AQ')).toBeNull();

    // A length one past a multiple of four cannot have come from any bytes.
    expect(decodeSession('AQAAA')).toBeNull();
  });

  it('measures the text before it unpacks it', () => {
    // Refused as text, so an enormous fragment never becomes an enormous
    // array on the way to being measured.
    const enormous = 'A'.repeat(MAX_ENCODED_LENGTH + 4);
    expect(decodeSession(enormous)).toBeNull();
  });

  it('refuses a version it does not know', () => {
    expect(decodeSession(payload(SHARE_VERSION + 1, [R], [U]))).toBeNull();
    expect(decodeSession(payload(0, [R], [U]))).toBeNull();
  });

  it('refuses a payload cut short of what its counts promise', () => {
    const bytes = bytesOf(payload(SHARE_VERSION, [R, U, F], [R]));

    for (let cut = 1; cut < bytes.length; cut += 1) {
      expect(decodeSession(base64url(bytes.subarray(0, cut)))).toBeNull();
    }
  });

  it('refuses anything left over after the second stretch', () => {
    expect(decodeSession(payload(SHARE_VERSION, [R], [U], [0, 0, 0, 0])))
      .toBeNull();
  });

  it('reads a payload of no moves as a cube of its size', () => {
    expect(decodeSession(payload(SHARE_VERSION, [], [], [], 5))).toEqual({
      size: 5,
      scramble: [],
      user: [],
    });
  });

  it('refuses the one axis code that names no axis', () => {
    // Two bits carry three axes, so exactly one value is not one of them --
    // and a hand-edited link is the only thing that can produce it.
    expect(decodeSession(payload(SHARE_VERSION, [R | 0x3], []))).toBeNull();
    expect(decodeSession(payload(SHARE_VERSION, [], [R | 0x3]))).toBeNull();
  });

  it('refuses a turns code that is no number of quarters', () => {
    expect(decodeSession(payload(SHARE_VERSION, [], [0x4c]))).toBeNull();
  });

  it('refuses a mask that is not a run of the cube it names', () => {
    // A wide move is a run, and a run is a move this application makes and
    // writes down, so it is carried rather than refused.
    const wide = 0x64; // axis X, one clockwise quarter, layers 1 and 2.
    expect(decodeSession(payload(SHARE_VERSION, [], [wide]))).not.toBeNull();

    // Two layers with a still one between them: no gesture and no command
    // makes it, and the move log has no notation for it. Refusing it at the
    // entrance is what keeps "a null notation is unreachable" true.
    const gapped = 0x54; // axis X, one clockwise quarter, layers 0 and 2.
    expect(decodeSession(payload(SHARE_VERSION, [], [gapped]))).toBeNull();

    // The whole cube at once, which is a rotation rather than a move.
    const rotation = 0x74;
    expect(decodeSession(payload(SHARE_VERSION, [], [rotation]))).toBeNull();

    // And a single layer past the edge of a three-layer cube.
    const beyond = 0x84;
    expect(decodeSession(payload(SHARE_VERSION, [], [beyond]))).toBeNull();

    // The same word on the cube that does have that layer: which masks may
    // travel is a question about the size, and the size is in the payload.
    expect(decodeSession(payload(SHARE_VERSION, [], [beyond], [], 5)))
      .not.toBeNull();
  });

  it('refuses a size no cube is built at', () => {
    // Written against the range rather than against the numbers either side
    // of it, so moving the ceiling does not quietly stop testing the edge.
    expect(decodeSession(payload(SHARE_VERSION, [], [R], [], 1))).toBeNull();
    expect(
      decodeSession(payload(SHARE_VERSION, [], [R], [], MAX_CUBE_SIZE + 1)),
    ).toBeNull();
    expect(decodeSession(payload(SHARE_VERSION, [], [R], [], 0))).toBeNull();
    expect(
      decodeSession(payload(SHARE_VERSION, [], [R], [], MAX_CUBE_SIZE)),
    ).not.toBeNull();
  });

  it('carries a bigger cube there and back', () => {
    // Rw and the slice behind it on a 5x5: axis X, one clockwise quarter, and
    // the layers written where the mask lives.
    const packed = (layers: number): number => 0x4 | (layers << 4);
    const session: SharedSession = {
      size: 5,
      scramble: [packed(0b11000)],
      user: [packed(0b01000)],
    };

    const encoded = encodeSession(session);
    expect(encoded).not.toBeNull();
    expect(decodeSession(encoded!)).toEqual(session);

    // The same record on a cube that has no such layers is refused, which is
    // the size and the masks being checked against each other.
    expect(encodeSession({ ...session, size: 3 })).toBeNull();
  });

  it('refuses a word that is no move', () => {
    expect(decodeSession(payload(SHARE_VERSION, [], [0]))).toBeNull();
  });

  it('refuses a count no payload of that size could hold', () => {
    // A count read out of a damaged payload can be any number at all, and it
    // is checked against the bytes actually present rather than multiplied out
    // into a request for bytes that are not there.
    const bytes = new Uint8Array(2 + 4 + 4);
    const view = new DataView(bytes.buffer);
    view.setUint8(0, SHARE_VERSION);
    view.setUint8(1, 3);
    view.setUint32(2, 0xffff_ffff, true);
    expect(decodeSession(base64url(bytes))).toBeNull();
  });
});

describe('a link that carries somebody\u2019s own cube', () => {
  /** A colouring of a solved cube of `size`, face by face. */
  const solvedPainting = (size: number): number[] => {
    const colours: number[] = [];
    for (let face = 0; face < 6; face += 1) {
      for (let cell = 0; cell < size * size; cell += 1) colours.push(face);
    }
    return colours;
  };

  it('carries the colours and the moves made after them', () => {
    const session: SharedPainting = {
      size: 3,
      painting: solvedPainting(3),
      user: [R, U],
    };

    const encoded = encodePainting(session);
    expect(encoded).not.toBeNull();
    expect(decodePainting(encoded!)).toEqual(session);
  });

  it('carries a colouring with no moves after it', () => {
    const session: SharedPainting = {
      size: 4,
      painting: solvedPainting(4),
      user: [],
    };

    const encoded = encodePainting(session);
    expect(encoded).not.toBeNull();
    expect(decodePainting(encoded!)).toEqual(session);
  });

  it('is refused by the reader of the other layout, and the other way round', () => {
    // The two never answer for each other: each refuses a version that is not
    // its own before reading a byte further, which is what lets a caller ask
    // both in turn and take whichever answers.
    const painted = encodePainting({
      size: 3,
      painting: solvedPainting(3),
      user: [],
    })!;
    expect(decodeSession(painted)).toBeNull();

    const moves = encodeSession({ size: 3, scramble: [R], user: [] })!;
    expect(decodePainting(moves)).toBeNull();
    expect(SHARE_PAINTED_VERSION).not.toBe(SHARE_VERSION);
  });

  it('will not write a colouring that is the wrong length or the wrong colours', () => {
    expect(
      encodePainting({ size: 3, painting: solvedPainting(4), user: [] }),
    ).toBeNull();
    expect(
      encodePainting({
        size: 3,
        painting: solvedPainting(3).map((c, i) => (i === 0 ? 6 : c)),
        user: [],
      }),
    ).toBeNull();
    expect(
      encodePainting({ size: 1, painting: solvedPainting(1), user: [] }),
    ).toBeNull();
  });

  it('will not read a payload that is short, long, or holds a colour that is not one', () => {
    const good = encodePainting({
      size: 3,
      painting: solvedPainting(3),
      user: [],
    })!;

    // Every truncation of a good payload is refused rather than half read.
    for (let cut = 1; cut < good.length; cut += 7) {
      expect(decodePainting(good.slice(0, cut))).toBeNull();
    }

    // And nothing may follow the moves.
    expect(decodePainting(`${good}AAAA`)).toBeNull();
    expect(decodePainting('not base64url!')).toBeNull();
    expect(decodePainting('')).toBeNull();
    expect(
      decodePainting('A'.repeat(MAX_PAINTED_ENCODED_LENGTH + 1)),
    ).toBeNull();
  });

  it('reads back every painted link it writes, at every size', () => {
    // The colours come on top of the moves, so a reader measuring by the
    // moves' own bound turned away links this writer had made.
    // A turn of layer 0 on X, the one layer every size has; R is layer 2.
    const outer = 0x14;
    for (let size = MIN_CUBE_SIZE; size <= MAX_CUBE_SIZE; size += 1) {
      const session: SharedPainting = {
        size,
        painting: solvedPainting(size),
        user: new Array<number>(MAX_SHARED_MOVES).fill(outer),
      };
      const encoded = encodePainting(session);
      expect(encoded, `size ${size}`).not.toBeNull();
      expect(encoded!.length).toBeLessThanOrEqual(MAX_PAINTED_ENCODED_LENGTH);
      expect(decodePainting(encoded!), `size ${size}`).toEqual(session);
    }

    // One move past the engine's bound is not written at all.
    expect(
      encodePainting({
        size: 3,
        painting: solvedPainting(3),
        user: new Array<number>(MAX_SHARED_MOVES + 1).fill(R),
      }),
    ).toBeNull();
  });
});
