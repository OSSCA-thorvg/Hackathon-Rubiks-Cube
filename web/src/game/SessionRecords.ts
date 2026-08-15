import { formatElapsed } from './SolveTimer.ts';

/** One finished solve, as it is worth remembering. */
export type SolveRecord = {
  readonly elapsedMs: number;
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
 */
export class SessionRecords {
  private readonly bestLine: HTMLElement;
  private readonly list: HTMLElement;
  private readonly entries: SolveRecord[] = [];
  private bestRecord: SolveRecord | null = null;

  constructor(bestLine: HTMLElement, list: HTMLElement) {
    this.bestLine = bestLine;
    this.list = list;
    this.draw();
  }

  /** The fastest solve so far, or null before there is one. */
  get best(): SolveRecord | null {
    return this.bestRecord;
  }

  /** The latest solves, newest first, at most RECENT_LIMIT of them. */
  get recent(): readonly SolveRecord[] {
    return this.entries;
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
    // first to get there keeps it.
    const isBest =
      this.bestRecord === null || record.elapsedMs < this.bestRecord.elapsedMs;
    if (isBest) this.bestRecord = record;

    this.draw();
    return isBest;
  }

  private draw(): void {
    this.bestLine.textContent =
      this.bestRecord === null
        ? 'No solves yet.'
        : `Best ${formatElapsed(this.bestRecord.elapsedMs)}`;

    this.list.replaceChildren(
      ...this.entries.map((record) => {
        const item = document.createElement('li');
        item.className = 'records__item';
        item.textContent =
          `${formatElapsed(record.elapsedMs)} · ` +
          `${record.userMoveCount} moves · ` +
          `${record.scrambleLength}-move scramble`;
        return item;
      }),
    );
  }
}
