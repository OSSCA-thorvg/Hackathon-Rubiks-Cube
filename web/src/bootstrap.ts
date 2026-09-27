import {
  startApp,
  type AppState,
  type StartAppOptions,
} from './AppLifecycle.ts';
import {
  controllerControls,
  PLAY_MOVES_EVENT,
  type PlayMovesDetail,
} from './game/GameController.ts';
import { parseMoves } from './game/notation.ts';
import { attachCommandPalette, type Command } from './ui/CommandPalette.ts';
import { attachDetailPanels } from './ui/DetailPanels.ts';
import { createGameShell, ICONS, type GameShell } from './ui/GameShell.ts';
import { lightingControls } from './ui/LightingControls.ts';
import { attachSettingsPanel, type SettingsPanel } from './ui/SettingsPanel.ts';
import {
  CubeScene,
  CubeSurface,
  DEFAULT_CUBE_SIZE,
} from './wasm/CubeEngine.ts';
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
 * The width from which the rails sit beside the stage and the panels can be
 * open side by side. Written once here and once in the stylesheet, which
 * draws the same line with `min-width: 1024px`.
 */
const WIDE_QUERY = '(min-width: 1024px)';

/** The media query for the wide layout. */
function wideQuery(): MediaQueryList {
  return window.matchMedia(WIDE_QUERY);
}

/** Whether this is a Mac or an iPhone, whose menu shortcut is Command-K. */
function isApple(): boolean {
  return /Mac|iPhone|iPad|iPod/i.test(
    navigator.platform === '' ? navigator.userAgent : navigator.platform,
  );
}

/**
 * Every command the menu offers, each one a press on a control already here.
 *
 * Nothing is run that the page could not run by hand, and nothing is offered
 * that the page would not let a hand press: a command is available exactly
 * while its button is enabled and not hidden, and the controller that owns
 * the button decides both, as it always has. A button in the Settings drawer
 * counts whether the drawer is open or not -- reaching it without opening
 * the drawer is what the menu is for.
 */
function pageCommands(
  shell: GameShell,
  settings: SettingsPanel,
): Command[] {
  const { ui } = shell;
  const press = (button: HTMLButtonElement) => (): void => button.click();
  const usable = (button: HTMLButtonElement) => (): boolean =>
    !button.disabled && !button.hidden;
  const pressed = (button: HTMLButtonElement): boolean =>
    button.getAttribute('aria-pressed') === 'true';
  const chosen = (button: HTMLButtonElement) => (): string | null =>
    pressed(button) ? 'Current' : null;
  const button = (
    label: string,
    target: HTMLButtonElement,
    icon: string,
    keywords: string,
  ): Command => ({
    label: () => label,
    keywords,
    icon,
    available: usable(target),
    run: press(target),
  });
  const viewIcon: Readonly<Record<string, string>> = {
    '3d': ICONS.cube,
    both: ICONS.split,
    '2d': ICONS.net,
  };
  const panelIcon: Readonly<Record<string, string>> = {
    moves: ICONS.moves,
    session: ICONS.session,
    turn: ICONS.turn,
  };

  return [
    button('Scramble', ui.scrambleButton, ICONS.scramble, 'shuffle mix new start'),
    button('Stop', ui.stopButton, ICONS.stop, 'halt pause break off'),
    button('Undo', ui.undoButton, ICONS.undo, 'back take previous'),
    button('Redo', ui.redoButton, ICONS.redo, 'forward again next'),
    button('Rewind to solved', ui.rewindButton, ICONS.rewind, 'rewind reverse back start'),
    button('Solve', ui.solveButton, ICONS.solve, 'solver solution answer finish'),
    {
      label: () => (pressed(ui.ambientButton) ? 'Stop watching' : 'Watch the cube'),
      keywords: 'ambient idle demo pattern',
      icon: ICONS.watch,
      available: usable(ui.ambientButton),
      run: press(ui.ambientButton),
    },
    {
      label: () =>
        pressed(ui.paintButton) ? 'Stop colouring' : 'Colour your own cube',
      keywords: 'paint color colour draw real',
      icon: ICONS.paint,
      available: usable(ui.paintButton),
      run: press(ui.paintButton),
    },
    button('Copy a share link', ui.shareButton, ICONS.share, 'share link url copy send'),
    ...ui.viewButtons.map(
      (view): Command => ({
        label: () => `View: ${view.textContent ?? ''}`,
        keywords: 'scene show look split flat net cube',
        icon: viewIcon[view.dataset.view ?? ''],
        available: usable(view),
        note: chosen(view),
        run: press(view),
      }),
    ),
    ...ui.flatButtons.map(
      (flat): Command => ({
        label: () => `Diagram: ${flat.textContent ?? ''}`,
        keywords: 'flat drawing net rings',
        icon: ICONS.net,
        available: usable(flat),
        note: chosen(flat),
        run: press(flat),
      }),
    ),
    ...shell.panelToggles.map(
      (toggle): Command => ({
        label: () =>
          `${toggle.getAttribute('aria-expanded') === 'true' ? 'Hide' : 'Show'} ${
            toggle.textContent ?? ''
          } panel`,
        keywords: 'details panel open close',
        icon: panelIcon[toggle.dataset.panel ?? ''],
        available: () => true,
        run: press(toggle),
      }),
    ),
    {
      label: () => 'Settings',
      keywords: 'preferences options',
      icon: ICONS.settings,
      available: () => true,
      run: () => settings.open(),
    },
    {
      label: () => 'Change cube size',
      keywords: 'size layers bigger smaller 2x2 4x4',
      icon: ICONS.cube,
      available: () => !ui.cubeSizeInput.disabled,
      run: () => settings.open({ focus: ui.cubeSizeInput }),
    },
    button('Home view', ui.homeViewButton, ICONS.home, 'camera reset view orbit'),
    {
      label: () => (pressed(ui.muteButton) ? 'Unmute turns' : 'Mute turns'),
      keywords: 'sound audio click quiet',
      icon: ICONS.sound,
      available: usable(ui.muteButton),
      run: press(ui.muteButton),
    },
    ...shell.themeButtons.map(
      (theme): Command => ({
        label: () => `Theme: ${theme.textContent ?? ''}`,
        keywords: 'appearance dark light mode system',
        icon: ICONS.sun,
        available: usable(theme),
        note: chosen(theme),
        run: press(theme),
      }),
    ),
    ...ui.paletteButtons.map(
      (palette): Command => ({
        label: () => `Stickers: ${palette.textContent ?? ''}`,
        keywords: 'palette colors colours contrast',
        icon: ICONS.paint,
        available: usable(palette),
        note: chosen(palette),
        run: press(palette),
      }),
    ),
    button('Reset session', ui.resetButton, ICONS.reset, 'clear fresh new cube'),
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
 * What is left here is wiring. The markup moved to GameShell, the settings
 * drawer, the detail panels and the command menu own their own behavior, and
 * the theme is a controller this hands to the lifecycle rather than something
 * the lifecycle asks the page for.
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
  // selector and the detail panels are the page's own and work whether or
  // not an engine ever arrives -- a browser that cannot run the cube can
  // still be read in the theme its owner chose.
  for (const control of controllerControls(shell.ui)) control.disabled = true;
  // The lighting sliders too: they write straight into the engine, so until
  // there is one they have nothing to write to.
  for (const control of lightingControls(shell.lightingUi)) {
    control.disabled = true;
  }

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

  const settings = attachSettingsPanel({
    trigger: shell.settingsTrigger,
    panel: shell.settingsPanel,
    backdrop: shell.settingsBackdrop,
    close: shell.settingsClose,
  });

  // The size beside the title is a way into Settings, at the one field it
  // names; the cube it shows is written there by the game controller.
  shell.cubeSizeChip.addEventListener('click', () => {
    settings.open({ focus: shell.ui.cubeSizeInput });
  });

  const wide = wideQuery();
  attachDetailPanels({
    toggles: shell.panelToggles,
    root: shell.root,
    oneAtATime: () => !wide.matches,
    watchWidth: (listener) => {
      wide.addEventListener('change', listener);
      return () => wide.removeEventListener('change', listener);
    },
  });

  // The two buttons beside the depth field are a bigger way of doing what
  // its own arrows do, and they end the same way: a change the controller
  // reads and either keeps or puts back.
  for (const stepper of shell.depthSteppers) {
    stepper.addEventListener('click', () => {
      const field = shell.ui.turnDepthInput;
      if (field.disabled) return;
      const next = Number(field.value) + Number(stepper.dataset.step);
      if (next < Number(field.min) || next > Number(field.max)) return;
      field.value = String(next);
      field.dispatchEvent(new Event('change', { bubbles: true }));
    });
  }

  const palette = attachCommandPalette({
    shell: shell.command,
    commands: () => pageCommands(shell, settings),
    parse: (text) =>
      parseMoves(
        text,
        Number(shell.ui.cubeSizeInput.value) || DEFAULT_CUBE_SIZE,
      ),
    // The controller enables the dock when it attaches and puts it out when
    // it goes, so the dock says whether anybody is listening for the moves.
    canPlay: () => !shell.ui.scrambleButton.disabled,
    play: (text) => {
      shell.root.dispatchEvent(
        new CustomEvent<PlayMovesDetail>(PLAY_MOVES_EVENT, { detail: { text } }),
      );
    },
    blocked: () => settings.isOpen(),
    mac: isApple(),
    playIcon: ICONS.play,
  });

  // The hint is a first-visit line and nothing else: the first gesture is
  // proof it was read, and there is nothing to remember past that. A reload
  // is a fresh page and offers it again, which is the same promise the rest
  // of the session makes.
  const dismissHint = (): void => {
    shell.interactionHint.hidden = true;
  };
  shell.ui.stage.addEventListener('pointerdown', dismissHint, { once: true });

  // A canvas for each view, so the page lays them out and the engine only
  // fits each drawing to the box it is given. The cube's canvas keeps the
  // scenes none of these take, which here is the cube alone.
  return start({
    canvas: shell.views.cube,
    views: [
      {
        id: CubeSurface.Net,
        canvas: shell.views.net,
        scenes: CubeScene.Net,
        pressable: true,
      },
      {
        id: CubeSurface.Rings,
        canvas: shell.views.rings,
        scenes: CubeScene.Rings,
        pressable: true,
      },
      {
        id: CubeSurface.Axes,
        canvas: shell.views.axes,
        scenes: CubeScene.Axes,
        pressable: false,
      },
    ],
    gameUi: shell.ui,
    lightingUi: shell.lightingUi,
    // Both of the page's dialogs are modal, so nothing they cover answers a
    // key -- the cube included.
    blocked: () => settings.isOpen() || palette.isOpen(),
    setState,
    onError: fail,
    theme: theme ?? undefined,
  }).then(() => undefined, fail);
}
