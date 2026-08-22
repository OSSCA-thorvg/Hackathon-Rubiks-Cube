import { describe, expect, it, vi } from 'vitest';

import {
  attachSettingsPanel,
  type KeyboardTarget,
} from '../../src/ui/SettingsPanel.ts';
import { attachActivityTabs } from '../../src/ui/ActivityTabs.ts';

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

describe('attachActivityTabs', () => {
  function buildTabs() {
    const host = document.createElement('div');
    host.innerHTML = `
      <button role="tab" data-activity="moves" aria-selected="true"></button>
      <button role="tab" data-activity="session" aria-selected="false" tabindex="-1"></button>
      <div role="tabpanel" data-activity="moves"></div>
      <div role="tabpanel" data-activity="session" hidden></div>
    `;
    document.body.replaceChildren(host);
    const tabs = [...host.querySelectorAll<HTMLButtonElement>('[role="tab"]')];
    const panels = [...host.querySelectorAll<HTMLElement>('[role="tabpanel"]')];
    return { tabs, panels, controller: attachActivityTabs({ tabs, panels }) };
  }

  it('shows one panel at a time and keeps both in the document', () => {
    const { tabs, panels, controller } = buildTabs();

    expect(controller.selected()).toBe('moves');
    expect(panels[0]!.hidden).toBe(false);
    expect(panels[1]!.hidden).toBe(true);

    tabs[1]!.click();
    expect(controller.selected()).toBe('session');
    expect(panels[0]!.hidden).toBe(true);
    expect(panels[1]!.hidden).toBe(false);
    expect(tabs[1]!.getAttribute('aria-selected')).toBe('true');
    // The records are still there to be read, which is what keeps a hidden
    // panel from being the same thing as a panel that was never drawn.
    expect(panels[1]!.isConnected).toBe(true);
  });

  it('walks the list with the arrow keys', () => {
    const { tabs, controller } = buildTabs();

    tabs[0]!.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', cancelable: true }),
    );
    expect(controller.selected()).toBe('session');
    expect(tabs[1]!.tabIndex).toBe(0);
    expect(tabs[0]!.tabIndex).toBe(-1);

    // And wraps, so the list has no dead end at either side.
    tabs[1]!.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', cancelable: true }),
    );
    expect(controller.selected()).toBe('moves');
  });

  it('ignores a name no tab carries', () => {
    const { controller } = buildTabs();

    controller.select('nowhere');
    expect(controller.selected()).toBe('moves');
  });

  it('stops answering at teardown', () => {
    const { tabs, controller } = buildTabs();

    controller.teardown();
    tabs[1]!.click();
    expect(controller.selected()).toBe('moves');
  });
});
