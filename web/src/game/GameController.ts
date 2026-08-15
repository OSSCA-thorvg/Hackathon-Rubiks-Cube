import {
  CubeFace,
  CubeFlatStyle,
  CubePalette,
  CubeViewMode,
  DEFAULT_SCRAMBLE_MOVES,
  isValidScrambleMoves,
  MAX_SCRAMBLE_MOVES,
  type FaceTurns,
} from '../wasm/CubeEngine.ts';
import { createClickSound, type ClickSound } from './ClickSound.ts';
import { MoveLog } from './MoveLog.ts';
import {
  GameSession,
  type EngineFrame,
  type GameState,
  type SessionEngine,
} from './GameSession.ts';
import { SessionRecords, type SolveRecord } from './SessionRecords.ts';
import { encodeSession, type SharedSession } from './shareCode.ts';
import { shareUrl } from './shareLink.ts';
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
  undo(): boolean;
  redo(): boolean;
  solveRewind(): boolean;
  stopPlayback(): void;
  timelineLength(): number;
  timelineScrambleEnd(): number;
  timelineMove(index: number): number;
  turnFace(face: CubeFace, faceTurns: FaceTurns): boolean;
  setViewMode(mode: CubeViewMode): void;
  viewMode(): CubeViewMode;
  setFlatStyle(style: CubeFlatStyle): void;
  flatStyle(): CubeFlatStyle;
  setPalette(palette: CubePalette): void;
  palette(): CubePalette;
  setSpeedScale(scale: number): void;
  speedScale(): number;
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
  /** Takes back one move of the user's own, and puts it back again. */
  readonly undoButton: HTMLButtonElement;
  readonly redoButton: HTMLButtonElement;
  /** Rewinds every applied move; the long form of Undo. */
  readonly solveButton: HTMLButtonElement;
  /** Breaks a rewind off, and is only on screen while one is playing. */
  readonly stopButton: HTMLButtonElement;
  /** Copies a link that opens this cube; out of reach when there is none. */
  readonly shareButton: HTMLButtonElement;
  /** The fastest solve of the sitting, written out in one line. */
  readonly recordBest: HTMLElement;
  /** The latest few solves, newest first. */
  readonly recordList: HTMLOListElement;
  /** Where the record is written out; the log owns everything inside it. */
  readonly moveLogList: HTMLOListElement;
  /** Starts and stops watching; pressed while a pattern is running. */
  readonly ambientButton: HTMLButtonElement;
  readonly homeViewButton: HTMLButtonElement;
  readonly viewButtons: readonly HTMLButtonElement[];
  readonly flatButtons: readonly HTMLButtonElement[];
  /** Which of the two verified sticker sets the cube is drawn in. */
  readonly paletteButtons: readonly HTMLButtonElement[];
  /** Silences the turn sound; pressed means silent. */
  readonly muteButton: HTMLButtonElement;
  /** How much faster than the written tempos every animation runs. */
  readonly speedInput: HTMLInputElement;
  /** What the slider currently means, written out beside it. */
  readonly speedValue: HTMLOutputElement;
  readonly moveButtons: readonly HTMLButtonElement[];
};

/**
 * Where a shared link is built from and where it is put; injectable for tests.
 *
 * Two capabilities rather than the two globals they come from, because a test
 * has neither: `navigator.clipboard` is absent outside a secure context, and
 * the page a test runs on is not the page a link should point at.
 */
export type ShareTarget = {
  /** The address of this page. */
  currentUrl(): string;
  /** Puts one string on the clipboard, or rejects when it cannot. */
  copy(text: string): Promise<void>;
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
  /** Where a share goes; production uses the address bar and the clipboard. */
  readonly shareTarget?: ShareTarget;
  /**
   * Whether the person has asked for less motion. Injectable so a test can
   * say so without a media query.
   */
  readonly prefersReducedMotion?: boolean;
  /**
   * The sound a committed turn makes. Injected whole rather than configured,
   * so a test hears a counter and jsdom is never asked for an AudioContext.
   */
  readonly sound?: ClickSound;
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

/** Where the slider opens for someone who asked their system for less. */
const REDUCED_MOTION_SPEED = 2;

const PALETTE_BY_NAME: Readonly<Record<string, CubePalette>> = {
  classic: CubePalette.Classic,
  'high-contrast': CubePalette.HighContrast,
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

/** Parses a data-palette value into the primitive engine enum. */
function paletteOf(button: HTMLButtonElement): CubePalette | null {
  return PALETTE_BY_NAME[button.dataset.palette ?? ''] ?? null;
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
  const shareTarget: ShareTarget = options.shareTarget ?? {
    currentUrl: (): string => window.location.href,
    copy: (text: string): Promise<void> =>
      // Absent outside a secure context, which is a refusal like any other:
      // sharing is an extra, so being unable to copy is something to say
      // rather than a failure that takes the application down.
      navigator.clipboard === undefined
        ? Promise.reject(new Error('No clipboard here.'))
        : navigator.clipboard.writeText(text),
  };
  const keyboardTarget: KeyboardTarget = options.keyboardTarget ?? {
    addEventListener: (_type, listener): void => {
      window.addEventListener('keydown', listener);
    },
    removeEventListener: (_type, listener): void => {
      window.removeEventListener('keydown', listener);
    },
  };

  let active = true;
  const sound = options.sound ?? createClickSound();

  /**
   * Runs one setup step, releasing the sound if it does not survive it.
   *
   * The sound is listening for a gesture from the moment it is made, which is
   * before there is a controller to hand back. So a step that throws leaves
   * nobody holding it: the caller sees an exception and has no teardown to
   * call, and the listeners would sit on the window for the life of the page
   * waiting to open an audio context nothing would ever close.
   */
  const setup = <T>(step: () => T): T => {
    try {
      return step();
    } catch (error) {
      sound.teardown();
      throw error;
    }
  };

  const moveLog = setup(() => new MoveLog(ui.moveLogList, engine));
  const records = setup(
    () => new SessionRecords(ui.recordBest, ui.recordList),
  );
  const session = setup(
    () =>
      new GameSession({
        engine,
        ui,
        timerEnvironment: options.timerEnvironment,
        sound,
        recordSolve: (record: SolveRecord): boolean => records.add(record),
      }),
  );

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
    // In here rather than among the controls that follow the engine: a
    // palette is not a command to the cube, so it stays live while one is
    // playing. Reading the board is most wanted exactly while it moves.
    ...ui.paletteButtons,
    ui.muteButton,
    ui.speedInput,
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

  const updatePaletteControls = (): void => {
    const selected = engine.palette();
    for (const button of ui.paletteButtons) {
      const value = paletteOf(button);
      button.setAttribute('aria-pressed', String(value === selected));
    }
  };

  /**
   * Writes out what the slider means, from the engine rather than the input.
   *
   * The engine clamps, so a value typed past the end comes back as the end,
   * and reading it back is what keeps the number under the handle honest.
   */
  const updateSpeedControl = (): void => {
    const scale = engine.speedScale();
    ui.speedInput.value = String(scale);
    ui.speedValue.value = `${scale.toFixed(2)}×`;
  };

  const onSpeedChange = (): void => {
    run((): void => {
      const typed = Number(ui.speedInput.value);
      // A range input cannot produce anything else, but it is the one control
      // here whose value is a number rather than a name, and the engine
      // refuses what is not one.
      if (!Number.isFinite(typed)) {
        updateSpeedControl();
        return;
      }

      engine.setSpeedScale(typed);
      updateSpeedControl();
    });
  };

  /**
   * The speed to open at, which is not always the written one.
   *
   * Someone who has asked their system for less motion is offered the quick
   * end rather than being given it: the animation is what this app is for, so
   * it is not taken away, and the slider is right there to put back. A setting
   * that is a suggestion has to remain one.
   */
  const initialSpeedScale = (): number => {
    const reduced =
      options.prefersReducedMotion ??
      (typeof window !== 'undefined' &&
        typeof window.matchMedia === 'function' &&
        window.matchMedia('(prefers-reduced-motion: reduce)').matches);
    return reduced ? REDUCED_MOTION_SPEED : 1;
  };

  const updateMuteControl = (): void => {
    ui.muteButton.setAttribute('aria-pressed', String(sound.isMuted()));
  };

  const onMute = (): void => {
    sound.setMuted(!sound.isMuted());
    updateMuteControl();
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

  /** What the engine says about itself, for everything that follows it. */
  const engineNow = (): EngineFrame => ({
    busy: engine.isBusy(),
    watching: engine.isAmbient(),
    cursor: engine.timelineCursor(),
    length: engine.timelineLength(),
    scrambleEnd: engine.timelineScrambleEnd(),
    userMoves: engine.committedMoveCount(),
  });

  // What the move buttons were last set to. Written only on a change: this
  // runs once per frame, and a played sequence keeps frames coming for
  // seconds at a time with the answer the same throughout.
  let movesDisabled: boolean | null = null;

  /**
   * Whether a rewind this controller asked for is still playing.
   *
   * Only what puts the Stop control on screen. Whether a press on it does
   * anything is the engine's answer and not this one, so a press arriving in
   * the frame between a rewind ending and this being cleared reaches a command
   * that refuses it.
   */
  let rewinding = false;

  /**
   * Whether there is a state to send, from the numbers a frame already has.
   *
   * A predicate rather than the session itself, because this is asked once a
   * frame and the answer is four numbers wide. Reading the record to answer it
   * would cross the boundary once per recorded move, every frame, for a
   * boolean -- and the frames keep coming through an orbit sweep, which is not
   * busy, and through a watched pattern, which is.
   *
   * Watching is not in the way, for the same reason it is not in the way of a
   * move button: it leaves the record untouched, and a press ends it first.
   * Anything else being played is, because the middle of a sequence is a cube
   * nobody has been handed yet. A cursor of nothing is a cube a link would
   * have opened anyway, and a cursor below the scramble boundary is a solve
   * stopped inside the scramble -- a state the payload has no shape for.
   */
  const canShare = (now: EngineFrame): boolean =>
    (!now.busy || now.watching) &&
    now.cursor > 0 &&
    now.cursor >= now.scrambleEnd;

  /**
   * The session a link carries, read off the record when one is asked for.
   *
   * Read rather than remembered: the scramble is everything below the boundary
   * and the user's own moves are what sits between the boundary and the
   * cursor, so a tail the sender has rewound behind is left out by where the
   * reading stops rather than by a rule against it. What the far side gets is
   * the cube as it stands, and undo and solve work through everything it was
   * given -- but the moves their sender took back are not theirs to put back,
   * because they are not part of a cube.
   */
  const sharableNow = (now: EngineFrame): SharedSession => ({
    scramble: recordedBetween(0, now.scrambleEnd),
    user: recordedBetween(now.scrambleEnd, now.cursor),
  });

  /** The recorded moves in a half-open stretch, packed as the engine has them. */
  const recordedBetween = (from: number, to: number): number[] => {
    const moves: number[] = [];
    for (let index = from; index < to; index += 1) {
      moves.push(engine.timelineMove(index));
    }
    return moves;
  };

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

    // The same three conditions the engine refuses on, so a button that can be
    // pressed is one that will be answered. Undo stopping at the end of the
    // scramble is the whole of "undo is for your own moves": on a cube that
    // has only been scrambled there is nothing above that line.
    ui.undoButton.disabled = movesOff || now.cursor <= now.scrambleEnd;
    ui.redoButton.disabled = movesOff || now.cursor >= now.length;
    ui.solveButton.disabled = movesOff || now.cursor === 0;

    ui.shareButton.disabled = !canShare(now);

    // Off the screen and out of reach together: it is not a control that is
    // sometimes unavailable but one that only exists while there is a rewind
    // to break off, and the two attributes are how that is said to a person
    // looking and to a person tabbing.
    if (!now.busy) rewinding = false;
    ui.stopButton.hidden = !rewinding;
    ui.stopButton.disabled = !rewinding;

    // The same reading the controls are set from, so the list can never be
    // describing a different moment than the buttons above it. It draws only
    // when the record has changed, which is what makes calling it every frame
    // and after every command the simple thing to do.
    moveLog.update(now);

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
   * Runs one of the three rewinds, which differ only in what they ask for.
   *
   * Each is a cube command like any other -- watching gives way to it -- and
   * each hands a sequence to the engine that the frame loop then plays, so
   * what is left to do here is ask for the frames and show the way out.
   */
  const rewindCommand = (start: () => boolean, announcement?: string): void => {
    cubeCommand((): void => {
      if (!start()) return;

      rewinding = true;
      if (announcement !== undefined) session.announce(announcement);
      startFrameLoop();
    });
  };

  const onUndo = (): void => {
    rewindCommand(() => engine.undo());
  };

  const onRedo = (): void => {
    rewindCommand(() => engine.redo());
  };

  const onSolve = (): void => {
    rewindCommand(
      () => engine.solveRewind(),
      'Rewinding to the solved cube. Press Stop to break off.',
    );
  };

  /**
   * Breaks off a rewind, which is the one command that does not leave first.
   *
   * Nothing is drawn here: the sequence being stopped is what was asking for
   * frames, and the loop draws once more after the engine stops asking -- so
   * the turn this confirms reaches the screen on that frame.
   */
  const onStop = (): void => {
    run((): void => {
      engine.stopPlayback();
      rewinding = false;
      session.announce('Stopped.');
      updateEngineControls();
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

  /**
   * Copies a link that opens this cube.
   *
   * A cube command, so watching gives way to it first -- what a link should
   * carry is the session, never the position an interlude happened to leave
   * the cube in. Reading the record after that is what makes the state on the
   * clipboard the state on the screen.
   *
   * The copying itself is the only thing here that finishes later, and both
   * of its endings are the same kind of news: sharing is an extra, so a
   * clipboard that refuses is something to mention rather than a fault.
   */
  const onShare = (): void => {
    cubeCommand((): void => {
      // Read after the watching has been left, so the numbers this asks about
      // are the ones the record is about to be read at.
      const now = engineNow();
      if (!canShare(now)) return;

      const encoded = encodeSession(sharableNow(now));
      if (encoded === null) {
        session.announce('This cube cannot be written into a link.');
        return;
      }

      // Guarded on the way back rather than on the way out: a controller torn
      // down while the clipboard was thinking has no status line left to
      // write to, and the elements are no longer this controller's.
      void shareTarget.copy(shareUrl(shareTarget.currentUrl(), encoded)).then(
        (): void => {
          if (active) session.announce('Link copied. It opens this cube.');
        },
        (): void => {
          if (active) session.announce('Could not copy the link.');
        },
      );
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
    ...bindChoices(ui.paletteButtons, paletteOf, (palette) => {
      run((): void => {
        engine.setPalette(palette);
        // Drawn here rather than left to the frame loop: with nothing moving
        // there is no next frame to wait for, and the cube would keep the old
        // shades until something else asked for one.
        engine.render();
        updatePaletteControls();
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
  ui.undoButton.addEventListener('click', onUndo);
  ui.redoButton.addEventListener('click', onRedo);
  ui.solveButton.addEventListener('click', onSolve);
  ui.stopButton.addEventListener('click', onStop);
  ui.shareButton.addEventListener('click', onShare);
  ui.ambientButton.addEventListener('click', onAmbient);
  ui.homeViewButton.addEventListener('click', onHomeView);
  ui.muteButton.addEventListener('click', onMute);
  // `input` rather than `change`, so the reading follows the handle while it
  // is being dragged. Nothing in flight is re-timed, so the stream is safe.
  ui.speedInput.addEventListener('input', onSpeedChange);
  keyboardTarget.addEventListener('keydown', onKeyDown);

  setup((): void => {
    setCommandsDisabled(false);
    updateViewControls();
    updatePaletteControls();
    updateMuteControl();
    engine.setSpeedScale(initialSpeedScale());
    updateSpeedControl();
    updateEngineControls();
  });

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
        session.observe(now);
        updateEngineControls(now);
      });
    },

    teardown(): void {
      if (!active) return;
      active = false;
      session.teardown();
      setCommandsDisabled(true);
      // The one moment the move buttons, the rewinds and the watch toggle are
      // not the engine's and the session's to decide.
      for (const button of [
        ...ui.moveButtons,
        ui.undoButton,
        ui.redoButton,
        ui.solveButton,
        ui.stopButton,
        ui.shareButton,
        ui.ambientButton,
      ]) {
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
      ui.undoButton.removeEventListener('click', onUndo);
      ui.redoButton.removeEventListener('click', onRedo);
      ui.solveButton.removeEventListener('click', onSolve);
      ui.stopButton.removeEventListener('click', onStop);
      ui.shareButton.removeEventListener('click', onShare);
      ui.ambientButton.removeEventListener('click', onAmbient);
      ui.homeViewButton.removeEventListener('click', onHomeView);
      ui.muteButton.removeEventListener('click', onMute);
      ui.speedInput.removeEventListener('input', onSpeedChange);
      for (const [button, listener] of choiceListeners) {
        button.removeEventListener('click', listener);
      }
      keyboardTarget.removeEventListener('keydown', onKeyDown);
      // Last, because it is the only step here that can fail: closing an audio
      // context is a browser call rather than a listener being unhooked, and
      // one that threw from the middle of this sequence would take the
      // removals below it down as well. The caller's own teardown catches it,
      // so nothing above is left half-undone.
      sound.teardown();
    },
  };
}
