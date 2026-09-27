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

describe('MoveLog with somewhere to go', () => {
  /** A middle slice, which only a drag makes. */
  const M = 0x20;

  function createPickable(moves: readonly number[]) {
    const list = document.createElement('ol');
    document.body.replaceChildren(list);
    const onPick = vi.fn();
    const log = new MoveLog(
      list,
      { timelineMove: (index) => moves[index] ?? 0, cubeSize: () => 3 },
      { onPick },
    );
    const buttons = (): HTMLButtonElement[] => [
      ...list.querySelectorAll<HTMLButtonElement>('button'),
    ];
    return { list, log, onPick, buttons };
  }

  it('makes each entry a button that hands back its record index', () => {
    const { log, onPick, buttons, list } = createPickable([R, U, F, R_PRIME]);
    log.update(frame(4, 4, 2));

    // The entries still read as the moves and nothing else.
    expect(entries(list)).toEqual(['F applied', "R' current"]);

    buttons()[0]!.click();
    expect(onPick).toHaveBeenCalledWith(2);
    expect(buttons()[1]!.getAttribute('aria-current')).toBe('step');
    expect(buttons()[0]!.getAttribute('aria-label')).toBe('Move 1, F');
  });

  it('shows each move in the colour of the face its letter names', () => {
    const { log, list } = createPickable([R, U, F, R_PRIME, M]);
    log.update(frame(5, 5, 0));

    const dots = [...list.querySelectorAll<HTMLElement>('.move-dot')].map(
      (dot) => dot.dataset.dot,
    );
    // A slice sits between two colours and is drawn in neither.
    expect(dots).toEqual(['r', 'u', 'f', 'r', 'slice']);
  });

  it('has one tab stop, on the move the cube is at, and walks with the arrows', () => {
    const { log, buttons } = createPickable([R, U, F]);
    log.update(frame(3, 2, 0));

    expect(buttons().map((button) => button.tabIndex)).toEqual([-1, 0, -1]);

    buttons()[1]!.focus();
    buttons()[1]!.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }),
    );
    expect(document.activeElement).toBe(buttons()[2]);
    expect(buttons().map((button) => button.tabIndex)).toEqual([-1, -1, 0]);

    buttons()[2]!.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'Home', bubbles: true }),
    );
    expect(document.activeElement).toBe(buttons()[0]);
  });

  it('keeps focus on the same move when the list is redrawn under it', () => {
    const { log, buttons } = createPickable([R, U, F]);
    log.update(frame(3, 3, 0));

    buttons()[0]!.focus();
    // A walk back is under way; a step keeps the entries and moves the marks.
    log.update(frame(3, 2, 0));
    expect(document.activeElement).toBe(buttons()[0]);
    expect((document.activeElement as HTMLElement).dataset.index).toBe('0');

    // And the one stop is where the focus is, not on the move the cube has
    // walked to -- a second stop further on would be where Tab went next,
    // rather than out of the list.
    expect(buttons().map((button) => button.tabIndex)).toEqual([0, -1, -1]);
  });

  it('hands focus to the entry that replaces the one holding it', () => {
    const moves = [R, U, F];
    const { log, buttons } = createPickable(moves);
    log.update(frame(3, 3, 0));
    log.update(frame(3, 1, 0));
    const old = buttons()[1]!;
    old.focus();

    // A move made here cuts the tail the focused entry was in.
    moves.splice(1, 2, R_PRIME, U);
    log.update(frame(3, 3, 0));
    expect(buttons()[1]).not.toBe(old);
    expect(document.activeElement).toBe(buttons()[1]);
    expect(buttons().map((button) => button.tabIndex)).toEqual([-1, 0, -1]);

    // An index that is gone altogether sends it to where the cube is.
    buttons()[2]!.focus();
    moves.splice(1, 2, F);
    log.update(frame(2, 2, 0));
    expect(document.activeElement).toBe(buttons()[1]);
    expect(buttons().map((button) => button.tabIndex)).toEqual([-1, 0]);
  });

  it('moves the stop back to the cursor once the keyboard is elsewhere', () => {
    const { log, buttons } = createPickable([R, U, F]);
    log.update(frame(3, 3, 0));
    buttons()[0]!.focus();
    log.update(frame(3, 2, 0));
    expect(buttons().map((button) => button.tabIndex)).toEqual([0, -1, -1]);

    buttons()[0]!.blur();
    log.update(frame(3, 3, 0));
    expect(buttons().map((button) => button.tabIndex)).toEqual([-1, -1, 0]);
  });

  it('stops answering presses at teardown', () => {
    const { log, onPick, buttons } = createPickable([R, U]);
    log.update(frame(2, 2, 0));

    log.teardown();
    buttons()[0]!.click();
    expect(onPick).not.toHaveBeenCalled();
  });
});

describe('MoveLog keeping what is still right', () => {
  /** A pickable log over a record the test goes on editing. */
  function createLive(moves: number[]) {
    const list = document.createElement('ol');
    document.body.replaceChildren(list);
    const engine = {
      timelineMove: vi.fn((index: number): number => moves[index] ?? 0),
      cubeSize: (): number => 3,
    };
    const log = new MoveLog(list, engine, { onPick: vi.fn() });
    return { list, log, engine };
  }

  /** Everything about the entries a person or a screen reader can tell apart. */
  function reading(list: HTMLElement): string[] {
    return [...list.children].map((child) => {
      const item = child as HTMLElement;
      const button = item.querySelector('button')!;
      return [
        item.textContent,
        item.dataset.state,
        button.getAttribute('aria-label'),
        button.getAttribute('aria-current'),
        button.tabIndex,
      ].join(' | ');
    });
  }

  /** What a log that has never drawn anything draws for the same record. */
  function fresh(moves: readonly number[], at: MoveLogFrame): string[] {
    const list = document.createElement('ol');
    new MoveLog(
      list,
      { timelineMove: (index) => moves[index] ?? 0, cubeSize: () => 3 },
      { onPick: vi.fn() },
    ).update(at);
    return reading(list);
  }

  it('keeps every entry through a step of the cursor, and makes none', () => {
    const moves = [R, U, F, R_PRIME, R, U];
    const { list, log } = createLive(moves);
    log.update(frame(6, 6, 0));
    const before = [...list.children];

    const made = vi.spyOn(document, 'createElement');
    log.update(frame(6, 4, 0));
    log.update(frame(6, 5, 0));
    expect(made).not.toHaveBeenCalled();
    made.mockRestore();

    // The same elements, marked where the cursor now is.
    expect([...list.children]).toEqual(before);
    expect(reading(list)).toEqual(fresh(moves, frame(6, 5, 0)));
  });

  it('adds one entry for a move made at the end', () => {
    const moves = [R, U];
    const { list, log } = createLive(moves);
    log.update(frame(2, 2, 0));
    const before = [...list.children];

    moves.push(F);
    log.update(frame(3, 3, 0));
    expect([...list.children].slice(0, 2)).toEqual(before);
    expect(reading(list)).toEqual(fresh(moves, frame(3, 3, 0)));
  });

  it('writes again from where a new move cut the record', () => {
    const moves = [R, U, F];
    const { list, log } = createLive(moves);
    log.update(frame(3, 3, 0));
    log.update(frame(3, 1, 0));
    const first = list.children[0];

    // Two undone, then a different move in their place: the length drops to
    // two and the second entry is a different move.
    moves.splice(1, 2, R_PRIME);
    log.update(frame(2, 2, 0));
    expect(list.children[0]).toBe(first);
    expect(reading(list)).toEqual(fresh(moves, frame(2, 2, 0)));
  });

  it('draws what a fresh log would draw, whatever the record went through', () => {
    // A fixed walk through the ways a record changes: steps both ways, jumps,
    // moves at the end, cuts, and the boundary moving under it.
    const moves: number[] = [R, U, F];
    const { list, log } = createLive(moves);
    let length = 3;
    let cursor = 3;
    let scrambleEnd = 1;
    const words = [R, U, F, R_PRIME];

    for (let step = 0; step < 60; step += 1) {
      const kind = (step * 7 + 3) % 5;
      if (kind === 0 && cursor > 0) cursor -= 1;
      else if (kind === 1 && cursor < length) cursor += 1;
      else if (kind === 2) {
        // A move made here cuts whatever was ahead of it.
        moves.splice(cursor, moves.length - cursor, words[step % 4]!);
        length = moves.length;
        cursor = length;
      } else if (kind === 3) cursor = step % (length + 1);
      else scrambleEnd = Math.min(cursor, step % 3);

      const at = frame(length, cursor, scrambleEnd);
      log.update(at);
      expect(reading(list), `step ${step}`).toEqual(fresh(moves, at));
      expect(
        [...list.querySelectorAll('button')].filter((b) => b.tabIndex === 0)
          .length,
      ).toBe(list.children.length === 0 ? 0 : 1);
    }
  });

  it('reads each move once a step, however long the record', () => {
    const moves = new Array<number>(4096).fill(R);
    const { list, log, engine } = createLive(moves);
    log.update(frame(4096, 4096, 0));
    engine.timelineMove.mockClear();

    const made = vi.spyOn(document, 'createElement');
    log.update(frame(4096, 4095, 0));
    expect(made).not.toHaveBeenCalled();
    made.mockRestore();
    expect(engine.timelineMove).toHaveBeenCalledTimes(4096);
    expect(list.children).toHaveLength(4096);
  });
});
