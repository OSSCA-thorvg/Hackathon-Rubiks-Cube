import { describe, expect, it, vi } from 'vitest';

import {
  attachThemeController,
  attachThemeSelector,
  canvasThemeOf,
  effectiveTheme,
  loadThemePreference,
  saveThemePreference,
  THEME_STORAGE_KEY,
  type MediaQueryLike,
  type StorageLike,
} from '../../src/ui/ThemeController.ts';
import { CubeCanvasTheme } from '../../src/wasm/CubeEngine.ts';

/** A media query whose answer the test moves, the way a machine would. */
function createMediaQuery(matches: boolean) {
  const listeners = new Set<() => void>();
  const query = {
    matches,
    addEventListener: vi.fn((_type: 'change', listener: () => void) => {
      listeners.add(listener);
    }),
    removeEventListener: vi.fn((_type: 'change', listener: () => void) => {
      listeners.delete(listener);
    }),
  };

  return {
    query: query as unknown as MediaQueryLike,
    listenerCount: (): number => listeners.size,
    /** The machine changes its mind. */
    set(next: boolean): void {
      query.matches = next;
      for (const listener of [...listeners]) listener();
    },
  };
}

/** An in-memory storage, optionally one that refuses everything. */
function createStorage(broken = false): StorageLike {
  const values = new Map<string, string>();
  return {
    getItem: (key) => {
      if (broken) throw new Error('storage is blocked');
      return values.get(key) ?? null;
    },
    setItem: (key, value) => {
      if (broken) throw new Error('storage is blocked');
      values.set(key, value);
    },
    removeItem: (key) => {
      if (broken) throw new Error('storage is blocked');
      values.delete(key);
    },
  };
}

describe('theme values', () => {
  it('answers the machine only for system', () => {
    expect(effectiveTheme('system', true)).toBe('dark');
    expect(effectiveTheme('system', false)).toBe('light');
    expect(effectiveTheme('light', true)).toBe('light');
    expect(effectiveTheme('dark', false)).toBe('dark');
  });

  it('translates an effective theme into the engine enum', () => {
    expect(canvasThemeOf('light')).toBe(CubeCanvasTheme.Light);
    expect(canvasThemeOf('dark')).toBe(CubeCanvasTheme.Dark);
  });
});

describe('theme persistence', () => {
  it('demotes a missing or unknown value to system', () => {
    const storage = createStorage();
    expect(loadThemePreference(storage)).toBe('system');

    storage.setItem(THEME_STORAGE_KEY, 'chartreuse');
    expect(loadThemePreference(storage)).toBe('system');

    storage.setItem(THEME_STORAGE_KEY, 'light');
    expect(loadThemePreference(storage)).toBe('light');
  });

  it('stores system as the absence of a value', () => {
    const storage = createStorage();

    saveThemePreference(storage, 'dark');
    expect(storage.getItem(THEME_STORAGE_KEY)).toBe('dark');

    saveThemePreference(storage, 'system');
    expect(storage.getItem(THEME_STORAGE_KEY)).toBeNull();
  });

  it('survives a storage that refuses to be read or written', () => {
    const broken = createStorage(true);
    expect(loadThemePreference(broken)).toBe('system');
    expect(() => saveThemePreference(broken, 'dark')).not.toThrow();
    expect(loadThemePreference(null)).toBe('system');
  });
});

describe('attachThemeController', () => {
  function attach(options: { dark?: boolean; storage?: StorageLike } = {}) {
    const media = createMediaQuery(options.dark ?? false);
    const root = document.createElement('html');
    const controller = attachThemeController({
      root,
      systemDark: media.query,
      storage: options.storage ?? createStorage(),
    });
    return { media, root, controller };
  }

  it('follows the machine while system is chosen', () => {
    const { media, root, controller } = attach({ dark: false });
    const seen: string[] = [];
    controller.subscribe((theme) => seen.push(theme));

    expect(controller.preference()).toBe('system');
    expect(controller.effective()).toBe('light');
    expect(root.dataset.theme).toBeUndefined();

    media.set(true);
    expect(controller.effective()).toBe('dark');
    expect(controller.preference()).toBe('system');
    // Still no attribute: what was chosen is "ask the machine", and the
    // stylesheet reads the absence of one as exactly that.
    expect(root.dataset.theme).toBeUndefined();
    expect(seen).toEqual(['dark']);
  });

  it('is not overruled by the machine once a theme is chosen', () => {
    const { media, root, controller } = attach({ dark: false });
    const seen: string[] = [];
    controller.subscribe((theme) => seen.push(theme));

    controller.setPreference('dark');
    expect(root.dataset.theme).toBe('dark');
    expect(seen).toEqual(['dark']);

    media.set(true);
    media.set(false);
    expect(controller.effective()).toBe('dark');
    expect(seen).toEqual(['dark']);
  });

  it('tells subscribers only when the answer moved', () => {
    const { media, controller } = attach({ dark: true });
    const seen: string[] = [];
    controller.subscribe((theme) => seen.push(theme));

    // Already dark, and choosing dark explicitly does not repaint anything.
    controller.setPreference('dark');
    expect(seen).toEqual([]);

    controller.setPreference('light');
    expect(seen).toEqual(['light']);

    controller.setPreference('system');
    expect(seen).toEqual(['light', 'dark']);
    media.set(true);
    expect(seen).toEqual(['light', 'dark']);
  });

  it('opens on the saved theme and writes it back', () => {
    const storage = createStorage();
    storage.setItem(THEME_STORAGE_KEY, 'light');

    const { root, controller } = attach({ dark: true, storage });
    expect(controller.preference()).toBe('light');
    expect(controller.effective()).toBe('light');
    expect(root.dataset.theme).toBe('light');

    controller.setPreference('system');
    expect(storage.getItem(THEME_STORAGE_KEY)).toBeNull();
    expect(root.dataset.theme).toBeUndefined();
  });

  it('applies a choice the storage refuses to keep', () => {
    const { root, controller } = attach({ storage: createStorage(true) });

    controller.setPreference('dark');
    expect(root.dataset.theme).toBe('dark');
    expect(controller.effective()).toBe('dark');
  });

  it('drops the media listener and the subscribers at teardown', () => {
    const { media, controller } = attach();
    const seen: string[] = [];
    const unsubscribe = controller.subscribe((theme) => seen.push(theme));

    unsubscribe();
    controller.setPreference('dark');
    expect(seen).toEqual([]);

    controller.teardown();
    expect(media.listenerCount()).toBe(0);
  });
});

describe('attachThemeSelector', () => {
  function build() {
    const media = createMediaQuery(true);
    const root = document.createElement('html');
    const controller = attachThemeController({
      root,
      systemDark: media.query,
      storage: createStorage(),
    });

    const buttons = (['system', 'light', 'dark'] as const).map((choice) => {
      const button = document.createElement('button');
      button.dataset.themeChoice = choice;
      return button;
    });
    const selector = attachThemeSelector({ buttons, controller });
    return { buttons, controller, selector, media, root };
  }

  it('presses the chosen preference, not the theme it resolves to', () => {
    const { buttons, controller, media } = build();
    const [system, light, dark] = buttons;

    expect(system!.getAttribute('aria-pressed')).toBe('true');
    expect(controller.effective()).toBe('dark');

    light!.click();
    expect(controller.preference()).toBe('light');
    expect(light!.getAttribute('aria-pressed')).toBe('true');
    expect(system!.getAttribute('aria-pressed')).toBe('false');

    // Back to the machine, which is dark. System stays the pressed button.
    system!.click();
    media.set(true);
    expect(controller.effective()).toBe('dark');
    expect(system!.getAttribute('aria-pressed')).toBe('true');
    expect(dark!.getAttribute('aria-pressed')).toBe('false');
  });

  it('stops answering the buttons at teardown', () => {
    const { buttons, controller, selector } = build();
    selector.teardown();

    buttons[1]!.click();
    expect(controller.preference()).toBe('system');
  });
});
