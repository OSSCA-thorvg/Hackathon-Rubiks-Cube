import { describe, expect, it } from 'vitest';

import { attachDetailPanels } from '../../src/ui/DetailPanels.ts';

/** The three panels and their toggles, as the shell writes them. */
function build(options: { narrow?: boolean } = {}) {
  const root = document.createElement('main');
  root.innerHTML = `
    <button data-panel="moves" aria-controls="panel-moves" aria-expanded="false"></button>
    <button data-panel="session" aria-controls="panel-session" aria-expanded="false"></button>
    <button data-panel="turn" aria-controls="panel-turn" aria-expanded="false"></button>
    <section id="panel-moves" hidden></section>
    <section id="panel-session" hidden></section>
    <section id="panel-turn" hidden></section>
  `;
  document.body.replaceChildren(root);

  let narrow = options.narrow ?? false;
  const listeners = new Set<() => void>();
  const toggles = [...root.querySelectorAll<HTMLButtonElement>('[data-panel]')];
  const panels = attachDetailPanels({
    toggles,
    root,
    oneAtATime: () => narrow,
    watchWidth: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
  });

  const [moves, session, turn] = toggles as [
    HTMLButtonElement,
    HTMLButtonElement,
    HTMLButtonElement,
  ];
  return {
    root,
    panels,
    moves,
    session,
    turn,
    section: (name: string): HTMLElement => root.querySelector(`#panel-${name}`)!,
    narrow(value: boolean): void {
      narrow = value;
      for (const listener of [...listeners]) listener();
    },
    listenerCount: (): number => listeners.size,
  };
}

describe('attachDetailPanels', () => {
  it('opens a panel with its button and keeps it until the same button', () => {
    const { root, moves, section } = build();

    expect(root.dataset.panels).toBe('closed');
    moves.click();
    expect(section('moves').hidden).toBe(false);
    expect(moves.getAttribute('aria-expanded')).toBe('true');
    expect(root.dataset.panels).toBe('open');

    // Nothing else puts it away: a press on the page, a key, another click
    // anywhere but its own button.
    document.body.click();
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(section('moves').hidden).toBe(false);

    moves.click();
    expect(section('moves').hidden).toBe(true);
    expect(moves.getAttribute('aria-expanded')).toBe('false');
    expect(root.dataset.panels).toBe('closed');
  });

  it('keeps any number open side by side on a wide screen', () => {
    const { panels, moves, session, turn } = build();

    moves.click();
    session.click();
    turn.click();
    expect(panels.isOpen('moves')).toBe(true);
    expect(panels.isOpen('session')).toBe(true);
    expect(panels.isOpen('turn')).toBe(true);

    session.click();
    expect(panels.isOpen('session')).toBe(false);
    expect(panels.isOpen('moves')).toBe(true);
  });

  it('keeps one at a time on a narrow screen', () => {
    const { panels, moves, session } = build({ narrow: true });

    moves.click();
    session.click();
    expect(panels.isOpen('moves')).toBe(false);
    expect(panels.isOpen('session')).toBe(true);
    expect(moves.getAttribute('aria-expanded')).toBe('false');
  });

  it('keeps the one opened last when the window narrows', () => {
    const { panels, moves, session, turn, narrow } = build();

    turn.click();
    moves.click();
    session.click();
    narrow(true);

    expect(panels.isOpen('session')).toBe(true);
    expect(panels.isOpen('moves')).toBe(false);
    expect(panels.isOpen('turn')).toBe(false);
  });

  it('stops answering at teardown', () => {
    const { panels, moves, listenerCount } = build();

    panels.teardown();
    moves.click();
    expect(panels.isOpen('moves')).toBe(false);
    expect(listenerCount()).toBe(0);
  });
});
