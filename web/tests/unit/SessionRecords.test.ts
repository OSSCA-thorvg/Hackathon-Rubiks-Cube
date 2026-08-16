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

function solve(
  elapsedMs: number,
  userMoveCount = 12,
  cubeSize = 3,
): SolveRecord {
  return { elapsedMs, cubeSize, scrambleLength: 20, userMoveCount };
}

describe('SessionRecords', () => {
  let board = createBoard();

  beforeEach(() => {
    board = createBoard();
  });

  it('says so before there is anything to say', () => {
    expect(board.best.textContent).toBe('No solves yet.');
    expect(board.list.children).toHaveLength(0);
  });

  it('keeps the first solve and calls it the best', () => {
    expect(board.records.add(solve(12_340))).toBe(true);

    expect(board.best.textContent).toBe('Best 3×3 00:12.34');
    expect(board.list.children).toHaveLength(1);
    expect(board.list.children[0]?.textContent).toBe(
      '00:12.34 · 3×3 · 12 moves · 20-move scramble',
    );
  });

  it('only calls a solve the best when it is faster than the last one', () => {
    expect(board.records.add(solve(12_340))).toBe(true);
    expect(board.records.add(solve(20_000))).toBe(false);
    expect(board.best.textContent).toBe('Best 3×3 00:12.34');

    // Equal is not faster: whoever got there first keeps it.
    expect(board.records.add(solve(12_340))).toBe(false);
    expect(board.records.add(solve(9_990))).toBe(true);
    expect(board.best.textContent).toBe('Best 3×3 00:09.99');
  });

  it('keeps a best for each cube, because the times are not comparable', () => {
    expect(board.records.add(solve(12_340, 12, 3))).toBe(true);

    // Slower than the 3x3 above and still a best: it is the first 5x5 there
    // has been, and a 5x5 in five minutes is not worse than a 3x3 in twelve
    // seconds -- it is a different question.
    expect(board.records.add(solve(300_000, 220, 5))).toBe(true);
    expect(board.best.textContent).toBe(
      'Best 3×3 00:12.34 · Best 5×5 05:00.00',
    );

    // And the smaller cube's best is untouched by the larger one's.
    expect(board.records.add(solve(20_000, 14, 3))).toBe(false);
    expect(board.records.add(solve(250_000, 200, 5))).toBe(true);
    expect(board.best.textContent).toBe(
      'Best 3×3 00:12.34 · Best 5×5 04:10.00',
    );
  });

  it('shows the latest few, newest first', () => {
    for (let index = 0; index < RECENT_LIMIT + 2; index += 1) {
      board.records.add(solve(10_000 + index * 1000, index));
    }

    const written = [...board.list.children].map((item) => item.textContent);
    expect(written).toHaveLength(RECENT_LIMIT);

    // The newest is at the top, and the two oldest have fallen off the end.
    expect(written[0]).toContain(`${RECENT_LIMIT + 1} moves`);
    expect(written.at(-1)).toContain('2 moves');

    // The best is still the first one, which is no longer on the list: the
    // fastest of the sitting is not the same question as the latest few.
    expect(board.best.textContent).toBe('Best 3×3 00:10.00');
  });
});
