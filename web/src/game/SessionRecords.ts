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
  private readonly tally: HTMLElement | null;
  private readonly entries: SolveRecord[] = [];
  /** Every solve of the sitting, including the ones the list has let go of. */
  private solved = 0;
  /**
   * The fastest solve of each cube, kept apart because they cannot be
   * compared: a 2x2 in twelve seconds is not better than a 5x5 in five
   * minutes, and one board holding both would say it was.
   */
  private readonly bestBySize = new Map<number, SolveRecord>();

  /**
   * @param tally where the count of the sitting's solves is written, beside
   *              the panel's title; absent on a page without one.
   */
  constructor(bestLine: HTMLElement, list: HTMLElement, tally?: HTMLElement) {
    this.bestLine = bestLine;
    this.list = list;
    this.tally = tally ?? null;
    this.draw();
  }

  /**
   * Keeps one finished solve and redraws the panel.
   *
   * @returns whether it is the fastest so far, which the completion message
   *          says as part of itself rather than in a second announcement.
   */
  add(record: SolveRecord): boolean {
    this.solved += 1;
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
    if (bests.length === 0) {
      this.bestLine.textContent = 'No solves yet.';
    } else {
      // In parts, so the time can be drawn large beside its label, and with
      // the words between them kept as text: what the line says, read or
      // heard, is the same sentence either way.
      const parts: (Node | string)[] = [];
      for (const [size, record] of bests) {
        if (parts.length > 0) parts.push(' · ');
        const entry = document.createElement('span');
        entry.className = 'records__best-entry';
        entry.append(
          span('records__best-label', `Best ${size}×${size}`),
          ' ',
          span('records__best-time', formatElapsed(record.elapsedMs)),
        );
        parts.push(entry);
      }
      this.bestLine.replaceChildren(...parts);
    }

    this.list.replaceChildren(
      ...this.entries.map((record) => {
        const item = document.createElement('li');
        item.className = 'records__item';
        // Marked rather than labelled: the stylesheet draws the tag, so the
        // entry still reads as the one sentence it has always been.
        if (this.bestBySize.get(record.cubeSize) === record) {
          item.dataset.best = 'true';
        }
        // The separators are read and not drawn, and so is the scramble
        // length, which the panel has no room for.
        item.append(
          span('records__time', formatElapsed(record.elapsedMs)),
          span('visually-hidden', ' · '),
          span('records__size', `${record.cubeSize}×${record.cubeSize}`),
          span('visually-hidden', ' · '),
          span('records__moves', `${record.userMoveCount} moves`),
          span('visually-hidden', ' · '),
          span(
            'records__scramble visually-hidden',
            `${record.scrambleLength}-move scramble`,
          ),
        );
        return item;
      }),
    );

    if (this.tally !== null) {
      this.tally.textContent =
        this.solved === 0
          ? 'No solves'
          : `${this.solved} ${this.solved === 1 ? 'solve' : 'solves'}`;
    }
  }
}

/** One run of text with a class, for the parts the stylesheet sets apart. */
function span(className: string, text: string): HTMLSpanElement {
  const element = document.createElement('span');
  element.className = className;
  element.textContent = text;
  return element;
}
