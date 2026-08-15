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
  ambientStart(choice: number): boolean;
  ambientStop(): void;
  isAmbient(): boolean;
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
  /** Starts and stops watching; pressed while a pattern is running. */
  readonly ambientButton: HTMLButtonElement;
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
  /**
   * One arbitrary uint32 per call.
   *
   * Both things the engine cannot decide for itself take one: which scramble
   * to make, and which pattern to watch. They ask at different moments and
   * never share an answer, so one source serves both.
   */
  readonly randomSource?: () => number;
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

/** Produces one arbitrary uint32 using Web Crypto. */
export function randomUint32(): number {
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
  const randomSource = options.randomSource ?? randomUint32;
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
   * The move buttons and the watch toggle are deliberately not here: whether a
   * layer can be turned by hand is the engine's answer and whether a pattern
   * can be watched is the session's, and `updateMoveAvailability` and
   * `updateAmbientControl` are what carry them. Listed once so that adding a
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

  const run = (action: () => void): void => {
    if (!active) return;
    try {
      action();
    } catch (error) {
      onError(error);
    }
  };

  /**
   * The states a session can be watched from: not one that is under way.
   *
   * With the timer armed or running, an interlude that takes the cube away and
   * gives it back is a stretch of a solve that nothing can account for, so it
   * is not offered rather than being quietly harmless.
   */
  const WATCHABLE: ReadonlySet<GameState> = new Set<GameState>([
    'idle',
    'completed',
  ]);

  /** What the engine says about itself, for the controls that follow it. */
  type EngineNow = { readonly busy: boolean; readonly watching: boolean };

  const engineNow = (): EngineNow => ({
    busy: engine.isBusy(),
    watching: engine.isAmbient(),
  });

  // What the move buttons were last set to. Written only on a change: this
  // runs once per frame, and a played sequence keeps frames coming for
  // seconds at a time with the answer the same throughout.
  let movesDisabled: boolean | null = null;


  /**
   * Puts the controls that follow the engine into the state it is in.
   *
   * One reading for all of them, so they cannot describe different moments.
   * The frame path has already taken it for the session and hands it in; the
   * command paths have nobody to take it from and ask here.
   */
  const updateEngineControls = (now = engineNow()): void => {
    // Watching makes the engine busy, but a press on a move button stops the
    // watching and then turns -- exactly as the same letter on the keyboard
    // does. So the buttons stay live for it, where anything else that made the
    // engine busy would put them out.
    const movesOff = now.busy && !now.watching;
    if (movesOff !== movesDisabled) {
      movesDisabled = movesOff;
      for (const button of ui.moveButtons) button.disabled = movesOff;
    }

    ui.ambientButton.setAttribute('aria-pressed', String(now.watching));
    ui.ambientButton.disabled =
      !now.watching && !WATCHABLE.has(session.state);
  };

  /**
   * Ends watching, if it is on, and puts the restored cube on the screen.
   *
   * Drawing here rather than leaving it to the frame loop that watching keeps
   * running: that loop is stopped while the tab is away, and the cube nobody
   * drew would still be showing the pattern's last position when it came back.
   *
   * @returns whether there was anything to stop.
   */
  const leaveAmbient = (): boolean => {
    if (!engine.isAmbient()) return false;

    engine.ambientStop();
    engine.render();
    // Whatever asked for this may have something of its own to say, and says
    // it over this one.
    session.announce('Watching stopped.');
    return true;
  };

  /**
   * Runs a control that acts on the cube, ending any watching first.
   *
   * The line this draws is the same one the engine draws for a press, and the
   * same one a scramble has always been on the other side of: changing the
   * cube is a command and watching gives way to it, while changing how the
   * cube is looked at is not, and watching carries on through it. Which is
   * why the view and style buttons do not come through here -- they leave a
   * scramble playing, and there is no reason a pattern should fare worse.
   *
   * The watch toggle is the one control that acts on the cube and still does
   * not come through here: stopping first would turn a press meant to end
   * watching into ending it and starting it again.
   */
  const cubeCommand = (action: () => void): void => {
    run((): void => {
      leaveAmbient();
      action();
      updateEngineControls();
    });
  };

  const startFaceTurn = (face: CubeFace, turns: FaceTurns): void => {
    cubeCommand((): void => {
      if (engine.turnFace(face, turns)) startFrameLoop();
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
    cubeCommand((): void => {
      engine.scramble(randomSource(), scrambleMoves);
      session.beginScramble();

      // The cube is still solved: the scramble is turned into it over the
      // frames that follow, and nothing else is running to ask for them. No
      // drawing here -- unlike a reset, the first of those frames is along
      // immediately and would only repaint what this one had just drawn.
      startFrameLoop();
    });
  };

  const onReset = (): void => {
    cubeCommand((): void => {
      engine.resetCube();
      engine.render();
      session.restart();
    });
  };

  /**
   * The watch toggle, which is the one control that does not leave first.
   *
   * Pressing it while a pattern runs means stop, and stopping is the whole of
   * what it does then: putting it through the ordinary command path would stop
   * the watching and then start it again in the same press.
   */
  const onAmbient = (): void => {
    run((): void => {
      if (!leaveAmbient() && engine.ambientStart(randomSource())) {
        session.announce('Watching. Look around freely; press Watch to stop.');
        // Nothing else is running to ask for frames, and a pattern that is
        // never over asks for them until it is stopped.
        startFrameLoop();
      }

      updateEngineControls();
    });
  };

  // Where the camera is put back, which is a way of looking rather than a
  // command: it leaves a scramble playing and it leaves a pattern watched.
  const onHomeView = (): void => {
    run((): void => {
      engine.resetView();
      engine.render();
      session.announce('View returned home.');
    });
  };

  // All three groups are buttons that name a value, so all three go through
  // the same binder and land in one map for teardown to walk. Only the last
  // of them acts on the cube.
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
  ui.ambientButton.addEventListener('click', onAmbient);
  ui.homeViewButton.addEventListener('click', onHomeView);
  keyboardTarget.addEventListener('keydown', onKeyDown);

  setCommandsDisabled(false);
  updateViewControls();
  updateEngineControls();

  return {
    get state(): GameState {
      return session.state;
    },

    afterEngineFrame(): void {
      if (!active) return;
      run((): void => {
        // One reading of the engine for everything that wants it, so the
        // status line and the controls cannot describe different frames. The
        // session goes first: whether watching can be offered follows the
        // state this frame may just have moved it to.
        const now = engineNow();
        session.observe(now.busy);
        updateEngineControls(now);
      });
    },

    teardown(): void {
      if (!active) return;
      active = false;
      session.teardown();
      setCommandsDisabled(true);
      // The one moment the move buttons and the watch toggle are not the
      // engine's and the session's to decide.
      for (const button of [...ui.moveButtons, ui.ambientButton]) {
        button.disabled = true;
      }
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
      ui.ambientButton.removeEventListener('click', onAmbient);
      ui.homeViewButton.removeEventListener('click', onHomeView);
      for (const [button, listener] of choiceListeners) {
        button.removeEventListener('click', listener);
      }
      keyboardTarget.removeEventListener('keydown', onKeyDown);
    },
  };
}
