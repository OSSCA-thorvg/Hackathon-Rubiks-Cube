import { describe, expect, it, vi } from 'vitest';

import {
  attachSettingsPanel,
  type KeyboardTarget,
} from '../../src/ui/SettingsPanel.ts';

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
    press(key: string, shiftKey = false): KeyboardEvent {
      const event = new KeyboardEvent('keydown', {
        key,
        shiftKey,
        cancelable: true,
      });
      for (const listener of [...listeners]) listener(event);
      return event;
    },
  };
}

function build() {
  const host = document.createElement('div');
  host.innerHTML = `
    <button id="trigger" aria-expanded="false"></button>
    <div id="backdrop" hidden></div>
    <aside id="panel" hidden>
      <button id="close"></button>
      <button id="first"></button>
      <button id="last"></button>
    </aside>
  `;
  document.body.replaceChildren(host);

  const keyboard = createKeyboard();
  const panel = attachSettingsPanel({
    trigger: host.querySelector<HTMLButtonElement>('#trigger')!,
    panel: host.querySelector<HTMLElement>('#panel')!,
    backdrop: host.querySelector<HTMLElement>('#backdrop')!,
    close: host.querySelector<HTMLButtonElement>('#close')!,
    keyboardTarget: keyboard.target,
  });

  return {
    panel,
    keyboard,
    trigger: host.querySelector<HTMLButtonElement>('#trigger')!,
    surface: host.querySelector<HTMLElement>('#panel')!,
    backdrop: host.querySelector<HTMLElement>('#backdrop')!,
    close: host.querySelector<HTMLButtonElement>('#close')!,
    last: host.querySelector<HTMLButtonElement>('#last')!,
  };
}

describe('attachSettingsPanel', () => {
  it('opens from the trigger and says so on it', () => {
    const { panel, trigger, surface, backdrop } = build();

    expect(panel.isOpen()).toBe(false);
    trigger.click();

    expect(panel.isOpen()).toBe(true);
    expect(surface.hidden).toBe(false);
    expect(backdrop.hidden).toBe(false);
    expect(trigger.getAttribute('aria-expanded')).toBe('true');
    // Focus arrives inside, so a keyboard is not left behind the panel.
    expect(document.activeElement).toBe(
      surface.querySelector<HTMLButtonElement>('#close'),
    );
  });

  it('closes on Escape, the backdrop and the done button alike', () => {
    const { panel, trigger, keyboard, backdrop, close } = build();

    trigger.click();
    const escape = keyboard.press('Escape');
    expect(panel.isOpen()).toBe(false);
    expect(escape.defaultPrevented).toBe(true);
    expect(document.activeElement).toBe(trigger);

    trigger.click();
    backdrop.dispatchEvent(new Event('pointerdown'));
    expect(panel.isOpen()).toBe(false);

    trigger.click();
    close.click();
    expect(panel.isOpen()).toBe(false);
    expect(trigger.getAttribute('aria-expanded')).toBe('false');
  });

  it('keeps Tab inside the panel', () => {
    const { panel, trigger, keyboard, close, last } = build();

    trigger.click();
    expect(panel.isOpen()).toBe(true);

    // Backwards off the first stop wraps to the last one.
    close.focus();
    const back = keyboard.press('Tab', true);
    expect(back.defaultPrevented).toBe(true);
    expect(document.activeElement).toBe(last);

    // And forwards off the last one wraps to the first.
    const forward = keyboard.press('Tab');
    expect(forward.defaultPrevented).toBe(true);
    expect(document.activeElement).toBe(close);
  });

  it('opens onto the one control it was opened for', () => {
    const { panel, last } = build();

    panel.open({ focus: last });
    expect(document.activeElement).toBe(last);

    // Something outside the panel is not somewhere it can send focus, and
    // neither is a control inside it that is out of reach.
    panel.close();
    const outside = document.createElement('button');
    document.body.append(outside);
    panel.open({ focus: outside });
    expect(document.activeElement).not.toBe(outside);

    panel.close();
    last.disabled = true;
    panel.open({ focus: last });
    expect(document.activeElement).not.toBe(last);
    expect(panel.isOpen()).toBe(true);
  });

  it('ignores keys while it is closed', () => {
    const { keyboard, panel } = build();

    const escape = keyboard.press('Escape');
    expect(escape.defaultPrevented).toBe(false);
    expect(panel.isOpen()).toBe(false);
  });

  it('closes and unwires at teardown without moving focus', () => {
    const { panel, trigger, keyboard, surface } = build();

    trigger.click();
    const elsewhere = document.createElement('button');
    document.body.append(elsewhere);
    elsewhere.focus();

    panel.teardown();
    expect(surface.hidden).toBe(true);
    expect(document.activeElement).toBe(elsewhere);
    expect(keyboard.listenerCount()).toBe(0);

    trigger.click();
    expect(panel.isOpen()).toBe(false);
  });
});
