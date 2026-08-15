import { beforeEach, describe, expect, it } from 'vitest';

import {
  RECENT_LIMIT,
  SessionRecords,
  type SolveRecord,
} from '../../src/game/SessionRecords.ts';

function createBoard(): {
  records: SessionRecords;
  best: HTMLParagraphElement;
  list: HTMLOListElement;
} {
  const best = document.createElement('p');
  const list = document.createElement('ol');
  document.body.replaceChildren(best, list);
  return { records: new SessionRecords(best, list), best, list };
}

function solve(elapsedMs: number, userMoveCount = 12): SolveRecord {
  return { elapsedMs, scrambleLength: 20, userMoveCount };
}

describe('SessionRecords', () => {
  let board = createBoard();

  beforeEach(() => {
    board = createBoard();
  });

  it('says so before there is anything to say', () => {
    expect(board.records.best).toBeNull();
    expect(board.records.recent).toHaveLength(0);
    expect(board.best.textContent).toBe('No solves yet.');
    expect(board.list.children).toHaveLength(0);
  });

  it('keeps the first solve and calls it the best', () => {
    expect(board.records.add(solve(12_340))).toBe(true);

    expect(board.records.best?.elapsedMs).toBe(12_340);
    expect(board.best.textContent).toBe('Best 00:12.34');
    expect(board.list.children).toHaveLength(1);
    expect(board.list.children[0]?.textContent).toBe(
      '00:12.34 · 12 moves · 20-move scramble',
    );
  });

  it('only calls a solve the best when it is faster than the last one', () => {
    expect(board.records.add(solve(12_340))).toBe(true);
    expect(board.records.add(solve(20_000))).toBe(false);
    expect(board.records.best?.elapsedMs).toBe(12_340);

    // Equal is not faster: whoever got there first keeps it.
    expect(board.records.add(solve(12_340))).toBe(false);
    expect(board.records.add(solve(9_990))).toBe(true);
    expect(board.best.textContent).toBe('Best 00:09.99');
  });

  it('shows the latest few, newest first', () => {
    for (let index = 0; index < RECENT_LIMIT + 2; index += 1) {
      board.records.add(solve(10_000 + index * 1000, index));
    }

    expect(board.records.recent).toHaveLength(RECENT_LIMIT);
    expect(board.list.children).toHaveLength(RECENT_LIMIT);

    // The newest is at the top, and the two oldest have fallen off the end.
    expect(board.records.recent[0]?.userMoveCount).toBe(RECENT_LIMIT + 1);
    expect(board.records.recent.at(-1)?.userMoveCount).toBe(2);

    // The best is still the first one, which is no longer on the list: the
    // fastest of the sitting is not the same question as the latest few.
    expect(board.records.best?.elapsedMs).toBe(10_000);
  });
});
