import { CUBE_SIZE } from '../wasm/CubeEngine.ts';
import { moveNotation } from './notation.ts';

/** What the log asks the engine, which is one question by index. */
export type MoveLogEngine = {
  timelineMove(index: number): number;
};

/**
 * The reading the log is drawn from: the same one every frame is measured by.
 *
 * Three numbers, and everything on the screen comes out of them. Where the
 * scramble stops is one of them rather than something the log remembers from
 * when a scramble was started, so a session restored from somewhere else draws
 * the same as one that has just been played.
 */
export type MoveLogFrame = {
  readonly cursor: number;
  readonly length: number;
  readonly scrambleEnd: number;
};

/**
 * The record, written out in standard notation.
 *
 * Owns one list element and nothing else. It holds no record of its own: the
 * engine's is the record, and this is a reading of it -- which is why undo,
 * redo, a scramble and a cut redo tail all arrive here as the same event, a
 * frame whose three numbers differ from the last one's.
 */
export class MoveLog {
  private readonly list: HTMLElement;
  private readonly engine: MoveLogEngine;

  /** What the list on the screen was drawn from, or null before anything was. */
  private drawn: MoveLogFrame | null = null;

  constructor(list: HTMLElement, engine: MoveLogEngine) {
    this.list = list;
    this.engine = engine;
  }

  /**
   * Redraws the list, if this frame differs from the one on the screen.
   *
   * Whole rather than in part, and on any of the three numbers changing. The
   * cheaper reading -- that only the cursor moved, so only the marks need
   * moving -- is wrong: a move made after an undo cuts the tail off and puts
   * one back in its place, which can leave the length exactly as it was with
   * different moves under it.
   *
   * A frame that says the same thing as the last one draws nothing, which is
   * what keeps a watched pattern and an orbit from rebuilding the list at
   * sixty frames a second.
   */
  update(frame: MoveLogFrame): void {
    if (
      this.drawn !== null &&
      this.drawn.cursor === frame.cursor &&
      this.drawn.length === frame.length &&
      this.drawn.scrambleEnd === frame.scrambleEnd
    ) {
      return;
    }
    this.drawn = frame;

    const items: HTMLLIElement[] = [];
    for (let index = 0; index < frame.length; index += 1) {
      items.push(this.item(index, frame));
    }
    this.list.replaceChildren(...items);

    // The cursor is the point of the list, so it is what stays on screen: a
    // hundred-move scramble is taller than the panel, and a rewind walking
    // down through it would otherwise leave the moving end out of sight.
    const current = items[frame.cursor - 1];
    if (current !== undefined) this.reveal(current);
  }

  /** One entry: what the move is written as, and where it stands. */
  private item(index: number, frame: MoveLogFrame): HTMLLIElement {
    const item = document.createElement('li');
    item.className = 'move-log__item';

    // Which stretch of the record this belongs to, derived rather than
    // remembered -- and derived the same way for a scramble half taken off
    // the cube, where the boundary sits above the cursor.
    item.dataset.part = index < frame.scrambleEnd ? 'scramble' : 'user';

    // Applied, the last of the applied, or waiting to be put back. The three
    // are one attribute because no entry is ever two of them.
    item.dataset.state =
      index < frame.cursor - 1
        ? 'applied'
        : index === frame.cursor - 1
          ? 'current'
          : 'pending';

    // Assigned rather than tested. A move a 3x3 cannot be written down has no
    // way of getting into the record -- every path that makes one turns a
    // single layer -- so a branch here would be for a case the cube cannot
    // reach, and the type is where that possibility is kept until a cube with
    // more layers gives it a notation of its own.
    item.textContent = moveNotation(this.engine.timelineMove(index), CUBE_SIZE);
    return item;
  }

  /** Scrolls the panel so one entry is in the middle of what is visible. */
  private reveal(item: HTMLLIElement): void {
    const middle =
      item.offsetTop - (this.list.clientHeight - item.offsetHeight) / 2;
    this.list.scrollTop = Math.max(0, middle);
  }
}
