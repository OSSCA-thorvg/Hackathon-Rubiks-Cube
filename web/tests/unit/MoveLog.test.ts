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
  return {
    list,
    timelineMove,
    log: new MoveLog(list, { timelineMove, cubeSize: () => 3 }),
  };
}

/** What each entry says, and where it stands, as one readable row. */
function entries(list: HTMLElement): string[] {
  return [...list.children].map((child) => {
    const item = child as HTMLElement;
    return `${item.textContent} ${item.dataset.state}`;
  });
}

/** One reading of the record: how long, how much of it is on, where it splits. */
function frame(
  length: number,
  cursor: number,
  scrambleEnd: number,
): MoveLogFrame {
  return { length, cursor, scrambleEnd };
}

describe('MoveLog', () => {
  it('writes out the moves of your own, marking where the cursor is', () => {
    const { list, log } = createLog([R, U, F, R_PRIME]);

    log.update(frame(4, 4, 2));

    // Two of the four are the scramble, and the log begins where it ends.
    expect(entries(list)).toEqual(['F applied', "R' current"]);
  });

  it('has nothing to write for a scramble nobody has moved on', () => {
    const { list, log, timelineMove } = createLog([R, U, F]);

    // A whole scramble, all of it on the cube, and none of it the user's.
    log.update(frame(3, 3, 3));
    expect(list.children).toHaveLength(0);

    // Not drawn and not even read: the boundary is where the drawing starts,
    // so the moves below it are never asked for.
    expect(timelineMove).not.toHaveBeenCalled();
  });

  it('has nothing to write for a record with nothing in it', () => {
    const { list, log } = createLog([]);

    log.update(frame(0, 0, 0));
    expect(list.children).toHaveLength(0);
  });

  it('marks what a rewind has taken off the cube', () => {
    const { list, log } = createLog([R, U, F, R_PRIME]);

    log.update(frame(4, 4, 2));
    log.update(frame(4, 3, 2));

    // One of them is still on the cube and one is waiting to be put back.
    expect(entries(list)).toEqual(['F current', "R' pending"]);

    // Down to the end of the scramble: both are waiting, and nothing is
    // marked, because none of these moves is on the cube any more.
    log.update(frame(4, 2, 2));
    expect(entries(list)).toEqual(['F pending', "R' pending"]);

    // And on into the scramble, where a solve goes. The cursor is below
    // everything drawn here, so it marks nothing rather than the first entry.
    log.update(frame(4, 1, 2));
    expect(entries(list)).toEqual(['F pending', "R' pending"]);
  });

  it('follows a boundary that a new move pulled down', () => {
    const { list, log } = createLog([R, U, F]);

    // A solve took the cursor to nothing, and a move was made there: the
    // record cut the scramble off behind it, so what was scramble a moment
    // ago is a move of the user's own now, and the log says so.
    log.update(frame(3, 3, 3));
    expect(entries(list)).toEqual([]);

    log.update(frame(1, 1, 0));
    expect(entries(list)).toEqual(['R current']);
  });

  it('redraws when the record changed underneath a length that did not', () => {
    const moves = [R, U, F];
    const list = document.createElement('ol');
    const log = new MoveLog(list, {
      cubeSize: (): number => 3,
      timelineMove: (index: number): number => moves[index] ?? 0,
    });

    log.update(frame(3, 3, 1));
    expect(entries(list)).toEqual(['U applied', 'F current']);

    // One move taken back...
    log.update(frame(3, 2, 1));

    // ...and a different one made in its place, which cuts off what there was
    // to redo. The record is the same length it was two frames ago with
    // something else at the end of it, so a redraw that was decided on the
    // length would leave the move that is no longer there on the screen.
    moves[2] = R_PRIME;
    log.update(frame(3, 3, 1));
    expect(entries(list)).toEqual(['U applied', "R' current"]);
  });

  it('draws nothing at all for a frame that says the same thing', () => {
    const { log, timelineMove } = createLog([R, U]);

    log.update(frame(2, 2, 0));
    expect(timelineMove).toHaveBeenCalledTimes(2);

    // Frames keep coming while a pattern is watched or the view is swept, and
    // none of them touches the record.
    timelineMove.mockClear();
    log.update(frame(2, 2, 0));
    log.update(frame(2, 2, 0));
    expect(timelineMove).not.toHaveBeenCalled();
  });
});
