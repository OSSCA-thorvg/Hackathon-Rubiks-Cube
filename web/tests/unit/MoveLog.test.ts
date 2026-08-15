import { describe, expect, it, vi } from 'vitest';

import { MoveLog, type MoveLogFrame } from '../../src/game/MoveLog.ts';

/** Packed words for the moves used below, as the engine writes them. */
const R = 0x44;
const U = 0x45;
const F = 0x46;
const R_PRIME = 0x40;

/** A log over a record the test hands over, and the list it draws into. */
function createLog(moves: readonly number[]) {
  const list = document.createElement('ol');
  document.body.replaceChildren(list);

  const timelineMove = vi.fn((index: number): number => moves[index] ?? 0);
  return { list, timelineMove, log: new MoveLog(list, { timelineMove }) };
}

/** What each entry says, and where it stands, as one readable row. */
function entries(list: HTMLElement): string[] {
  return [...list.children].map((child) => {
    const item = child as HTMLElement;
    return `${item.textContent} ${item.dataset.part} ${item.dataset.state}`;
  });
}

/** A whole record with everything in it applied. */
function frame(length: number, cursor: number, scrambleEnd: number): MoveLogFrame {
  return { length, cursor, scrambleEnd };
}

describe('MoveLog', () => {
  it('writes out the record, marking where the cursor is', () => {
    const { list, log } = createLog([R, U, F, R_PRIME]);

    log.update(frame(4, 4, 3));

    // The scramble and the user's own moves come off one list and are told
    // apart by where the boundary is, not by which query they came from.
    expect(entries(list)).toEqual([
      'R scramble applied',
      'U scramble applied',
      'F scramble applied',
      "R' user current",
    ]);
  });

  it('has nothing to write for a record with nothing in it', () => {
    const { list, log } = createLog([]);

    log.update(frame(0, 0, 0));
    expect(list.children).toHaveLength(0);
  });

  it('marks what a rewind has taken off the cube', () => {
    const { list, log } = createLog([R, U, F, R_PRIME]);

    log.update(frame(4, 4, 3));
    log.update(frame(4, 2, 3));

    // Two of them are still on the cube and two are waiting to be put back --
    // and one of the two waiting is a move of the scramble, which is exactly
    // what a rewind that ran past the boundary leaves behind. Which stretch
    // an entry belongs to does not move with the cursor.
    expect(entries(list)).toEqual([
      'R scramble applied',
      'U scramble current',
      'F scramble pending',
      "R' user pending",
    ]);
  });

  it('has no move marked on a cube a rewind emptied', () => {
    const { list, log } = createLog([R, U]);

    log.update(frame(2, 2, 2));
    log.update(frame(2, 0, 2));

    // Everything is waiting and nothing is current: there is no last applied
    // move, which is the one reading where the mark is nowhere.
    expect(entries(list)).toEqual([
      'R scramble pending',
      'U scramble pending',
    ]);
  });

  it('redraws when the record changed underneath a length that did not', () => {
    const moves = [R, U, F];
    const list = document.createElement('ol');
    const log = new MoveLog(list, {
      timelineMove: (index: number): number => moves[index] ?? 0,
    });

    log.update(frame(3, 3, 2));
    expect(entries(list)).toEqual([
      'R scramble applied',
      'U scramble applied',
      'F user current',
    ]);

    // One move taken back...
    log.update(frame(3, 2, 2));

    // ...and a different one made in its place, which cuts off what there was
    // to redo. The record is the same length it was two frames ago with
    // something else at the end of it, so a redraw that was decided on the
    // length would leave the move that is no longer there on the screen.
    moves[2] = R_PRIME;
    log.update(frame(3, 3, 2));
    expect(entries(list)).toEqual([
      'R scramble applied',
      'U scramble applied',
      "R' user current",
    ]);
  });

  it('draws nothing at all for a frame that says the same thing', () => {
    const { log, timelineMove } = createLog([R, U]);

    log.update(frame(2, 2, 2));
    expect(timelineMove).toHaveBeenCalledTimes(2);

    // Frames keep coming while a pattern is watched or the view is swept, and
    // none of them touches the record.
    timelineMove.mockClear();
    log.update(frame(2, 2, 2));
    log.update(frame(2, 2, 2));
    expect(timelineMove).not.toHaveBeenCalled();
  });
});
