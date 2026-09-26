import { MoveLog, type MoveLogEngine, type MoveLogFrame } from './MoveLog.ts';
import { moveNotation } from './notation.ts';

/** The elements the timeline writes to. */
export type TimelineUi = {
  /** The row of moves under the stage, which scrolls to keep the cursor in it. */
  readonly strip: HTMLElement;
  /**
   * The bar under the row. Two custom properties place everything on it --
   * `--scramble` and `--cursor`, as shares of the whole record -- so the
   * drawing of it stays in the stylesheet.
   */
  readonly track: HTMLElement;
  /** How long the scramble is, at the bar's start. */
  readonly scrambleLabel: HTMLElement;
  /** How far into your own moves the cube is, at the bar's end. */
  readonly progressLabel: HTMLElement;
  /** The scramble written out in full, which only the Moves panel has room for. */
  readonly scrambleText: HTMLElement;
  /** The panel's own copy of the progress, beside its title. */
  readonly panelProgress: HTMLElement;
};

/**
 * The record as a line you can see the whole of, and step along.
 *
 * The move log is the record written out; this is the same record laid flat:
 * the scramble first, as the stretch you were handed, then your own moves,
 * with the cube's place in it marked. It reads the same three numbers the log
 * does, so a rewind walking back through the scramble shows here as the mark
 * walking back across it -- which the log, starting where the scramble ends,
 * has no way to show.
 *
 * Nothing here commands the engine. A press on a move is handed to the
 * caller with the move's record index, the same way the log hands one over.
 */
export class Timeline {
  private readonly ui: TimelineUi;
  private readonly engine: MoveLogEngine;
  private readonly strip: MoveLog;

  /** What was last drawn, or null before anything was. */
  private drawn: MoveLogFrame | null = null;

  constructor(
    ui: TimelineUi,
    engine: MoveLogEngine,
    onPick: (index: number) => void,
  ) {
    this.ui = ui;
    this.engine = engine;
    this.strip = new MoveLog(ui.strip, engine, { onPick });
  }

  /** Redraws what the frame changed, and nothing for a frame that changed nothing. */
  update(frame: MoveLogFrame): void {
    this.strip.update(frame);

    if (
      this.drawn !== null &&
      this.drawn.cursor === frame.cursor &&
      this.drawn.length === frame.length &&
      this.drawn.scrambleEnd === frame.scrambleEnd
    ) {
      return;
    }
    this.drawn = frame;

    // Shares of the whole record, so the scramble's part of the bar is as
    // long as the scramble is against everything done since. An empty record
    // has no shares; both marks sit at the start.
    const share = (count: number): string =>
      frame.length === 0 ? '0%' : `${(count / frame.length) * 100}%`;
    this.ui.track.style.setProperty('--scramble', share(frame.scrambleEnd));
    this.ui.track.style.setProperty('--cursor', share(frame.cursor));

    this.ui.scrambleLabel.textContent =
      frame.scrambleEnd > 0 ? `Scramble · ${frame.scrambleEnd}` : 'No scramble';

    // Your own moves only: a cursor rewound into the scramble has none of
    // them on the cube, which is nothing rather than a negative number.
    const own = frame.length - frame.scrambleEnd;
    const done = Math.max(0, frame.cursor - frame.scrambleEnd);
    const progress = `${done} / ${own}`;
    this.ui.progressLabel.textContent = progress;
    this.ui.panelProgress.textContent = progress;

    // Written out again whenever the record moved, rather than only when a
    // scramble was started: the stretch below the boundary is read off the
    // record like everything else, so a shared link and a solve that pulled
    // the boundary down are drawn by the same line as a fresh scramble.
    const size = this.engine.cubeSize();
    const words: string[] = [];
    for (let index = 0; index < frame.scrambleEnd; index += 1) {
      words.push(moveNotation(this.engine.timelineMove(index), size) ?? '');
    }
    this.ui.scrambleText.textContent = words.join(' ');
  }

  teardown(): void {
    this.strip.teardown();
  }
}
