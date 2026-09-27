import { describe, expect, it, vi } from 'vitest';

import { parseMoves } from '../../src/game/notation.ts';
import {
  attachCommandPalette,
  type Command,
  type KeyboardTarget,
} from '../../src/ui/CommandPalette.ts';
import type { CommandShell } from '../../src/ui/GameShell.ts';

/** A keyboard whose events the test delivers itself. */
function createKeyboard() {
  const listeners = new Set<(event: KeyboardEvent) => void>();
  const target: KeyboardTarget = {
    addEventListener: (_type, listener) => {
      listeners.add(listener);
    },
    removeEventListener: (_type, listener) => {
      listeners.delete(listener);
    },
  };
  return {
    target,
    listenerCount: (): number => listeners.size,
    press(init: KeyboardEventInit): KeyboardEvent {
      const event = new KeyboardEvent('keydown', { cancelable: true, ...init });
      for (const listener of [...listeners]) listener(event);
      return event;
    },
  };
}

function build(
  options: {
    canPlay?: boolean;
    blocked?: boolean;
    /** Further commands, after the ones every test has. */
    extra?: readonly Command[];
  } = {},
) {
  const host = document.createElement('div');
  host.innerHTML = `
    <button id="trigger" aria-expanded="false"><kbd id="shortcut"></kbd></button>
    <div id="backdrop" hidden></div>
    <div id="dialog" hidden>
      <input id="input">
      <p id="message"></p>
      <p class="command-group__label" id="label">Commands</p>
      <div id="list" role="listbox"></div>
    </div>
  `;
  document.body.replaceChildren(host);

  const shell: CommandShell = {
    trigger: host.querySelector<HTMLButtonElement>('#trigger')!,
    shortcut: host.querySelector<HTMLElement>('#shortcut')!,
    dialog: host.querySelector<HTMLElement>('#dialog')!,
    backdrop: host.querySelector<HTMLElement>('#backdrop')!,
    input: host.querySelector<HTMLInputElement>('#input')!,
    list: host.querySelector<HTMLElement>('#list')!,
    message: host.querySelector<HTMLElement>('#message')!,
  };

  let watching = false;
  const ran: string[] = [];
  const commands: Command[] = [
    {
      label: () => 'Scramble',
      keywords: 'shuffle',
      available: () => true,
      run: () => ran.push('scramble'),
    },
    {
      label: () => (watching ? 'Stop watching' : 'Watch the cube'),
      available: () => true,
      run: () => {
        watching = !watching;
        ran.push('watch');
      },
    },
    {
      label: () => 'Redo',
      available: () => false,
      run: () => ran.push('redo'),
    },
    {
      label: () => 'View: Split',
      available: () => true,
      note: () => 'Current',
      run: () => ran.push('split'),
    },
    ...(options.extra ?? []),
  ];

  const keyboard = createKeyboard();
  const play = vi.fn();
  const palette = attachCommandPalette({
    shell,
    commands: () => commands,
    parse: (text) => parseMoves(text, 3),
    canPlay: () => options.canPlay ?? true,
    play,
    blocked: () => options.blocked ?? false,
    mac: false,
    keyboardTarget: keyboard.target,
  });

  const options_ = (): HTMLElement[] => [
    ...shell.list.querySelectorAll<HTMLElement>('[role="option"]'),
  ];
  const type = (text: string): void => {
    shell.input.value = text;
    shell.input.dispatchEvent(new Event('input'));
  };
  const key = (keyName: string): KeyboardEvent => {
    const event = new KeyboardEvent('keydown', { key: keyName, cancelable: true });
    shell.input.dispatchEvent(event);
    return event;
  };

  return { shell, palette, keyboard, play, ran, options: options_, type, key };
}

describe('attachCommandPalette', () => {
  it('opens on Control-K, lists what can be run, and closes on Escape', () => {
    const { shell, palette, keyboard, options } = build();

    // The shortcut is written the way this platform presses it.
    expect(shell.shortcut.textContent).toBe('Ctrl K');

    const open = keyboard.press({ key: 'k', ctrlKey: true });
    expect(open.defaultPrevented).toBe(true);
    expect(palette.isOpen()).toBe(true);
    expect(shell.dialog.hidden).toBe(false);
    expect(document.activeElement).toBe(shell.input);

    // Redo cannot be pressed, so it is not offered.
    expect(options().map((option) => option.textContent)).toEqual([
      'Scramble',
      'Watch the cube',
      'View: SplitCurrent',
    ]);
    expect(options()[0]!.getAttribute('aria-selected')).toBe('true');
    expect(shell.input.getAttribute('aria-activedescendant')).toBe(
      options()[0]!.id,
    );

    keyboard.press({ key: 'Escape' });
    expect(palette.isOpen()).toBe(false);
    expect(shell.dialog.hidden).toBe(true);
  });

  it('finds a command by its words and runs it with Enter', () => {
    const { palette, ran, options, type, key } = build();
    palette.open();

    type('shuf');
    expect(options().map((option) => option.textContent)).toEqual(['Scramble']);
    key('Enter');

    expect(ran).toEqual(['scramble']);
    expect(palette.isOpen()).toBe(false);
  });

  it('walks the entries with the arrows, and wraps', () => {
    const { palette, ran, options, key } = build();
    palette.open();

    key('ArrowDown');
    expect(options()[1]!.getAttribute('aria-selected')).toBe('true');
    key('ArrowUp');
    key('ArrowUp');
    expect(options()[2]!.getAttribute('aria-selected')).toBe('true');

    key('Enter');
    expect(ran).toEqual(['split']);
  });

  it('offers to play a line that reads as moves, first', () => {
    const { palette, play, options, type, key } = build();
    palette.open();

    type("r u r' u'");
    const first = options()[0]!;
    expect(first.textContent).toBe("PlayR U R' U'4 moves");

    key('Enter');
    expect(play).toHaveBeenCalledWith("r u r' u'");
  });

  it('puts a command first for a word that is also a move', () => {
    const { palette, play, ran, options, type, key } = build({
      extra: [
        { label: () => 'View: 2D', available: () => true, run: () => ran.push('2d') },
        { label: () => 'Reset cube', available: () => true, run: () => ran.push('reset') },
      ],
    });
    palette.open();

    // "2d" is a slice of D as well as the name of a view; somebody typing it
    // into a search is after the view.
    type('2d');
    expect(options()[0]!.textContent).toBe('View: 2D');
    expect(options().some((option) => option.textContent?.startsWith('Play'))).toBe(
      true,
    );
    key('Enter');
    expect(ran).toEqual(['2d']);
    expect(play).not.toHaveBeenCalled();

    // The start of a command's name is a search too, however it reads.
    palette.open();
    type('res');
    expect(options()[0]!.textContent).toBe('Reset cube');

    // A spaced line, a primed one and a single letter are moves first, even
    // where every letter of them turns up in some command's name.
    for (const line of ['r u', "RUR'U'", 'R']) {
      type(line);
      expect(options()[0]!.textContent?.startsWith('Play')).toBe(true);
    }
  });

  it('says why a line of moves cannot be played', () => {
    const { shell, palette, options, type } = build();
    palette.open();

    type('4R');
    expect(options()).toHaveLength(0);
    expect(shell.message.textContent).toBe('"4R" reaches past a 3×3.');
    expect(shell.message.hidden).toBe(false);

    type('');
    expect(shell.message.hidden).toBe(true);
  });

  it('does not offer moves before there is a cube to play them on', () => {
    const { shell, palette, options, type } = build({ canPlay: false });
    palette.open();

    // Nothing but moves in it, so no command matches either.
    type('F2 B2');
    expect(options()).toHaveLength(0);
    expect(shell.message.textContent).toBe('The cube is not ready for moves yet.');
  });

  it('keeps the focus in its field when the rest of it is pressed', () => {
    const { shell, keyboard } = build();
    keyboard.press({ key: 'k', ctrlKey: true });

    // A heading or the space round the list would otherwise take the focus to
    // the page behind, where Tab reaches what the menu covers.
    const press = (target: Element): MouseEvent => {
      const event = new MouseEvent('mousedown', { bubbles: true, cancelable: true });
      target.dispatchEvent(event);
      return event;
    };
    expect(press(shell.dialog.querySelector('#label')!).defaultPrevented).toBe(
      true,
    );
    expect(press(shell.message).defaultPrevented).toBe(true);
    // The field itself is still pressed as a field, to place the caret.
    expect(press(shell.input).defaultPrevented).toBe(false);
  });

  it('does not open over another dialog', () => {
    const { palette, keyboard } = build({ blocked: true });

    keyboard.press({ key: 'k', metaKey: true });
    expect(palette.isOpen()).toBe(false);
  });

  it('reads a toggle each time it opens, and stops answering at teardown', () => {
    const { palette, options, keyboard, type, key } = build();
    palette.open();
    type('watch');
    key('Enter');

    palette.open();
    type('watch');
    expect(options()[0]!.textContent).toBe('Stop watching');
    palette.close();

    palette.teardown();
    expect(keyboard.listenerCount()).toBe(0);
    keyboard.press({ key: 'k', ctrlKey: true });
    expect(palette.isOpen()).toBe(false);
  });
});
