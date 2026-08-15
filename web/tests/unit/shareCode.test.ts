import { describe, expect, it } from 'vitest';

import {
  decodeSession,
  encodeSession,
  MAX_ENCODED_LENGTH,
  SHARE_VERSION,
  type SharedSession,
} from '../../src/game/shareCode.ts';
import { MAX_SHARED_MOVES } from '../../src/wasm/CubeEngine.ts';

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
): string {
  const bytes = new Uint8Array(
    1 + 4 + scramble.length * 4 + 4 + user.length * 4 + trailing.length,
  );
  const view = new DataView(bytes.buffer);

  view.setUint8(0, version);
  let offset = 1;
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

describe('encodeSession and decodeSession', () => {
  it('carries a session there and back', () => {
    const session: SharedSession = { scramble: [R, U, F], user: [U, R] };
    const encoded = encodeSession(session);

    expect(encoded).not.toBeNull();
    expect(decodeSession(encoded!)).toEqual(session);
  });

  it('carries a session that never had a scramble', () => {
    // Turned straight from a solved cube. Both counts are in the payload, so
    // an empty stretch needs no case of its own on either side.
    const session: SharedSession = { scramble: [], user: [R] };
    const encoded = encodeSession(session);

    expect(decodeSession(encoded!)).toEqual(session);
  });

  it('writes the layout a link is fixed to, byte for byte', () => {
    // A known answer over the bytes rather than over the string alone. What
    // it is guarding is that the payload holds the actual scramble moves: a
    // version that went back to carrying a seed would still produce some
    // string, and only the layout says which.
    const encoded = encodeSession({ scramble: [R], user: [U] })!;
    expect(encoded).toBe('AQEAAABEAAAAAQAAAEUAAAA');

    const binary = atob(encoded.replace(/-/g, '+').replace(/_/g, '/') + '=');
    const bytes = Uint8Array.from(binary, (character) =>
      character.charCodeAt(0),
    );

    // version | scramble_count | R | user_count | U, every number little-endian.
    expect([...bytes]).toEqual([
      SHARE_VERSION,
      1, 0, 0, 0,
      0x44, 0, 0, 0,
      1, 0, 0, 0,
      0x45, 0, 0, 0,
    ]);
  });

  it('has no session to write for a cube nothing has happened to', () => {
    expect(encodeSession({ scramble: [], user: [] })).toBeNull();
  });

  it('refuses a record longer than the far side would take back', () => {
    const tooMany = new Array<number>(MAX_SHARED_MOVES + 1).fill(R);
    expect(encodeSession({ scramble: tooMany, user: [] })).toBeNull();

    // And the bound itself is reachable, so it is an edge rather than an
    // off-by-one that refuses everything near it.
    const exactly = new Array<number>(MAX_SHARED_MOVES).fill(R);
    const encoded = encodeSession({ scramble: exactly, user: [] });
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
    const whole = payload(SHARE_VERSION, [R, U, F], [R]);
    const bytes = Uint8Array.from(
      atob(whole.replace(/-/g, '+').replace(/_/g, '/').padEnd(
        Math.ceil(whole.length / 4) * 4,
        '=',
      )),
      (character) => character.charCodeAt(0),
    );

    for (let cut = 1; cut < bytes.length; cut += 1) {
      expect(decodeSession(base64url(bytes.subarray(0, cut)))).toBeNull();
    }
  });

  it('refuses anything left over after the second stretch', () => {
    expect(decodeSession(payload(SHARE_VERSION, [R], [U], [0, 0, 0, 0])))
      .toBeNull();
  });

  it('refuses a payload holding no moves at all', () => {
    // The counts are well formed and the payload is whole; it just describes
    // nothing. Keeping it out here is what lets the engine treat "was a
    // buffer asked for" as an unambiguous question.
    expect(decodeSession(payload(SHARE_VERSION, [], []))).toBeNull();
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

  it('refuses a move that turns more than one layer', () => {
    // A wide move: the packing carries it happily and this cube has no way of
    // making one, so the move log has no notation for it. Refusing it at the
    // entrance is what keeps "a null notation is unreachable" true.
    const wide = 0x64; // axis X, one clockwise quarter, layers 1 and 2.
    expect(decodeSession(payload(SHARE_VERSION, [], [wide]))).toBeNull();

    // And a single layer past the edge of a three-layer cube.
    const beyond = 0x84;
    expect(decodeSession(payload(SHARE_VERSION, [], [beyond]))).toBeNull();
  });

  it('refuses a word that is no move', () => {
    expect(decodeSession(payload(SHARE_VERSION, [], [0]))).toBeNull();
  });

  it('refuses a count no payload of that size could hold', () => {
    // A count read out of a damaged payload can be any number at all, and it
    // is checked against the bytes actually present rather than multiplied out
    // into a request for bytes that are not there.
    const bytes = new Uint8Array(1 + 4 + 4);
    const view = new DataView(bytes.buffer);
    view.setUint8(0, SHARE_VERSION);
    view.setUint32(1, 0xffff_ffff, true);
    expect(decodeSession(base64url(bytes))).toBeNull();
  });
});
