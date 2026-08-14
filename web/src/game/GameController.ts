import {
  CubeFace,
  CubeFlatStyle,
  CubeViewMode,
  DEFAULT_SCRAMBLE_MOVES,
  type FaceTurns,
} from '../wasm/CubeEngine.ts';
import {
  formatElapsed,
  SolveTimer,
  type TimerEnvironment,
} from './SolveTimer.ts';

/**
 * One local solve session inside a ready application.
 *
 * `scrambling` is the sequence being played into the cube. Nothing can be
 * turned by hand during it and the clock is not yet armed, so it is a state of
 * its own rather than an early `ready`.
 */
export type GameState =
  | 'idle'
  | 'scrambling'
  | 'ready'
  | 'running'
  | 'completed';

/** Engine surface required by gameplay controls. */
export type GameEngine = {
  scramble(seed: number, moveCount: number): void;
  resetCube(): void;
  isSolved(): boolean;
  committedMoveCount(): number;
  turnFace(face: CubeFace, faceTurns: FaceTurns): boolean;
  setViewMode(mode: CubeViewMode): void;
  viewMode(): CubeViewMode;
  setFlatStyle(style: CubeFlatStyle): void;
  flatStyle(): CubeFlatStyle;
  resetView(): void;
  isBusy(): boolean;
  render(): void;
};

/** Typed DOM elements owned by the gameplay controller. */
export type GameUi = {
  readonly root: HTMLElement;
  readonly canvas: HTMLCanvasElement;
  readonly timer: HTMLOutputElement;
  readonly status: HTMLParagraphElement;
  readonly scrambleButton: HTMLButtonElement;
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
  let gameState: GameState = 'idle';
  let previousMoveCount = engine.committedMoveCount();

  // Assigning value on an <output> publishes the text too, so the DOM only
  // has to be written once per tick.
  const timer = new SolveTimer((elapsedMs: number): void => {
    ui.timer.value = formatElapsed(elapsedMs);
  }, options.timerEnvironment);

  const setGameState = (state: GameState): void => {
    gameState = state;
    ui.root.dataset.gameState = state;
  };

  const announce = (message: string): void => {
    ui.status.textContent = message;
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

  const updateMoveAvailability = (): void => {
    const disabled = engine.isBusy();
    for (const button of ui.moveButtons) button.disabled = disabled;
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

  const onScramble = (): void => {
    run((): void => {
      engine.scramble(seedSource(), DEFAULT_SCRAMBLE_MOVES);
      engine.render();
      previousMoveCount = 0;
      timer.reset();
      setGameState('scrambling');
      updateMoveAvailability();
      announce('Scrambling the cube…');

      // The cube is still solved: the scramble is turned into it over the
      // frames that follow, and nothing else is running to ask for them.
      startFrameLoop();
    });
  };

  const onReset = (): void => {
    run((): void => {
      engine.resetCube();
      engine.render();
      previousMoveCount = 0;
      timer.reset();
      setGameState('idle');
      updateMoveAvailability();
      announce('Cube reset.');
    });
  };

  const onHomeView = (): void => {
    run((): void => {
      engine.resetView();
      engine.render();
      announce('View returned home.');
    });
  };

  const viewListeners = bindChoices(ui.viewButtons, viewModeOf, (mode) => {
    run((): void => {
      engine.setViewMode(mode);
      engine.render();
      updateViewControls();
    });
  });

  const flatListeners = bindChoices(ui.flatButtons, flatStyleOf, (style) => {
    run((): void => {
      engine.setFlatStyle(style);
      engine.render();
      updateViewControls();
    });
  });

  const moveListeners = new Map<HTMLButtonElement, () => void>();
  for (const button of ui.moveButtons) {
    const listener = (): void => {
      const move = moveOf(button);
      if (move !== null) startFaceTurn(move.face, move.turns);
    };
    moveListeners.set(button, listener);
    button.addEventListener('click', listener);
  }

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

  ui.scrambleButton.addEventListener('click', onScramble);
  ui.resetButton.addEventListener('click', onReset);
  ui.homeViewButton.addEventListener('click', onHomeView);
  keyboardTarget.addEventListener('keydown', onKeyDown);

  timer.reset();
  setGameState('idle');
  ui.scrambleButton.disabled = false;
  ui.resetButton.disabled = false;
  ui.homeViewButton.disabled = false;
  for (const button of ui.viewButtons) button.disabled = false;
  for (const button of ui.flatButtons) button.disabled = false;
  updateViewControls();
  updateMoveAvailability();

  return {
    get state(): GameState {
      return gameState;
    },

    afterEngineFrame(): void {
      if (!active) return;
      run((): void => {
        const moveCount = engine.committedMoveCount();
        const committed = moveCount > previousMoveCount;
        previousMoveCount = moveCount;

        // Asked as a state rather than as a change, so a one-move scramble
        // that has already finished by the first observed frame is still seen
        // to finish. That the call was accepted is what says it began.
        if (gameState === 'scrambling' && !engine.isBusy()) {
          timer.arm();
          setGameState('ready');
          announce('Scramble ready. The timer starts after your first move.');
        }

        if (committed && gameState === 'ready') {
          timer.start();
          setGameState('running');
        }

        if (
          committed &&
          gameState === 'running' &&
          engine.isSolved()
        ) {
          const finalMs = timer.stop();
          setGameState('completed');
          announce(`Solved in ${formatElapsed(finalMs)}.`);
        }

        updateMoveAvailability();
      });
    },

    teardown(): void {
      if (!active) return;
      active = false;
      timer.teardown();
      ui.scrambleButton.disabled = true;
      ui.resetButton.disabled = true;
      ui.homeViewButton.disabled = true;
      for (const button of ui.viewButtons) button.disabled = true;
      for (const button of ui.flatButtons) button.disabled = true;
      for (const button of ui.moveButtons) button.disabled = true;
      ui.scrambleButton.removeEventListener('click', onScramble);
      ui.resetButton.removeEventListener('click', onReset);
      ui.homeViewButton.removeEventListener('click', onHomeView);
      for (const [button, listener] of viewListeners) {
        button.removeEventListener('click', listener);
      }
      for (const [button, listener] of flatListeners) {
        button.removeEventListener('click', listener);
      }
      for (const [button, listener] of moveListeners) {
        button.removeEventListener('click', listener);
      }
      keyboardTarget.removeEventListener('keydown', onKeyDown);
    },
  };
}
