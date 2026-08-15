import {
  CubeFace,
  CubeFlatStyle,
  CubeViewMode,
  DEFAULT_SCRAMBLE_MOVES,
  isValidScrambleMoves,
  MAX_SCRAMBLE_MOVES,
  type FaceTurns,
} from '../wasm/CubeEngine.ts';
import {
  GameSession,
  type GameState,
  type SessionEngine,
} from './GameSession.ts';
import type { TimerEnvironment } from './SolveTimer.ts';

export type { GameState } from './GameSession.ts';

/** Engine surface required by gameplay controls. */
export type GameEngine = SessionEngine & {
  isBusy(): boolean;
  scramble(seed: number, moveCount: number): void;
  resetCube(): void;
  turnFace(face: CubeFace, faceTurns: FaceTurns): boolean;
  setViewMode(mode: CubeViewMode): void;
  viewMode(): CubeViewMode;
  setFlatStyle(style: CubeFlatStyle): void;
  flatStyle(): CubeFlatStyle;
  resetView(): void;
  render(): void;
};

/** Typed DOM elements owned by the gameplay controller. */
export type GameUi = {
  readonly root: HTMLElement;
  readonly canvas: HTMLCanvasElement;
  readonly timer: HTMLOutputElement;
  readonly status: HTMLParagraphElement;
  readonly scrambleButton: HTMLButtonElement;
  /** How many moves the next scramble is, as a plain integer. */
  readonly scrambleMovesInput: HTMLInputElement;
  readonly resetButton: HTMLButtonElement;
  readonly homeViewButton: HTMLButtonElement;
  readonly viewButtons: readonly HTMLButtonElement[];
  readonly flatButtons: readonly HTMLButtonElement[];
  readonly moveButtons: readonly HTMLButtonElement[];
};

/** Minimal keyboard event target, injectable for tests. */
export type KeyboardTarget = {
  addEventListener(type: 'keydown', listener: (event: KeyboardEvent) => void): void;
  removeEventListener(
    type: 'keydown',
    listener: (event: KeyboardEvent) => void,
  ): void;
};

/** Options for attaching one gameplay controller. */
export type GameControllerOptions = {
  readonly engine: GameEngine;
  readonly ui: GameUi;
  readonly startFrameLoop: () => void;
  readonly onError: (error: unknown) => void;
  readonly seedSource?: () => number;
  readonly timerEnvironment?: TimerEnvironment;
  readonly keyboardTarget?: KeyboardTarget;
};

/** Controller surface consumed by AppLifecycle. */
export type GameController = {
  readonly state: GameState;
  /** Observes status after one engine animation frame. */
  afterEngineFrame(): void;
  /** Removes listeners and stops the timer DOM loop. */
  teardown(): void;
};

/** Faces by their standard letter, shared by keys and `data-face`. */
const FACE_BY_LETTER: Readonly<Record<string, CubeFace>> = {
  r: CubeFace.Right,
  l: CubeFace.Left,
  u: CubeFace.Up,
  d: CubeFace.Down,
  f: CubeFace.Front,
  b: CubeFace.Back,
};

const VIEW_BY_NAME: Readonly<Record<string, CubeViewMode>> = {
  '3d': CubeViewMode.Cube3D,
  both: CubeViewMode.Both,
  '2d': CubeViewMode.Flat,
};

const VIEW_NAME_BY_MODE: Readonly<Record<CubeViewMode, string>> = {
  [CubeViewMode.Cube3D]: '3d',
  [CubeViewMode.Both]: 'both',
  [CubeViewMode.Flat]: '2d',
};

const FLAT_BY_NAME: Readonly<Record<string, CubeFlatStyle>> = {
  net: CubeFlatStyle.Net,
  rings: CubeFlatStyle.Rings,
  both: CubeFlatStyle.Both,
};

const FLAT_NAME_BY_STYLE: Readonly<Record<CubeFlatStyle, string>> = {
  [CubeFlatStyle.Net]: 'net',
  [CubeFlatStyle.Rings]: 'rings',
  [CubeFlatStyle.Both]: 'both',
};

/** Produces a uint32 seed using Web Crypto. */
export function randomSeed(): number {
  const value = new Uint32Array(1);
  crypto.getRandomValues(value);
  return value[0] ?? 0;
}

/** Returns whether a keyboard event belongs to an editable control. */
function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) return false;
  return (
    target.isContentEditable ||
    target.matches('input, textarea, select, option')
  );
}

/** Parses a data-view value into the primitive engine enum. */
function viewModeOf(button: HTMLButtonElement): CubeViewMode | null {
  return VIEW_BY_NAME[button.dataset.view ?? ''] ?? null;
}

/** Parses a data-flat value into the primitive engine enum. */
function flatStyleOf(button: HTMLButtonElement): CubeFlatStyle | null {
  return FLAT_BY_NAME[button.dataset.flat ?? ''] ?? null;
}

/**
 * Wires one group of choice buttons, each of which names a value.
 *
 * The view regions and the flat style are two such groups and were two copies
 * of the same thirteen lines, differing only in how a button names its value
 * and what is done with it.
 */
function bindChoices<T>(
  buttons: readonly HTMLButtonElement[],
  parse: (button: HTMLButtonElement) => T | null,
  choose: (value: T) => void,
): Map<HTMLButtonElement, () => void> {
  const listeners = new Map<HTMLButtonElement, () => void>();
  for (const button of buttons) {
    const listener = (): void => {
      const value = parse(button);
      if (value === null) return;
      choose(value);
    };
    listeners.set(button, listener);
    button.addEventListener('click', listener);
  }
  return listeners;
}

/** Parses a DOM move button into one typed face turn. */
function moveOf(
  button: HTMLButtonElement,
): { readonly face: CubeFace; readonly turns: FaceTurns } | null {
  const face = FACE_BY_LETTER[(button.dataset.face ?? '').toLowerCase()];
  const turns = Number(button.dataset.turn);
  if (face === undefined || (turns !== -1 && turns !== 1 && turns !== 2)) {
    return null;
  }
  return { face, turns };
}

/**
 * Connects DOM controls, keyboard commands, timer state, and one engine.
 *
 * Every listener is installed here and removed by the returned controller,
 * keeping AppLifecycle's single teardown path intact.
 */
export function attachGameController(
  options: GameControllerOptions,
): GameController {
  const { engine, ui, startFrameLoop, onError } = options;
  const seedSource = options.seedSource ?? randomSeed;
  const keyboardTarget: KeyboardTarget = options.keyboardTarget ?? {
    addEventListener: (_type, listener): void => {
      window.addEventListener('keydown', listener);
    },
    removeEventListener: (_type, listener): void => {
      window.removeEventListener('keydown', listener);
    },
  };

  let active = true;
  const session = new GameSession({
    engine,
    ui,
    timerEnvironment: options.timerEnvironment,
  });

  /**
   * Every control that is simply on while a controller is attached.
   *
   * The move buttons are deliberately not here: whether a layer can be turned
   * by hand is the engine's answer rather than the controller's, and
   * `updateMoveAvailability` is what carries it. Listed once so that adding a
   * control cannot enable it without disabling it again at teardown.
   */
  const commands: readonly (HTMLButtonElement | HTMLInputElement)[] = [
    ui.scrambleMovesInput,
    ui.scrambleButton,
    ui.resetButton,
    ui.homeViewButton,
    ...ui.viewButtons,
    ...ui.flatButtons,
  ];

  const setCommandsDisabled = (disabled: boolean): void => {
    for (const control of commands) control.disabled = disabled;
  };

  const updateViewControls = (): void => {
    const selected = engine.viewMode();
    for (const button of ui.viewButtons) {
      const mode = viewModeOf(button);
      button.setAttribute('aria-pressed', String(mode === selected));
    }
    ui.canvas.dataset.viewMode = VIEW_NAME_BY_MODE[selected];

    const style = engine.flatStyle();
    for (const button of ui.flatButtons) {
      const value = flatStyleOf(button);
      button.setAttribute('aria-pressed', String(value === style));
      // The toggle only means something while the flat region is on screen.
      button.hidden = selected === CubeViewMode.Cube3D;
    }
    ui.canvas.dataset.flatStyle = FLAT_NAME_BY_STYLE[style];
  };

  // What the move buttons were last set to. Written only on a change: this
  // runs once per frame, and a played sequence keeps frames coming for
  // seconds at a time with the answer the same throughout.
  let movesDisabled: boolean | null = null;

  // `busy` is passed in on the frame path, where it has already been read for
  // the session; the command paths have nobody to take it from and ask here.
  const updateMoveAvailability = (busy = engine.isBusy()): void => {
    if (busy === movesDisabled) return;

    movesDisabled = busy;
    for (const button of ui.moveButtons) button.disabled = busy;
  };

  const run = (command: () => void): void => {
    if (!active) return;
    try {
      command();
    } catch (error) {
      onError(error);
    }
  };

  const startFaceTurn = (face: CubeFace, turns: FaceTurns): void => {
    run((): void => {
      if (!engine.turnFace(face, turns)) return;
      updateMoveAvailability();
      startFrameLoop();
    });
  };

  // The last value the box held that the engine would accept. Kept here so a
  // refused edit has something to go back to.
  let scrambleMoves = DEFAULT_SCRAMBLE_MOVES;
  ui.scrambleMovesInput.value = String(scrambleMoves);

  /**
   * Marks the box as having just refused an edit.
   *
   * Not `aria-invalid`: the value put back is a valid one, so the field is not
   * in an invalid state -- what happened is an event, and this is how long it
   * stays visible. It has to be visible on its own because the spoken half of
   * the refusal goes to the shared status line, and pressing Scramble right
   * after a refusal overwrites that line in the same interaction.
   *
   * Cleared out and re-set so that refusing twice running shows twice; setting
   * an attribute that is already there restarts no animation.
   */
  const flashRefusal = (): void => {
    const box = ui.scrambleMovesInput;
    delete box.dataset.refused;
    void box.offsetWidth;
    box.dataset.refused = '';
  };

  // The mark lasts as long as its animation, which is why one always runs --
  // the reduced-motion variant fades instead of moving rather than not being
  // there, so this always arrives and the mark can never stick.
  const onRefusalFlashEnd = (): void => {
    delete ui.scrambleMovesInput.dataset.refused;
  };

  const onScrambleMovesChange = (): void => {
    const typed = Number(ui.scrambleMovesInput.value);
    if (isValidScrambleMoves(typed)) {
      scrambleMoves = typed;
      // A correction inside the flash puts it out early: the box is right
      // again, and a mark still burning would be describing the last edit.
      delete ui.scrambleMovesInput.dataset.refused;
      return;
    }

    // Put back rather than clamped: a hundred is not what someone typing a
    // thousand meant, and quietly substituting it hides the mistake.
    ui.scrambleMovesInput.value = String(scrambleMoves);
    flashRefusal();
    session.announce(
      `A scramble is 1 to ${MAX_SCRAMBLE_MOVES} moves. Kept ${scrambleMoves}.`,
    );
  };

  const onScramble = (): void => {
    run((): void => {
      engine.scramble(seedSource(), scrambleMoves);
      session.beginScramble();
      updateMoveAvailability();

      // The cube is still solved: the scramble is turned into it over the
      // frames that follow, and nothing else is running to ask for them. No
      // drawing here -- unlike a reset, the first of those frames is along
      // immediately and would only repaint what this one had just drawn.
      startFrameLoop();
    });
  };

  const onReset = (): void => {
    run((): void => {
      engine.resetCube();
      engine.render();
      session.restart();
      updateMoveAvailability();
    });
  };

  const onHomeView = (): void => {
    run((): void => {
      engine.resetView();
      engine.render();
      session.announce('View returned home.');
    });
  };

  // All three groups are buttons that name a value, so all three go through
  // the same binder and land in one map for teardown to walk.
  const choiceListeners = new Map<HTMLButtonElement, () => void>([
    ...bindChoices(ui.viewButtons, viewModeOf, (mode) => {
      run((): void => {
        engine.setViewMode(mode);
        engine.render();
        updateViewControls();
      });
    }),
    ...bindChoices(ui.flatButtons, flatStyleOf, (style) => {
      run((): void => {
        engine.setFlatStyle(style);
        engine.render();
        updateViewControls();
      });
    }),
    ...bindChoices(ui.moveButtons, moveOf, (move) => {
      startFaceTurn(move.face, move.turns);
    }),
  ]);

  const onKeyDown = (event: KeyboardEvent): void => {
    if (
      event.repeat ||
      event.altKey ||
      event.ctrlKey ||
      event.metaKey ||
      isEditableTarget(event.target)
    ) {
      return;
    }

    const face = FACE_BY_LETTER[event.key.toLowerCase()];
    if (face === undefined) return;

    event.preventDefault();
    startFaceTurn(face, event.shiftKey ? -1 : 1);
  };

  ui.scrambleMovesInput.addEventListener('change', onScrambleMovesChange);
  ui.scrambleMovesInput.addEventListener('animationend', onRefusalFlashEnd);
  ui.scrambleButton.addEventListener('click', onScramble);
  ui.resetButton.addEventListener('click', onReset);
  ui.homeViewButton.addEventListener('click', onHomeView);
  keyboardTarget.addEventListener('keydown', onKeyDown);

  setCommandsDisabled(false);
  updateViewControls();
  updateMoveAvailability();

  return {
    get state(): GameState {
      return session.state;
    },

    afterEngineFrame(): void {
      if (!active) return;
      run((): void => {
        // One reading of the engine for the two things that want it, so the
        // status line and the controls cannot describe different frames.
        const busy = engine.isBusy();
        session.observe(busy);
        updateMoveAvailability(busy);
      });
    },

    teardown(): void {
      if (!active) return;
      active = false;
      session.teardown();
      setCommandsDisabled(true);
      // The one moment the move buttons are not the engine's to decide.
      for (const button of ui.moveButtons) button.disabled = true;
      ui.scrambleMovesInput.removeEventListener(
        'change',
        onScrambleMovesChange,
      );
      ui.scrambleMovesInput.removeEventListener(
        'animationend',
        onRefusalFlashEnd,
      );
      ui.scrambleButton.removeEventListener('click', onScramble);
      ui.resetButton.removeEventListener('click', onReset);
      ui.homeViewButton.removeEventListener('click', onHomeView);
      for (const [button, listener] of choiceListeners) {
        button.removeEventListener('click', listener);
      }
      keyboardTarget.removeEventListener('keydown', onKeyDown);
    },
  };
}
