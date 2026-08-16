import { formatElapsed } from './SolveTimer.ts';

/** One finished solve, as it is worth remembering. */
export type SolveRecord = {
  readonly elapsedMs: number;
  /** Which cube it was, because a time means nothing without one. */
  readonly cubeSize: number;
  /** How many moves the cube was handed, which is what was solved. */
  readonly scrambleLength: number;
  /** How many of the solver's own moves were on it at the end. */
  readonly userMoveCount: number;
};

/** How many of the latest solves stay on the board. */
export const RECENT_LIMIT = 5;

/**
 * The solves of this sitting, and the panel they are written on.
 *
 * Nothing here outlives the page, and that is the whole design rather than a
 * shortcoming of it. A stored record would have to be earned the same way
 * every time to be worth anything, and this application cannot promise that
 * -- a reload starts a clean session by rule, so what would be restored is a
 * table of times with no way back to the sessions that made them. Kept in
 * memory, the board is what it looks like: how today's sitting has gone.
 *
 * Owns its two elements the way the move log owns its list, because the
 * records have nowhere else to live: the engine keeps a cube and a record of
 * moves, and a finished solve is neither.
 *
 * The panel is the whole of what it says. There is no accessor for the best
 * or for the recent list, because nothing but the drawing reads them -- the
 * one answer a caller wants back is whether the solve it just handed over is
 * the fastest, and that comes back from add().
 */
export class SessionRecords {
  private readonly bestLine: HTMLElement;
  private readonly list: HTMLElement;
  private readonly entries: SolveRecord[] = [];
  /**
   * The fastest solve of each cube, kept apart because they cannot be
   * compared: a 2x2 in twelve seconds is not better than a 5x5 in five
   * minutes, and one board holding both would say it was.
   */
  private readonly bestBySize = new Map<number, SolveRecord>();

  constructor(bestLine: HTMLElement, list: HTMLElement) {
    this.bestLine = bestLine;
    this.list = list;
    this.draw();
  }

  /**
   * Keeps one finished solve and redraws the panel.
   *
   * @returns whether it is the fastest so far, which the completion message
   *          says as part of itself rather than in a second announcement.
   */
  add(record: SolveRecord): boolean {
    this.entries.unshift(record);
    if (this.entries.length > RECENT_LIMIT) {
      this.entries.length = RECENT_LIMIT;
    }

    // Strictly faster, so an equal time leaves the earlier one standing: the
    // first to get there keeps it. Against the best of this cube, because
    // that is the only record this one is in the running for.
    const standing = this.bestBySize.get(record.cubeSize);
    const isBest =
      standing === undefined || record.elapsedMs < standing.elapsedMs;
    if (isBest) this.bestBySize.set(record.cubeSize, record);

    this.draw();
    return isBest;
  }

  private draw(): void {
    // The best of each cube, smallest first, on one line: there is one of
    // these for every size that has been solved, and a session rarely visits
    // more than a couple.
    const bests = [...this.bestBySize.entries()].sort(
      ([left], [right]) => left - right,
    );
    this.bestLine.textContent =
      bests.length === 0
        ? 'No solves yet.'
        : bests
            .map(
              ([size, record]) =>
                `Best ${size}×${size} ${formatElapsed(record.elapsedMs)}`,
            )
            .join(' · ');

    this.list.replaceChildren(
      ...this.entries.map((record) => {
        const item = document.createElement('li');
        item.className = 'records__item';
        item.textContent =
          `${formatElapsed(record.elapsedMs)} · ` +
          `${record.cubeSize}×${record.cubeSize} · ` +
          `${record.userMoveCount} moves · ` +
          `${record.scrambleLength}-move scramble`;
        return item;
      }),
    );
  }
}
