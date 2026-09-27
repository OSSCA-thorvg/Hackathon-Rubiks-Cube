import type { TypedMoves } from '../game/notation.ts';
import type { CommandShell } from './GameShell.ts';

/**
 * One thing the menu can do.
 *
 * Every command in the page's own menu is a press on a control that is
 * already on the page, which is what keeps the menu honest: whether a command
 * is offered is whether its button could be pressed right now, and what it
 * does is whatever that button does -- the controller that owns the button
 * never learns there is a menu.
 */
export type Command = {
  /** What the entry says. Read each time, for toggles that change wording. */
  readonly label: () => string;
  /** Other words a person might search for it by. */
  readonly keywords?: string;
  /** Path data for a 24-unit stroke icon shown before the label, if any. */
  readonly icon?: string;
  /** Whether it can be run right now; an entry that cannot is not listed. */
  readonly available: () => boolean;
  /** A word said at the end of the entry, such as the choice already made. */
  readonly note?: () => string | null;
  readonly run: () => void;
};

/** Minimal keyboard event target, the same shape the other controllers take. */
export type KeyboardTarget = {
  addEventListener(type: 'keydown', listener: (event: KeyboardEvent) => void): void;
  removeEventListener(type: 'keydown', listener: (event: KeyboardEvent) => void): void;
};

export type CommandPaletteOptions = {
  readonly shell: CommandShell;
  /** Asked for each time the list is drawn, so an entry's wording is never stale. */
  readonly commands: () => readonly Command[];
  /** Reads a line as moves for the cube in hand. */
  readonly parse: (text: string) => TypedMoves;
  /** Whether a line of moves could be played right now. */
  readonly canPlay: () => boolean;
  /** Plays a line of moves the parser accepted. */
  readonly play: (text: string) => void;
  /** Whether another dialog is up, which the menu does not open over. */
  readonly blocked?: () => boolean;
  /** Whether this is a Mac, which decides the modifier the trigger shows. */
  readonly mac?: boolean;
  /** Where key events are heard; the window in the page. */
  readonly keyboardTarget?: KeyboardTarget;
  /** Path data for the play icon, on the entry that plays typed moves. */
  readonly playIcon?: string;
};

export type CommandPalette = {
  isOpen(): boolean;
  open(): void;
  close(options?: { readonly restoreFocus?: boolean }): void;
  teardown(): void;
};

/** One entry as drawn: what it runs, and whether it plays moves. */
type Entry = {
  readonly id: string;
  readonly element: HTMLElement;
  readonly run: () => void;
};

/**
 * A line that looks like moves rather than words, so a reason it could not be
 * read is worth saying even when a command matches it too.
 */
const MOVE_LIKE = /^[\s,RLUDFBMESXYZrludfbmesxyzwW'’′`\d-]+$/;

/**
 * Whether a line that reads as moves is more likely a search for a command.
 *
 * Moves are read in either case, so short words are moves too: "2d" is a
 * slice of D, "res" is R E S, "mu" is M U. A single run of two or more
 * characters that begins a word in the name of a command it found is taken
 * as that search, and the command goes above the moves. Anything spaced or
 * primed -- "R U R' U'", "RUR'U'" -- and any single letter stays a line of
 * moves first. Both are listed either way, an arrow key apart.
 */
function readsAsSearch(query: string, found: readonly string[]): boolean {
  const token = query.toLowerCase();
  if (token.length < 2 || /[\s,'’′`]/.test(token)) return false;
  return found.some((haystack) =>
    haystack.split(/[^a-z0-9]+/).some((word) => word.startsWith(token)),
  );
}

/**
 * The command menu: one field that searches the page's commands and plays
 * typed moves.
 *
 * A combobox over a listbox, with the field keeping focus throughout: the
 * arrows move an active entry that the field points at, Enter runs it, and
 * Escape or a press outside closes. Unlike the detail panels this is a
 * question the page is asking, and it goes away once answered.
 */
export function attachCommandPalette(
  options: CommandPaletteOptions,
): CommandPalette {
  const { shell } = options;
  const { trigger, dialog, backdrop, input, list, message } = shell;
  const keyboard: KeyboardTarget = options.keyboardTarget ?? {
    addEventListener: (_type, listener): void => {
      window.addEventListener('keydown', listener);
    },
    removeEventListener: (_type, listener): void => {
      window.removeEventListener('keydown', listener);
    },
  };

  shell.shortcut.textContent = options.mac === false ? 'Ctrl K' : '⌘K';

  let open = false;
  let entries: Entry[] = [];
  let active = 0;
  /** Where focus was before the menu opened, and goes back to. */
  let returnTo: HTMLElement | null = null;

  const setActive = (index: number): void => {
    if (entries.length === 0) {
      input.removeAttribute('aria-activedescendant');
      return;
    }
    active = (index + entries.length) % entries.length;
    entries.forEach((entry, at) => {
      entry.element.setAttribute('aria-selected', String(at === active));
    });
    const current = entries[active]!;
    input.setAttribute('aria-activedescendant', current.id);
    current.element.scrollIntoView({ block: 'nearest' });
  };

  const option = (
    id: string,
    parts: readonly (Node | string)[],
    run: () => void,
  ): Entry => {
    const element = document.createElement('div');
    element.className = 'command';
    element.id = id;
    element.setAttribute('role', 'option');
    element.setAttribute('aria-selected', 'false');
    element.append(...parts);
    return { id, element, run };
  };

  const text = (className: string, content: string): HTMLSpanElement => {
    const span = document.createElement('span');
    span.className = className;
    span.textContent = content;
    return span;
  };

  const iconOf = (paths: string | undefined): HTMLSpanElement => {
    const holder = document.createElement('span');
    holder.className = 'command__icon';
    holder.setAttribute('aria-hidden', 'true');
    if (paths !== undefined) {
      holder.innerHTML = `<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" focusable="false">${paths}</svg>`;
    }
    return holder;
  };

  const group = (label: string, members: readonly Entry[]): HTMLElement => {
    const element = document.createElement('div');
    element.className = 'command-group';
    element.setAttribute('role', 'group');
    const heading = document.createElement('div');
    heading.className = 'command-group__label';
    heading.id = `command-group-${label.toLowerCase()}`;
    heading.setAttribute('role', 'presentation');
    heading.textContent = label;
    element.setAttribute('aria-labelledby', heading.id);
    element.append(heading, ...members.map((entry) => entry.element));
    return element;
  };

  /** Rebuilds the list for what is in the field. */
  const render = (): void => {
    const query = input.value.trim();
    const words = query.toLowerCase().split(/\s+/).filter(Boolean);
    let reason = '';

    const moves: Entry[] = [];
    if (query !== '') {
      const typed = options.parse(query);
      if (typed.ok && typed.turns.length > 0) {
        if (options.canPlay()) {
          const count = typed.turns.length;
          moves.push(
            option(
              'command-play',
              [
                iconOf(options.playIcon),
                text('command__label', 'Play'),
                text('command__moves', typed.written.join(' ')),
                text('command__note', `${count} ${count === 1 ? 'move' : 'moves'}`),
              ],
              () => options.play(query),
            ),
          );
        } else {
          reason = 'The cube is not ready for moves yet.';
        }
      } else if (!typed.ok && MOVE_LIKE.test(query)) {
        reason = typed.reason;
      }
    }

    const commands: Entry[] = [];
    const found: string[] = [];
    options.commands().forEach((command, index) => {
      if (!command.available()) return;
      const label = command.label();
      const haystack = `${label} ${command.keywords ?? ''}`.toLowerCase();
      if (!words.every((word) => haystack.includes(word))) return;
      found.push(haystack);

      const note = command.note?.() ?? null;
      commands.push(
        option(
          `command-${index}`,
          [
            iconOf(command.icon),
            text('command__label', label),
            ...(note === null ? [] : [text('command__note', note)]),
          ],
          command.run,
        ),
      );
    });

    if (reason === '' && moves.length === 0 && commands.length === 0) {
      reason =
        query === ''
          ? ''
          : `Nothing matches "${query}". Moves are written like R U' F2 or 2Rw.`;
    }

    const groups = [
      { name: 'Moves', members: moves },
      { name: 'Commands', members: commands },
    ].filter(({ members }) => members.length > 0);
    if (moves.length > 0 && readsAsSearch(query, found)) groups.reverse();

    entries = groups.flatMap(({ members }) => members);
    list.replaceChildren(
      ...groups.map(({ name, members }) => group(name, members)),
    );
    message.textContent = reason;
    message.hidden = reason === '';
    setActive(0);
  };

  const closeMenu = (settings: { readonly restoreFocus?: boolean } = {}): void => {
    if (!open) return;
    open = false;

    dialog.hidden = true;
    backdrop.hidden = true;
    trigger.setAttribute('aria-expanded', 'false');

    if (settings.restoreFocus !== false) {
      (returnTo?.isConnected === true ? returnTo : trigger).focus();
    }
    returnTo = null;
  };

  const openMenu = (): void => {
    if (open || options.blocked?.() === true) return;
    open = true;

    // A press that did not focus the trigger -- Safari's buttons do not take
    // focus on a click -- leaves the body active, which is nowhere to go back
    // to; the trigger is where the menu came from.
    const from = document.activeElement;
    returnTo =
      from instanceof HTMLElement && from !== document.body ? from : trigger;
    input.value = '';
    backdrop.hidden = false;
    dialog.hidden = false;
    trigger.setAttribute('aria-expanded', 'true');
    render();
    input.focus();
  };

  /**
   * Runs one entry after the menu is gone, so whatever it opens -- the
   * settings drawer, most of all -- is not opened underneath it, and focus
   * lands where the command sends it rather than back on the trigger.
   */
  const runEntry = (entry: Entry): void => {
    const target = returnTo ?? trigger;
    closeMenu({ restoreFocus: false });
    entry.run();
    // A command that did not move focus anywhere leaves it where the menu
    // was opened from, which is where a keyboard expects to be -- or on the
    // trigger, when what it was opened from has since been redrawn away.
    const now = document.activeElement;
    if (now === null || now === document.body) {
      (target.isConnected ? target : trigger).focus();
    }
  };

  const onInput = (): void => render();

  const onFieldKeyDown = (event: KeyboardEvent): void => {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault();
      setActive(active + (event.key === 'ArrowDown' ? 1 : -1));
      return;
    }
    if (event.key === 'Enter') {
      event.preventDefault();
      const entry = entries[active];
      if (entry !== undefined) runEntry(entry);
      return;
    }
    // Focus stays in the field; the list is walked with the arrows.
    if (event.key === 'Tab') event.preventDefault();
  };

  const onListClick = (event: Event): void => {
    const element = (event.target as Element | null)?.closest?.('[role="option"]');
    const entry = entries.find((candidate) => candidate.element === element);
    if (entry !== undefined) runEntry(entry);
  };

  const onListPointerMove = (event: Event): void => {
    const element = (event.target as Element | null)?.closest?.('[role="option"]');
    const at = entries.findIndex((candidate) => candidate.element === element);
    if (at !== -1 && at !== active) setActive(at);
  };

  /** Command-K or Control-K from anywhere, and Escape while open. */
  const onKeyDown = (event: KeyboardEvent): void => {
    if (
      event.key.toLowerCase() === 'k' &&
      (event.metaKey || event.ctrlKey) &&
      !event.altKey &&
      !event.shiftKey
    ) {
      event.preventDefault();
      if (open) closeMenu();
      else openMenu();
      return;
    }
    if (open && event.key === 'Escape') {
      event.preventDefault();
      closeMenu();
    }
  };

  const onTriggerClick = (): void => openMenu();
  const onBackdropPointerDown = (): void => closeMenu();

  // The field keeps the focus, so a press anywhere else in the menu -- a
  // group's heading, the message, the space around the list -- must not take
  // it. Left to the browser, it goes to the page behind, where Tab reaches
  // the controls the menu is covering. `mousedown` rather than `pointerdown`,
  // because only the first moves the focus; the entries still get their
  // click.
  const onDialogMouseDown = (event: MouseEvent): void => {
    if (event.target !== input) event.preventDefault();
  };

  trigger.addEventListener('click', onTriggerClick);
  backdrop.addEventListener('pointerdown', onBackdropPointerDown);
  dialog.addEventListener('mousedown', onDialogMouseDown);
  input.addEventListener('input', onInput);
  input.addEventListener('keydown', onFieldKeyDown);
  list.addEventListener('click', onListClick);
  list.addEventListener('pointermove', onListPointerMove);
  keyboard.addEventListener('keydown', onKeyDown);

  return {
    isOpen: (): boolean => open,
    open: openMenu,
    close: closeMenu,
    teardown: (): void => {
      closeMenu({ restoreFocus: false });
      trigger.removeEventListener('click', onTriggerClick);
      backdrop.removeEventListener('pointerdown', onBackdropPointerDown);
      dialog.removeEventListener('mousedown', onDialogMouseDown);
      input.removeEventListener('input', onInput);
      input.removeEventListener('keydown', onFieldKeyDown);
      list.removeEventListener('click', onListClick);
      list.removeEventListener('pointermove', onListPointerMove);
      keyboard.removeEventListener('keydown', onKeyDown);
    },
  };
}
