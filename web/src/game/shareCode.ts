import {
  MAX_CUBE_SIZE,
  MAX_SHARED_MOVES,
  MIN_CUBE_SIZE,
} from '../wasm/CubeEngine.ts';
import { layerRun, unpackMove } from './notation.ts';

/**
 * How one cube state travels as text, and how it is read back.
 *
 * What is carried is a scramble sequence and the user's own applied moves --
 * the two stretches of the engine's record, split at the scramble boundary --
 * and nothing else. A sticker array would be shorter and would need checking
 * on arrival, because an arbitrary one is not a cube any turning could reach;
 * a list of moves is legal by construction, because every one of them is
 * played into the cube the ordinary way.
 *
 * A seed is deliberately not carried. Carrying one would make the generator,
 * its arithmetic, and its agreement between native and WASM a permanent
 * compatibility contract of every link ever shared: fix the scramble rules
 * afterwards and old links quietly restore a different cube. Carrying the
 * moves costs eighty bytes for a twenty-move scramble and removes that
 * coupling entirely.
 */

/**
 * Which layout the bytes are in.
 *
 * Bumped rather than migrated: a payload whose version this build does not
 * know is dropped and the page opens fresh. Version one carried no size,
 * because there was one size; a mask means nothing without the cube it was
 * taken from, so the size arriving is what made this version two, and links
 * written before it open a fresh cube.
 */
export const SHARE_VERSION = 2;

/**
 * The two stretches one shared state is made of, as packed words.
 *
 * Packed rather than decoded, because nothing between the engine and the text
 * has any use for what a move means: the engine hands these words over and
 * takes them back, and the encoding writes them down.
 */
export type SharedSession = {
  /** How many layers the cube these moves were made on has. */
  readonly size: number;
  /** Everything the cube was handed, which may be empty. */
  readonly scramble: readonly number[];
  /** The user's own moves that are on the cube, without a rewound tail. */
  readonly user: readonly number[];
};

/**
 * The byte layout, written out because it is a permanent contract.
 *
 * ```text
 * version(1) | size(1) | scramble_count(4) | packed(4) x scramble_count
 *                      | user_count(4)     | packed(4) x user_count
 * ```
 *
 * The size is one byte and will never need another: a packed move carries
 * twenty-eight layers at the outside, and no cube anyone would look at comes
 * anywhere near that.
 *
 * Every multi-byte field is little-endian, and the reader says so explicitly
 * through DataView rather than laying a Uint32Array over the bytes -- a typed
 * array follows the platform's own byte order, which is no basis for a format
 * that has to outlive the machine that wrote it.
 *
 * The counts are four bytes each where two would do. Two would bring a bound
 * of sixty-five thousand moves along with the rule that refuses it, the
 * message that explains it and the tests that hold it -- for a case reachable
 * only by turning a layer every second for eighteen hours. Two more bytes
 * remove the case instead of handling it.
 */
const VERSION_BYTES = 1;
const SIZE_BYTES = 1;
const COUNT_BYTES = 4;
const MOVE_BYTES = 4;

/** The smallest payload that could still be a session: both counts, no moves. */
const MIN_PAYLOAD_BYTES = VERSION_BYTES + SIZE_BYTES + COUNT_BYTES * 2;

/** The largest, at the bound the engine's restore buffer takes. */
const MAX_PAYLOAD_BYTES = MIN_PAYLOAD_BYTES + MOVE_BYTES * MAX_SHARED_MOVES;

/**
 * The longest encoded string that could still be a payload.
 *
 * Measured before anything is decoded, which is the point of having it: a
 * fragment of any size can be pasted into the address bar, and measuring the
 * text first means an enormous one is refused as text rather than being
 * unpacked into memory to be measured as bytes.
 */
export const MAX_ENCODED_LENGTH = Math.ceil((MAX_PAYLOAD_BYTES * 4) / 3);

/** The alphabet base64url uses, which is the whole of what may appear. */
const ENCODED_PATTERN = /^[A-Za-z0-9_-]+$/;

/**
 * Whether a word is a move a cube of `size` may be handed.
 *
 * Stricter than "a move": the mask is held to one unbroken run of that cube's
 * layers, short of all of them. `CubeMove` is happy to hold any set at all,
 * and nothing in this application can make one with a gap in it or one that
 * turns the whole cube -- so the move log treats a notation of null as
 * unreachable rather than drawing it. A link is the one way a word could
 * arrive from outside, so the same rule the notation reads by goes on the way
 * in, and the engine checks it again on its own side.
 */
function isSharableMove(packed: number, size: number): boolean {
  const move = unpackMove(packed);
  if (move === null) return false;

  return layerRun(move.layers, size) !== null;
}

/** Whether a size is one this application builds a cube at. */
function isSharableSize(size: number): boolean {
  return (
    Number.isInteger(size) && size >= MIN_CUBE_SIZE && size <= MAX_CUBE_SIZE
  );
}

/**
 * Writes one session as a base64url string, or returns null.
 *
 * Null for the sessions there is no link for: an empty record, one longer than
 * the far end can take back, and one holding a word this version cannot carry.
 * The share control never offers any of them, so this is the assembly side
 * agreeing with the reading side rather than a path a person walks down.
 */
export function encodeSession(session: SharedSession): string | null {
  const moves = [...session.scramble, ...session.user];
  if (!isSharableSize(session.size)) return null;
  if (moves.length === 0 || moves.length > MAX_SHARED_MOVES) return null;
  if (!moves.every((packed) => isSharableMove(packed, session.size))) {
    return null;
  }

  const bytes = new Uint8Array(MIN_PAYLOAD_BYTES + MOVE_BYTES * moves.length);
  const view = new DataView(bytes.buffer);

  view.setUint8(0, SHARE_VERSION);
  view.setUint8(VERSION_BYTES, session.size);
  let offset = VERSION_BYTES + SIZE_BYTES;
  for (const section of [session.scramble, session.user]) {
    view.setUint32(offset, section.length, true);
    offset += COUNT_BYTES;
    for (const packed of section) {
      view.setUint32(offset, packed, true);
      offset += MOVE_BYTES;
    }
  }

  return base64urlEncode(bytes);
}

/**
 * Reads a base64url string back as a session, or returns null.
 *
 * Everything is checked here, before a single move reaches the engine: the
 * length of the text, its alphabet, the version, the two counts against the
 * actual byte length in both directions, and every word. A payload with
 * anything left over after the second stretch is refused as well -- a link
 * carries one session and nothing after it.
 *
 * There is one answer for every way of failing, because there is one thing to
 * do about all of them: open the page with a fresh cube.
 */
export function decodeSession(encoded: string): SharedSession | null {
  if (encoded.length === 0 || encoded.length > MAX_ENCODED_LENGTH) return null;
  if (!ENCODED_PATTERN.test(encoded)) return null;

  const bytes = base64urlDecode(encoded);
  if (bytes === null || bytes.length < MIN_PAYLOAD_BYTES) return null;

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint8(0) !== SHARE_VERSION) return null;

  // Read before any move is, because a mask means nothing without it: which
  // layers a word may name is a question about this cube.
  const size = view.getUint8(VERSION_BYTES);
  if (!isSharableSize(size)) return null;

  // Walked by the reader rather than handed to it, so the two stretches are
  // read by one function called twice instead of by a loop whose result has to
  // be taken apart again -- which is what needed a type assertion to do.
  let offset = VERSION_BYTES + SIZE_BYTES;
  const readSection = (): number[] | null => {
    // Checked before it is used as a length: a count read out of a truncated
    // payload can be any number at all, and multiplying it out first is how a
    // reader ends up asking for bytes that are not there.
    if (offset + COUNT_BYTES > bytes.length) return null;
    const count = view.getUint32(offset, true);
    offset += COUNT_BYTES;

    if (count > MAX_SHARED_MOVES) return null;
    if (offset + count * MOVE_BYTES > bytes.length) return null;

    const moves: number[] = [];
    for (let index = 0; index < count; index += 1) {
      const packed = view.getUint32(offset, true);
      offset += MOVE_BYTES;
      if (!isSharableMove(packed, size)) return null;
      moves.push(packed);
    }
    return moves;
  };

  const scramble = readSection();
  if (scramble === null) return null;
  const user = readSection();
  if (user === null) return null;

  // A session of nothing is not a state anybody shared, and it is also the
  // count the engine's buffer refuses -- so keeping it out here is what makes
  // "was a buffer asked for at all" an unambiguous question over there. The
  // other end of that range needs nothing here: the text was measured before
  // it was decoded, and a payload that fits under MAX_ENCODED_LENGTH cannot
  // describe more than MAX_SHARED_MOVES moves between its two counts.
  if (scramble.length + user.length === 0) return null;

  // Nothing may follow the second stretch.
  if (offset !== bytes.length) return null;

  return { size, scramble, user };
}

/** How many characters of the input one chunk of the conversion takes. */
const CHUNK = 0x8000;

/** Writes bytes as base64url: the URL alphabet, and no padding to carry. */
function base64urlEncode(bytes: Uint8Array): string {
  // In chunks, because spreading a payload of thousands of bytes into an
  // argument list is what overflows a call stack on the large end of the range.
  let binary = '';
  for (let start = 0; start < bytes.length; start += CHUNK) {
    binary += String.fromCharCode(...bytes.subarray(start, start + CHUNK));
  }

  return btoa(binary).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

/** Reads base64url back, or returns null for text that is not any. */
function base64urlDecode(encoded: string): Uint8Array | null {
  // A length of one past a multiple of four cannot come from any byte string,
  // and is the one malformed case atob accepts in some engines.
  if (encoded.length % 4 === 1) return null;

  const padded = encoded
    .replace(/-/g, '+')
    .replace(/_/g, '/')
    .padEnd(Math.ceil(encoded.length / 4) * 4, '=');

  let binary: string;
  try {
    binary = atob(padded);
  } catch {
    return null;
  }

  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index);
  }
  return bytes;
}
