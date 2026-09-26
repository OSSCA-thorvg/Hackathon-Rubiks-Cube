import { faceOfNotation, moveNotation } from './notation.ts';

/** What the log asks the engine: one question by index, and which cube. */
export type MoveLogEngine = {
  timelineMove(index: number): number;
  /** Read at every entry rather than remembered: the cube can be replaced. */
  cubeSize(): number;
};

/**
 * The reading the log is drawn from: the same one every frame is measured by.
 *
 * Three numbers, and everything on the screen comes out of them. Where the
 * scramble stops is one of them rather than something the log remembers from
 * when a scramble was started, so a session restored from somewhere else draws
 * the same as one that has just been played -- and so the moves that are the
 * user's own are a stretch of the record rather than a list kept apart.
 */
export type MoveLogFrame = {
  readonly cursor: number;
  readonly length: number;
  readonly scrambleEnd: number;
};

export type MoveLogOptions = {
  /**
   * Told the record index of a move somebody pressed.
   *
   * Absent, the entries are words and nothing else. Present, each one is a
   * button, because a move on the list is somewhere the cube can be taken
   * back or forward to -- and what doing that means is the caller's.
   */
  readonly onPick?: (index: number) => void;
};

/**
 * The moves you made, written out in standard notation.
 *
 * Owns one list element and nothing else. It holds no record of its own: the
 * engine's is the record, and this is a reading of it -- which is why undo,
 * redo, a scramble and a cut redo tail all arrive here as the same event, a
 * frame whose three numbers differ from the last one's.
 *
 * The scramble is not written out here. It is the cube somebody was handed
 * rather than anything they did, and reading twenty moves of it to find your
 * own two is worse than not having them. Nothing is filtered out to leave it
 * off: the record is one list with the scramble at the front of it, so the
 * moves that are yours are the stretch above the boundary, and that is where
 * the drawing starts.
 *
 * Each entry carries the colour of the face its letter names, so a run of
 * moves can be read at a glance by which sides of the cube it works on.
 */
export class MoveLog {
  private readonly list: HTMLElement;
  private readonly engine: MoveLogEngine;
  private readonly onPick: ((index: number) => void) | null;

  /** What the list on the screen was drawn from, or null before anything was. */
  private drawn: MoveLogFrame | null = null;

  constructor(
    list: HTMLElement,
    engine: MoveLogEngine,
    options: MoveLogOptions = {},
  ) {
    this.list = list;
    this.engine = engine;
    this.onPick = options.onPick ?? null;

    if (this.onPick !== null) {
      list.addEventListener('click', this.onClick);
      list.addEventListener('keydown', this.onKeyDown);
    }
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

    // A press on an entry is often what started the frames that redraw it, so
    // the entry holding focus is about to be replaced. Which one it was is
    // kept by its index and handed to the new one below, or the keyboard
    // would drop back to the top of the page at every step of the walk.
    const focusedIndex = this.focusedIndex();

    const items: HTMLLIElement[] = [];
    for (let index = frame.scrambleEnd; index < frame.length; index += 1) {
      items.push(this.item(index, frame));
    }
    this.list.replaceChildren(...items);

    // The cursor is the point of the list, so it is what stays on screen: a
    // long solve runs past the end of the panel, and a rewind walking down
    // through it would otherwise leave the moving end out of sight. A cursor
    // that has been rewound into the scramble is below everything drawn here,
    // and lands outside the list rather than on its first entry.
    const current = items[frame.cursor - 1 - frame.scrambleEnd];
    if (current !== undefined) this.reveal(current);

    if (this.onPick === null) return;

    // One stop for the whole list, so tabbing past a long solve is one key
    // rather than one per move; the arrows walk the rest. It is on the entry
    // the cube is at -- unless the keyboard is already in the list, when it
    // stays on the entry holding focus. A stop anywhere else would be one
    // more place for Tab to land before it left.
    const buttons = this.buttons();
    const atCursor = (current ?? items[0])?.querySelector('button') ?? null;
    const focused =
      focusedIndex === null
        ? null
        : (buttons.find(
            (button) => Number(button.dataset.index) === focusedIndex,
          ) ?? atCursor);
    const stop = focused ?? atCursor;
    for (const button of buttons) button.tabIndex = button === stop ? 0 : -1;

    focused?.focus({ preventScroll: true });
  }

  /** Stops listening; the list itself is the caller's. */
  teardown(): void {
    this.list.removeEventListener('click', this.onClick);
    this.list.removeEventListener('keydown', this.onKeyDown);
  }

  /** One entry: what the move is written as, and where it stands. */
  private item(index: number, frame: MoveLogFrame): HTMLLIElement {
    const item = document.createElement('li');
    item.className = 'move-log__item';

    // Applied, the last of the applied, or waiting to be put back. The three
    // are one attribute because no entry is ever two of them.
    const state =
      index < frame.cursor - 1
        ? 'applied'
        : index === frame.cursor - 1
          ? 'current'
          : 'pending';
    item.dataset.state = state;

    // Assigned rather than tested. A move with no notation has no way of
    // getting into the record: turning, the commands and a shared link are all
    // held to the masks this writes, so a branch here would be for a case the
    // cube cannot reach, and the type is where that possibility is kept.
    const written =
      moveNotation(this.engine.timelineMove(index), this.engine.cubeSize()) ??
      '';

    // The swatch is decoration for the letter beside it, so it carries no
    // text: an entry reads, and is read aloud, as the move and nothing else.
    const dot = document.createElement('span');
    dot.className = 'move-dot';
    dot.dataset.dot = (faceOfNotation(written) ?? 'slice').toLowerCase();
    dot.setAttribute('aria-hidden', 'true');

    if (this.onPick === null) {
      item.append(dot, written);
      return item;
    }

    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'move-chip';
    button.dataset.index = String(index);
    const ordinal = index - frame.scrambleEnd + 1;
    button.setAttribute(
      'aria-label',
      `Move ${ordinal}, ${written}${state === 'pending' ? ', taken back' : ''}`,
    );
    if (state === 'current') button.setAttribute('aria-current', 'step');
    button.append(dot, written);
    item.append(button);
    return item;
  }

  private buttons(): HTMLButtonElement[] {
    return [...this.list.querySelectorAll<HTMLButtonElement>('button[data-index]')];
  }

  /** The record index of the entry holding focus, or null for none. */
  private focusedIndex(): number | null {
    const active = document.activeElement;
    if (!(active instanceof HTMLButtonElement)) return null;
    if (!this.list.contains(active)) return null;
    const index = Number(active.dataset.index);
    return Number.isInteger(index) ? index : null;
  }

  private readonly onClick = (event: Event): void => {
    const button = (event.target as Element | null)?.closest?.(
      'button[data-index]',
    );
    if (!(button instanceof HTMLButtonElement)) return;
    this.onPick?.(Number(button.dataset.index));
  };

  /** The arrows and Home and End walk the entries; Tab leaves the list. */
  private readonly onKeyDown = (event: KeyboardEvent): void => {
    const buttons = this.buttons();
    const at = buttons.indexOf(event.target as HTMLButtonElement);
    if (at === -1) return;

    const next =
      event.key === 'ArrowLeft' || event.key === 'ArrowUp'
        ? at - 1
        : event.key === 'ArrowRight' || event.key === 'ArrowDown'
          ? at + 1
          : event.key === 'Home'
            ? 0
            : event.key === 'End'
              ? buttons.length - 1
              : null;
    if (next === null) return;

    event.preventDefault();
    const target = buttons[Math.max(0, Math.min(buttons.length - 1, next))];
    if (target === undefined) return;
    for (const button of buttons) button.tabIndex = button === target ? 0 : -1;
    target.focus();
    this.reveal(target.closest('li') ?? target);
  };

  /**
   * Scrolls the list so one entry is in the middle of what is visible.
   *
   * Along both axes, because the same list is a column in one place and a
   * single row in another, and whichever way it does not overflow is left
   * where it is. Measured against the list itself, which the stylesheet
   * positions so that it is the entries' offset parent.
   */
  private reveal(item: HTMLElement): void {
    const top = item.offsetTop - (this.list.clientHeight - item.offsetHeight) / 2;
    const left = item.offsetLeft - (this.list.clientWidth - item.offsetWidth) / 2;
    this.list.scrollTop = Math.max(0, top);
    this.list.scrollLeft = Math.max(0, left);
  }
}
