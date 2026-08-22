import {
  startApp,
  type AppState,
  type StartAppOptions,
} from './AppLifecycle.ts';
import type { GameUi } from './game/GameController.ts';
import { attachActivityTabs } from './ui/ActivityTabs.ts';
import { createGameShell } from './ui/GameShell.ts';
import { attachSettingsPanel } from './ui/SettingsPanel.ts';
import {
  attachThemeController,
  attachThemeSelector,
  type StorageLike,
  type ThemeController,
} from './ui/ThemeController.ts';

/** Lifecycle entry point, injectable so tests can observe the wiring. */
export type StartAppFn = (options: StartAppOptions) => Promise<unknown>;

/** Browser feature detection seam, injectable for DOM integration tests. */
export type SupportCheckFn = () => boolean;

/**
 * Where the page's theme comes from; injectable so tests need no globals.
 *
 * Returns null for a page that cannot have one, which is how a test says it
 * is not exercising the theme rather than having to stand in for a media
 * query and a storage.
 */
export type ThemeFactory = () => ThemeController | null;

/**
 * Every control the game controller owns, flattened out of the typed shell.
 *
 * Read off the fields rather than by querying the markup, so a control the
 * page adds beside them -- a tab, a theme button, the settings trigger --
 * cannot be switched off by a rule that was written for the cube.
 */
function gameplayControls(
  ui: GameUi,
): (HTMLButtonElement | HTMLInputElement)[] {
  return [
    ui.scrambleButton,
    ui.scrambleMovesInput,
    ui.resetButton,
    ui.cubeSizeInput,
    ui.turnDepthInput,
    ui.turnWideButton,
    ui.undoButton,
    ui.redoButton,
    ui.rewindButton,
    ui.solveButton,
    ui.stopButton,
    ui.shareButton,
    ui.ambientButton,
    ui.homeViewButton,
    ui.muteButton,
    ui.speedInput,
    ...ui.viewButtons,
    ...ui.flatButtons,
    ...ui.paletteButtons,
    ...ui.moveButtons,
  ];
}

/** Returns whether the browser has every primitive required by the app. */
export function supportsBrowser(): boolean {
  if (
    typeof WebAssembly === 'undefined' ||
    typeof PointerEvent === 'undefined' ||
    typeof ResizeObserver === 'undefined' ||
    typeof crypto === 'undefined' ||
    typeof crypto.getRandomValues !== 'function'
  ) {
    return false;
  }

  const probe = document.createElement('canvas');
  return probe.getContext('2d') !== null;
}

/**
 * Local storage, or nothing when the browser will not hand it over.
 *
 * Reaching for the property is itself what throws in a browser with site data
 * blocked, so the guard is around the access and not only around the calls
 * made on what it returns.
 */
function readableStorage(): StorageLike | null {
  try {
    return window.localStorage;
  } catch {
    return null;
  }
}

/** Builds the real theme controller from the page's own globals. */
export function createPageTheme(): ThemeController {
  return attachThemeController({
    root: document.documentElement,
    systemDark: window.matchMedia('(prefers-color-scheme: dark)'),
    storage: readableStorage(),
  });
}

/**
 * Wires the page DOM to the app lifecycle: builds the shell, attaches the
 * page's own small controllers, reflects lifecycle states on the container,
 * and routes both startup rejections and post-ready failures to the error UI.
 *
 * What is left here is wiring. The markup moved to GameShell, the panel and
 * the tabs own their own behavior, and the theme is a controller this hands
 * to the lifecycle rather than something the lifecycle asks the page for.
 *
 * The returned promise settles when startup finished either way; it never
 * rejects, because failures are presented through the error state.
 */
export function bootstrap(
  app: HTMLElement,
  start: StartAppFn = startApp,
  supportCheck: SupportCheckFn = supportsBrowser,
  createTheme: ThemeFactory = createPageTheme,
): Promise<void> {
  const shell = createGameShell(app);
  const statusElement = shell.ui.status;

  // Only the gameplay controls, which the game controller takes over from
  // here and owns the enabled state of. The settings trigger, the theme
  // selector and the activity tabs are the page's own and work whether or
  // not an engine ever arrives -- a browser that cannot run the cube can
  // still be read in the theme its owner chose.
  for (const control of gameplayControls(shell.ui)) control.disabled = true;

  const setState = (state: AppState, message: string): void => {
    app.dataset.state = state;
    statusElement.textContent = message;
  };

  const fail = (error: unknown): void => {
    console.error(error);
    setState('error', 'Failed to start the ThorVG engine.');
  };

  if (!supportCheck()) {
    setState(
      'unsupported',
      'This browser does not support the WebAssembly and canvas features required by the cube.',
    );
    return Promise.resolve();
  }

  // After the support check, so a browser that cannot run the cube is told so
  // rather than failing on a media query on the way there.
  const theme: ThemeController | null = createTheme();
  if (theme !== null) {
    attachThemeSelector({ buttons: shell.themeButtons, controller: theme });
  }

  attachSettingsPanel({
    trigger: shell.settingsTrigger,
    panel: shell.settingsPanel,
    backdrop: shell.settingsBackdrop,
    close: shell.settingsClose,
  });

  attachActivityTabs({
    tabs: shell.activityTabs,
    panels: shell.activityPanels,
  });

  // The hint is a first-visit line and nothing else: the first gesture is
  // proof it was read, and there is nothing to remember past that. A reload
  // is a fresh page and offers it again, which is the same promise the rest
  // of the session makes.
  const dismissHint = (): void => {
    shell.interactionHint.hidden = true;
  };
  shell.ui.canvas.addEventListener('pointerdown', dismissHint, { once: true });

  return start({
    canvas: shell.ui.canvas,
    gameUi: shell.ui,
    setState,
    onError: fail,
    theme: theme ?? undefined,
  }).then(() => undefined, fail);
}
