import { CubeCanvasTheme } from '../wasm/CubeEngine.ts';

/**
 * What a person can choose between.
 *
 * Three values, and one of them is not a color: `system` is a standing
 * question about the machine rather than an answer, so it is the only one
 * whose meaning can change while the page is open.
 */
export type ThemePreference = 'system' | 'light' | 'dark';

/**
 * What the page and the canvas can actually be.
 *
 * Two values, because a surface is painted one way or the other. Everything
 * downstream of the question takes this rather than the preference, which is
 * what keeps "follow the machine" from having to be understood twice.
 */
export type EffectiveTheme = 'light' | 'dark';

/** Where the one saved preference lives. */
export const THEME_STORAGE_KEY = 'thorvg-rubiks-theme';

/** Turns the machine's answer into the theme a surface is painted in. */
export function effectiveTheme(
  preference: ThemePreference,
  prefersDark: boolean,
): EffectiveTheme {
  if (preference === 'system') return prefersDark ? 'dark' : 'light';
  return preference;
}

/** Translates an effective theme into the engine's primitive enum. */
export function canvasThemeOf(theme: EffectiveTheme): CubeCanvasTheme {
  return theme === 'dark' ? CubeCanvasTheme.Dark : CubeCanvasTheme.Light;
}

/**
 * The three storage calls this uses, injectable for tests.
 *
 * Narrower than `Storage` so a test does not have to implement `key`,
 * `length` and `clear` to stand in for it.
 */
export type StorageLike = {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
};

/**
 * Reads the saved preference, demoting anything unrecognized to `system`.
 *
 * A missing key and a key holding nonsense are the same situation: nobody
 * has chosen, so the machine is asked. Storage that throws on access -- a
 * browser with cookies blocked does exactly that -- is also nobody having
 * chosen, rather than a page that fails to start.
 */
export function loadThemePreference(
  storage: StorageLike | null,
): ThemePreference {
  if (storage === null) return 'system';

  let stored: string | null = null;
  try {
    stored = storage.getItem(THEME_STORAGE_KEY);
  } catch {
    return 'system';
  }

  if (stored === 'light' || stored === 'dark') return stored;
  return 'system';
}

/**
 * Saves an explicit choice and clears the key for `system`.
 *
 * `system` is stored as the absence of a value rather than as the string
 * "system", so a saved preference and the default cannot disagree about what
 * a page with no key means.
 *
 * A write that throws loses the persistence and nothing else. The choice has
 * already been applied to the page by the time this runs.
 */
export function saveThemePreference(
  storage: StorageLike | null,
  preference: ThemePreference,
): void {
  if (storage === null) return;

  try {
    if (preference === 'system') {
      storage.removeItem(THEME_STORAGE_KEY);
      return;
    }
    storage.setItem(THEME_STORAGE_KEY, preference);
  } catch {
    // Deliberately swallowed: see above.
  }
}

/** The part of a MediaQueryList this needs, injectable for tests. */
export type MediaQueryLike = {
  readonly matches: boolean;
  addEventListener(type: 'change', listener: () => void): void;
  removeEventListener(type: 'change', listener: () => void): void;
};

/** What a theme change is handed to; the engine boundary is one of these. */
export type ThemeListener = (theme: EffectiveTheme) => void;

export type ThemeControllerOptions = {
  /** The element carrying `data-theme`; the document element in the page. */
  readonly root: HTMLElement;
  /** The standing `(prefers-color-scheme: dark)` query. */
  readonly systemDark: MediaQueryLike;
  /** Where the preference is kept, or null when there is nowhere. */
  readonly storage?: StorageLike | null;
};

export type ThemeController = {
  /** What the person chose, which may be `system`. */
  preference(): ThemePreference;
  /** What that currently means for a surface. */
  effective(): EffectiveTheme;
  /** Chooses, applies, and saves, in that order. */
  setPreference(preference: ThemePreference): void;
  /** Adds a listener and returns the call that removes it. */
  subscribe(listener: ThemeListener): () => void;
  /** Removes the media-query listener and forgets every subscriber. */
  teardown(): void;
};

/**
 * Owns the theme preference, the attribute it writes, and who hears about it.
 *
 * Not part of the game session: a theme is a property of the page rather than
 * of the cube, it survives a reset that clears everything else, and putting it
 * in the session would give the session a field no share link carries.
 *
 * The attribute is the only thing written to the DOM. Which colors that stands
 * for is the stylesheet's business, and the canvas hears about it through a
 * subscriber rather than through this reaching for an engine.
 */
export function attachThemeController(
  options: ThemeControllerOptions,
): ThemeController {
  const { root, systemDark } = options;
  const storage: StorageLike | null = options.storage ?? null;

  let preference: ThemePreference = loadThemePreference(storage);
  let current: EffectiveTheme = effectiveTheme(preference, systemDark.matches);
  let listeners: ThemeListener[] = [];

  /**
   * Writes the attribute for an explicit choice and removes it for `system`.
   *
   * The absence of the attribute is what the stylesheet reads as "ask the
   * machine", and it is also what the small script in the page head leaves
   * behind, so the two agree without sharing any code.
   */
  const writeAttribute = (): void => {
    if (preference === 'system') {
      delete root.dataset.theme;
      return;
    }
    root.dataset.theme = preference;
  };

  /** Tells the subscribers, but only when the answer actually moved. */
  const apply = (): void => {
    const next = effectiveTheme(preference, systemDark.matches);
    writeAttribute();
    if (next === current) return;

    current = next;
    for (const listener of listeners) listener(current);
  };

  /**
   * The machine changed its mind, which only matters while it is being asked.
   *
   * An explicit Light or Dark is a decision about this page, and a machine
   * switching to its night colors underneath one is not a reason to overrule
   * it.
   */
  const onSystemChange = (): void => {
    if (preference !== 'system') return;
    apply();
  };

  systemDark.addEventListener('change', onSystemChange);
  writeAttribute();

  return {
    preference: (): ThemePreference => preference,
    effective: (): EffectiveTheme => current,

    setPreference: (next: ThemePreference): void => {
      preference = next;
      // Applied before it is saved, so a storage that refuses the write still
      // leaves the page looking the way it was asked to look.
      apply();
      saveThemePreference(storage, next);
    },

    subscribe: (listener: ThemeListener): (() => void) => {
      listeners = [...listeners, listener];
      return (): void => {
        listeners = listeners.filter((entry) => entry !== listener);
      };
    },

    teardown: (): void => {
      systemDark.removeEventListener('change', onSystemChange);
      listeners = [];
    },
  };
}

export type ThemeSelectorOptions = {
  /** One button per preference, each carrying `data-theme-choice`. */
  readonly buttons: readonly HTMLButtonElement[];
  readonly controller: ThemeController;
};

export type ThemeSelector = {
  teardown(): void;
};

/** Reads a preference off a button, refusing anything else. */
function preferenceOf(button: HTMLElement): ThemePreference | null {
  const choice = button.dataset.themeChoice;
  if (choice === 'system' || choice === 'light' || choice === 'dark') {
    return choice;
  }
  return null;
}

/**
 * Points three buttons at the controller and keeps them showing the choice.
 *
 * The pressed button is the preference and never the effective theme: on a
 * machine that has just gone dark, `System` stays the pressed one, because
 * what was chosen is still "follow the machine". Showing `Dark` there would
 * be reporting an answer as though it were the question.
 */
export function attachThemeSelector(
  options: ThemeSelectorOptions,
): ThemeSelector {
  const { buttons, controller } = options;

  const apply = (): void => {
    const chosen = controller.preference();
    for (const button of buttons) {
      button.setAttribute('aria-pressed', String(preferenceOf(button) === chosen));
    }
  };

  const onClick = (event: Event): void => {
    const preference = preferenceOf(event.currentTarget as HTMLElement);
    if (preference === null) return;

    controller.setPreference(preference);
    apply();
  };

  for (const button of buttons) button.addEventListener('click', onClick);
  apply();

  return {
    teardown: (): void => {
      for (const button of buttons) button.removeEventListener('click', onClick);
    },
  };
}
