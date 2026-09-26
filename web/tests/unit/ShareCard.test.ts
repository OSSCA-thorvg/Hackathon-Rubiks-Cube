import { describe, expect, it, vi } from 'vitest';

import { ShareCard, type CardTimers } from '../../src/game/ShareCard.ts';

/** Timers the test runs by hand, so going away on its own can be watched. */
function createTimers() {
  let next = 1;
  const pending = new Map<number, () => void>();
  const timers: CardTimers = {
    setTimeout: (callback) => {
      const handle = next;
      next += 1;
      pending.set(handle, callback);
      return handle;
    },
    clearTimeout: (handle) => {
      pending.delete(handle);
    },
  };
  return {
    timers,
    pendingCount: (): number => pending.size,
    runAll(): void {
      for (const [handle, callback] of [...pending]) {
        pending.delete(handle);
        callback();
      }
    },
  };
}

function build(options: { copyFails?: boolean } = {}) {
  const host = document.createElement('div');
  host.innerHTML = `
    <button id="share">Share</button>
    <section id="card" hidden>
      <h2 id="title"></h2>
      <button id="close" type="button">Close</button>
      <p id="note"></p>
      <input id="link" readonly>
      <button id="copy" type="button">Copy</button>
    </section>
  `;
  document.body.replaceChildren(host);

  const clock = createTimers();
  let copyFails = options.copyFails ?? false;
  const copy = vi.fn((): Promise<void> =>
    copyFails ? Promise.reject(new Error('No clipboard.')) : Promise.resolve(),
  );
  const ui = {
    card: host.querySelector<HTMLElement>('#card')!,
    title: host.querySelector<HTMLElement>('#title')!,
    note: host.querySelector<HTMLElement>('#note')!,
    link: host.querySelector<HTMLInputElement>('#link')!,
    copyButton: host.querySelector<HTMLButtonElement>('#copy')!,
    closeButton: host.querySelector<HTMLButtonElement>('#close')!,
  };
  const share = host.querySelector<HTMLButtonElement>('#share')!;
  const card = new ShareCard(ui, {
    copy,
    returnFocus: share,
    timers: clock.timers,
  });
  return {
    ui,
    card,
    copy,
    clock,
    share,
    failCopy: (value: boolean): void => {
      copyFails = value;
    },
  };
}

const LINK = 'https://example.test/cube/#s=abc';

describe('ShareCard', () => {
  it('shows a copied link and goes away on its own', () => {
    const { ui, card, clock } = build();

    card.show({ link: LINK, copied: true, opens: 'It opens this cube.' });
    expect(ui.card.hidden).toBe(false);
    expect(ui.card.dataset.copied).toBe('true');
    expect(ui.title.textContent).toBe('Link copied');
    expect(ui.note.textContent).toBe('It opens this cube.');
    expect(ui.link.value).toBe(LINK);

    clock.runAll();
    expect(ui.card.hidden).toBe(true);
    expect(card.isOpen()).toBe(false);
  });

  it('stays while somebody is on it, and gets the whole time back after', () => {
    const { ui, card, clock } = build();
    card.show({ link: LINK, copied: true, opens: 'It opens this cube.' });

    ui.card.dispatchEvent(new Event('pointerenter'));
    expect(clock.pendingCount()).toBe(0);
    clock.runAll();
    expect(ui.card.hidden).toBe(false);

    ui.card.dispatchEvent(new Event('pointerleave'));
    expect(clock.pendingCount()).toBe(1);
    clock.runAll();
    expect(ui.card.hidden).toBe(true);
  });

  it('keeps a link the clipboard refused, selected and in focus', () => {
    const { ui, card, clock } = build();

    card.show({ link: LINK, copied: false, opens: 'It opens this cube.' });
    expect(ui.card.dataset.copied).toBe('false');
    expect(ui.title.textContent).toBe('Copy this link');
    expect(ui.note.textContent).toContain('copy it yourself');
    expect(document.activeElement).toBe(ui.link);
    expect(ui.link.selectionStart).toBe(0);
    expect(ui.link.selectionEnd).toBe(LINK.length);

    // Nothing takes it down but a person.
    clock.runAll();
    expect(ui.card.hidden).toBe(false);
  });

  it('copies again from its own button', async () => {
    const { ui, card, copy } = build();
    card.show({ link: LINK, copied: true, opens: 'It opens this cube.' });

    ui.copyButton.click();
    expect(copy).toHaveBeenCalledWith(LINK);
    await vi.waitFor(() => expect(ui.copyButton.textContent).toBe('Copied'));
  });

  it('turns a refused link into a copied one when the second try works', async () => {
    const { ui, card } = build();
    card.show({ link: LINK, copied: false, opens: 'It opens this cube.' });

    ui.copyButton.click();
    await vi.waitFor(() => expect(ui.card.dataset.copied).toBe('true'));
    expect(ui.title.textContent).toBe('Link copied');
    expect(ui.note.textContent).toBe('It opens this cube.');
  });

  it('closes on its button and on Escape, giving focus back to Share', () => {
    const { ui, card, share } = build();

    card.show({ link: LINK, copied: false, opens: 'It opens this cube.' });
    ui.closeButton.focus();
    ui.closeButton.click();
    expect(ui.card.hidden).toBe(true);
    expect(document.activeElement).toBe(share);

    card.show({ link: LINK, copied: false, opens: 'It opens this cube.' });
    ui.link.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }),
    );
    expect(ui.card.hidden).toBe(true);
  });

  it('stops answering at teardown', () => {
    const { ui, card, copy } = build();
    card.show({ link: LINK, copied: true, opens: 'It opens this cube.' });

    card.teardown();
    expect(ui.card.hidden).toBe(true);
    ui.copyButton.click();
    expect(copy).not.toHaveBeenCalled();
    card.show({ link: LINK, copied: true, opens: 'It opens this cube.' });
    expect(ui.card.hidden).toBe(true);
  });
});
