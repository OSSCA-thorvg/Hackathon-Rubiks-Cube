import {
  CubeFace,
  CubeFlatStyle,
  CubePaintFault,
  CubePalette,
  CubeStickerColour,
  CubeViewMode,
  STICKER_COLOURS,
  DEFAULT_CUBE_SIZE,
  DEFAULT_SCRAMBLE_MOVES,
  isValidScrambleMoves,
  MAX_CUBE_SIZE,
  SOLVE_WARNING_CUBE_SIZE,
  MAX_SCRAMBLE_MOVES,
  MIN_CUBE_SIZE,
  type FaceTurns,
  type PaintCursor,
} from '../wasm/CubeEngine.ts';
import { createClickSound, type ClickSound } from './ClickSound.ts';
import { MoveLog } from './MoveLog.ts';
import { parseMoves, type FaceLetter, type TypedTurn } from './notation.ts';
import { Timeline, type TimelineUi } from './Timeline.ts';
import {
  GameSession,
  type EngineFrame,
  type GameState,
  type SessionEngine,
} from './GameSession.ts';
import { SessionRecords, type SolveRecord } from './SessionRecords.ts';
import { ShareCard, type ShareCardUi } from './ShareCard.ts';
import {
  encodePainting,
  encodeSession,
  type SharedSession,
} from './shareCode.ts';
import { pageUrl, shareUrl } from './shareLink.ts';
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
  canSolve(): boolean;
  solve(): boolean;
  stopPlayback(): void;
  timelineLength(): number;
  timelineScrambleEnd(): number;
  timelineMove(index: number): number;
  turnFace(
    face: CubeFace,
    firstDepth: number,
    lastDepth: number,
    faceTurns: FaceTurns,
  ): boolean;
  setCubeSize(size: number): boolean;
  cubeSize(): number;
  originPainting(): number[];
  beginPainting(): boolean;
  cancelPainting(): void;
  isPainting(): boolean;
  setBrush(colour: CubeStickerColour): void;
  brush(): CubeStickerColour;
  paintedCount(colour: CubeStickerColour): number;
  applyPainting(): boolean;
  paintFault(): CubePaintFault;
  setFilling(wholeFace: boolean): boolean;
  isFilling(): boolean;
  paintCursorStep(columns: number, rows: number): boolean;
  paintAtCursor(): boolean;
  setPaintCursorShown(shown: boolean): void;
  paintCursor(): PaintCursor | null;
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
  /**
   * The box the views sit in, which is told what is on show so the page can
   * lay the canvases out for it.
   */
  readonly stage: HTMLElement;
  readonly timer: HTMLOutputElement;
  readonly status: HTMLParagraphElement;
  readonly scrambleButton: HTMLButtonElement;
  /** How many moves the next scramble is, as a plain integer. */
  readonly scrambleMovesInput: HTMLInputElement;
  readonly resetButton: HTMLButtonElement;
  /** Which cube is on the table, as a plain integer of layers. */
  readonly cubeSizeInput: HTMLInputElement;
  /** How deep the face buttons and the keyboard letters reach. */
  readonly turnDepthInput: HTMLInputElement;
  /** Whether they take everything above that depth with them. */
  readonly turnWideButton: HTMLButtonElement;
  /** Takes back one move of the user's own, and puts it back again. */
  readonly undoButton: HTMLButtonElement;
  readonly redoButton: HTMLButtonElement;
  /** Rewinds every applied move; the long form of Undo. */
  readonly rewindButton: HTMLButtonElement;
  /** Works the cube out from where it stands and plays the answer. */
  readonly solveButton: HTMLButtonElement;
  /** Says why Solve is out of reach, when it is out of reach for good. */
  readonly solverNote: HTMLElement;
  /** Breaks a rewind off, and is only on screen while one is playing. */
  readonly stopButton: HTMLButtonElement;
  /** Copies a link that opens this cube; out of reach when there is none. */
  readonly shareButton: HTMLButtonElement;
  /** Where the link a share made is shown, by the button that made it. */
  readonly shareCard: ShareCardUi;
  /** The fastest solve of the sitting, written out in one line. */
  readonly recordBest: HTMLElement;
  /** The latest few solves, newest first. */
  readonly recordList: HTMLOListElement;
  /** How many solves the sitting has had, beside the board's title. */
  readonly recordTally: HTMLElement;
  /** Where the record is written out; the log owns everything inside it. */
  readonly moveLogList: HTMLOListElement;
  /** The record laid flat under the stage, and the scramble in the panel. */
  readonly timeline: TimelineUi;
  /** Where the size of the cube in hand is written, beside the title. */
  readonly cubeSizeLabel: HTMLElement;
  /** Starts and stops watching; pressed while a pattern is running. */
  readonly ambientButton: HTMLButtonElement;
  /** Opens and closes a draft of the cube to colour; pressed while one is open. */
  readonly paintButton: HTMLButtonElement;
  /** Everything the colouring needs, shown only while a draft is open. */
  readonly paintBar: HTMLElement;
  /**
   * The net's canvas, which takes the keyboard while a draft is open: the one
   * way to reach a square of it without a pointer.
   */
  readonly netCanvas: HTMLCanvasElement;
  readonly paintSwatches: readonly HTMLButtonElement[];
  readonly paintFillButton: HTMLButtonElement;
  readonly paintApplyButton: HTMLButtonElement;
  readonly paintCancelButton: HTMLButtonElement;
  /** What a refused colouring was refused for. */
  readonly paintNote: HTMLElement;
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
  /**
   * Whether a dialog is up, which the face letters do not reach under.
   *
   * Asked rather than worked out, because which dialogs there are and whether
   * one is open is the page's to know -- the same question the command menu
   * is given. Absent, nothing is ever in the way.
   */
  readonly blocked?: () => boolean;
};

/**
 * The event a line of typed moves arrives on, dispatched at `GameUi.root`.
 *
 * An event rather than a method, because what types the moves -- the command
 * menu -- is the page's and exists before any controller does. A line sent
 * while no controller is attached reaches nobody, which is the same as a
 * button pressed on a page whose engine never arrived.
 */
export const PLAY_MOVES_EVENT = 'thorvg-rubiks:play-moves';

/** What a `PLAY_MOVES_EVENT` carries: the line, exactly as it was typed. */
export type PlayMovesDetail = {
  readonly text: string;
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

/** The same faces by the letter typed notation writes them with. */
const FACE_BY_NOTATION: Readonly<Record<FaceLetter, CubeFace>> = {
  R: CubeFace.Right,
  L: CubeFace.Left,
  U: CubeFace.Up,
  D: CubeFace.Down,
  F: CubeFace.Front,
  B: CubeFace.Back,
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

const PALETTE_NAME_BY_VALUE: Readonly<Record<CubePalette, string>> = {
  [CubePalette.Classic]: 'classic',
  [CubePalette.HighContrast]: 'high-contrast',
};

/**
 * Somewhere the page is walking the cube, one engine command at a time.
 *
 * The engine plays one sequence at a time and has no command for "go to move
 * twelve" or "turn these five", so both are walked from here: every step is
 * an ordinary undo, redo or face turn, asked for once the one before it has
 * landed. The turns are a queue the walk eats from the front of.
 *
 * `landsAt` is where the record's cursor stands once the step in flight has
 * landed, or null before the first. Every commit moves the cursor by exactly
 * one, so a cursor anywhere else when the walk is next idle is a commit the
 * walk did not make.
 */
type Walk = (
  | { readonly kind: 'seek'; readonly target: number }
  | { readonly kind: 'turns'; readonly turns: TypedTurn[] }
) & { landsAt: number | null };

/**
 * What each refusal reads as, one sentence apiece.
 *
 * Written here rather than in the engine because a sentence is a thing the
 * page says and the engine's business is which of them is true. Every fault
 * the ABI can return has one: a number arriving without a sentence would leave
 * somebody staring at a cube they were told nothing about.
 */
const PAINT_FAULT_SENTENCE: Readonly<Record<CubePaintFault, string>> = {
  [CubePaintFault.None]: '',
  [CubePaintFault.ColourCount]:
    'One colour is on too many squares and another on too few. The tallies beside the colours say which.',
  [CubePaintFault.OrbitCount]:
    'Two squares have been given each other\u2019s colours across parts of the cube that never mix. The highlighted ones cannot be reached from one another.',
  [CubePaintFault.OppositePairs]:
    'This cube has different colours facing each other than the one here does. That is a real cube, but not one this app solves.',
  [CubePaintFault.ImpossiblePiece]:
    'The highlighted piece wears colours no piece of a real cube wears, or wears them the other way round.',
  [CubePaintFault.RepeatedPiece]:
    'Two places on the cube hold the same piece. One of the highlighted ones has been copied down twice.',
  [CubePaintFault.CornerTwist]:
    'A corner is turned in a way no sequence of moves could leave it. One of them is copied down a third of a turn out.',
  [CubePaintFault.EdgeFlip]:
    'An edge is the other way round from how a real cube can leave it. One of the highlighted ones is flipped.',
  [CubePaintFault.Permutation]:
    'The corners and the edges disagree: swapping two of one would need two of the other swapped as well.',
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

/** Parses a data-sticker value into the primitive engine colour. */
function stickerOf(button: HTMLButtonElement): CubeStickerColour | null {
  const value = Number(button.dataset.sticker);
  return STICKER_COLOURS.includes(value as CubeStickerColour)
    ? (value as CubeStickerColour)
    : null;
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

/**
 * The controls that are simply on while a controller is attached.
 *
 * The paint bar's own among them: the bar is out of sight whenever there is no
 * draft for them to act on, so there is no moment they need to be out of
 * reach as well.
 */
function commandControls(ui: GameUi): (HTMLButtonElement | HTMLInputElement)[] {
  return [
    ui.scrambleMovesInput,
    ui.scrambleButton,
    ui.resetButton,
    ui.cubeSizeInput,
    ui.homeViewButton,
    ...ui.viewButtons,
    ...ui.flatButtons,
    // In here rather than among the controls that follow the engine: a
    // palette is not a command to the cube, so it stays live while one is
    // playing. Reading the board is most wanted exactly while it moves.
    ...ui.paletteButtons,
    ui.muteButton,
    ui.speedInput,
    ...ui.paintSwatches,
    ui.paintFillButton,
    ui.paintApplyButton,
    ui.paintCancelButton,
  ];
}

/**
 * Every control a game controller owns the enabled state of.
 *
 * Off before one attaches, which the page sees to, and off again once it has
 * gone, which its teardown does -- both from this list. In between, the ones
 * above are on and the rest are set from the cube and the engine as those
 * change: the move buttons and their depth, the rewinds, Stop, Share, Watch
 * and Paint. One list for all three moments, so a control added to the shell
 * cannot be switched on by one of them and left on by another.
 */
export function controllerControls(
  ui: GameUi,
): (HTMLButtonElement | HTMLInputElement)[] {
  return [
    ...commandControls(ui),
    ...ui.moveButtons,
    ui.turnDepthInput,
    ui.turnWideButton,
    ui.undoButton,
    ui.redoButton,
    ui.rewindButton,
    ui.solveButton,
    ui.stopButton,
    ui.shareButton,
    ui.ambientButton,
    ui.paintButton,
  ];
}

/** A face as a sentence says it. */
const FACE_NAMES: Readonly<Record<CubeFace, string>> = {
  [CubeFace.Right]: 'Right',
  [CubeFace.Left]: 'Left',
  [CubeFace.Up]: 'Up',
  [CubeFace.Down]: 'Down',
  [CubeFace.Front]: 'Front',
  [CubeFace.Back]: 'Back',
};

/** How far each arrow moves the keyboard's place on the net, in cells. */
const NET_STEPS: Readonly<Record<string, readonly [number, number]>> = {
  ArrowLeft: [-1, 0],
  ArrowRight: [1, 0],
  ArrowUp: [0, -1],
  ArrowDown: [0, 1],
};

/** What the net's canvas is called while it is being coloured. */
const PAINT_NET_LABEL =
  "Your cube's faces, to colour. Arrow keys move, Enter or Space colours " +
  'the square, 1 to 6 choose a colour in the order of the colour bar.';

/**
 * Whether the focus arrived the way a keyboard brings it.
 *
 * A press on the net focuses it as well, and a place drawn for a mouse would
 * be a ring nobody asked for. Browsers that cannot say are taken as keyboard,
 * which shows the place rather than hiding it.
 */
function focusVisible(element: Element): boolean {
  try {
    return element.matches(':focus-visible');
  } catch {
    return true;
  }
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
 * keeping AppLifecycle's single teardown path intact. A failure part way
 * through leaves nothing behind either: the one step that can fail before
 * any listener exists releases the sound, and the one after they all do
 * unwinds through that same teardown.
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
   * waiting to open an audio context nothing would ever close. A sound that
   * fails to close as well is not what went wrong, so the step's own error is
   * the one that is thrown.
   */
  const setup = <T>(step: () => T): T => {
    try {
      return step();
    } catch (error) {
      try {
        sound.teardown();
      } catch {
        // The step's failure is the one to report.
      }
      throw error;
    }
  };

  // The session first: it is the one piece that reads the engine as it is
  // made, and so the one that can fail, and nothing after it may be holding a
  // listener when it does. Everything from the lists on listens from the
  // moment it exists, and none of it can fail on its own.
  const records = setup(
    () => new SessionRecords(ui.recordBest, ui.recordList, ui.recordTally),
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

  // A move pressed on either list is a place to take the cube to: the cursor
  // just past it, so that move is the last one on. The walk is declared
  // further down; nothing can be pressed before this function has returned.
  const pickMove = (index: number): void => {
    seekTo(index + 1);
  };
  const moveLog = new MoveLog(ui.moveLogList, engine, { onPick: pickMove });
  const timeline = new Timeline(ui.timeline, engine, pickMove);
  const shareCard = new ShareCard(ui.shareCard, {
    copy: (text: string): Promise<void> => shareTarget.copy(text),
    returnFocus: ui.shareButton,
  });

  // Where the record stood when the link on the card -- or on its way to it,
  // while the clipboard is still thinking -- was read from it. The card comes
  // down once the cube is somewhere else, since it says the link opens this
  // cube: a command takes it down as it runs, and a drag -- which comes
  // through no command -- shows as a record that has moved. A copy that
  // finishes after either finds this gone and shows nothing.
  let sharedAt: { readonly cursor: number; readonly length: number } | null =
    null;
  const hideShareCard = (): void => {
    sharedAt = null;
    shareCard.hide();
  };

  const updateViewControls = (): void => {
    const selected = engine.viewMode();
    for (const button of ui.viewButtons) {
      const mode = viewModeOf(button);
      button.setAttribute('aria-pressed', String(mode === selected));
    }
    ui.stage.dataset.viewMode = VIEW_NAME_BY_MODE[selected];

    const style = engine.flatStyle();
    for (const button of ui.flatButtons) {
      const value = flatStyleOf(button);
      button.setAttribute('aria-pressed', String(value === style));
      // The toggle only means something while the flat region is on screen.
      button.hidden = selected === CubeViewMode.Cube3D;
    }
    ui.stage.dataset.flatStyle = FLAT_NAME_BY_STYLE[style];
  };

  /**
   * What the line under the dock says about Solve, if it says anything.
   *
   * Three states rather than two, because there are two different things a
   * person can need to know before pressing. One is that Solve will never
   * answer for this cube, which is a fact about the build. The other is that
   * it will answer and take a long time doing it, which is a fact about the
   * size in hand -- and it has to be readable *before* the press, because the
   * solve happens inside one call into the engine and nothing on the page
   * answers again until it returns. There is no stopping it and no progress
   * to show; the only kind moment is this one.
   */
  const writeSolverNote = (solvable: boolean): void => {
    if (!solvable) {
      ui.solverNote.textContent =
        'No solver for this cube size yet — Rewind still works.';
      ui.solverNote.hidden = false;
      return;
    }

    const size = engine.cubeSize();
    if (size < SOLVE_WARNING_CUBE_SIZE) {
      ui.solverNote.hidden = true;
      return;
    }

    ui.solverNote.textContent =
      `Solve works on a ${size}×${size}, but it can take minutes and the ` +
      'page will not respond while it runs. Rewind undoes the scramble ' +
      'instead.';
    ui.solverNote.hidden = false;
  };

  const updatePaletteControls = (): void => {
    const selected = engine.palette();
    for (const button of ui.paletteButtons) {
      const value = paletteOf(button);
      button.setAttribute('aria-pressed', String(value === selected));
    }
    // The move chips show each face in the shade the cube is drawn in, and
    // the stylesheet picks those shades by this.
    ui.root.dataset.stickerPalette = PALETTE_NAME_BY_VALUE[selected];
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
      window.matchMedia('(prefers-reduced-motion: reduce)').matches;
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

  /** The walk under way, or null. Any other command drops it. */
  let walk: Walk | null = null;

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
   * nobody has been handed yet. A cursor below the scramble boundary is a
   * solve stopped inside the scramble -- a state the payload has no shape for.
   *
   * A cursor of nothing used to be refused here on the grounds that a link
   * would have opened that cube anyway. That is true, and it is a reason to
   * hand over the plain address rather than a reason to hand over nothing: a
   * button that cannot be pressed on the screen somebody wants to send is a
   * button they have to work out an explanation for. What travels then is the
   * page, which opens the cube they are looking at.
   */
  const canShare = (now: EngineFrame): boolean =>
    (!now.busy || now.watching) && now.cursor >= now.scrambleEnd;

  /**
   * Whether the cube is anything the plain address would not open.
   *
   * False only for the untouched cube of the opening size, which the guard
   * above has already narrowed this to: a cursor at nothing with no scramble
   * behind it, at the size every page opens on. An untouched cube of another
   * size is something a link has to say, since that size is on this table and
   * not on a fresh one.
   */
  const hasStateToShare = (now: EngineFrame): boolean =>
    now.cursor > 0 ||
    engine.originPainting().length > 0 ||
    engine.cubeSize() !== DEFAULT_CUBE_SIZE;

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
    size: engine.cubeSize(),
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
    // A draft holds the cube still -- the engine refuses every command that
    // would move it until the draft is applied or put away -- so the controls
    // that move it go out with it.
    const painting = engine.isPainting();

    // Watching makes the engine busy, but a press on a move button stops the
    // watching and then turns -- exactly as the same letter on the keyboard
    // does. So the buttons stay live for it, where anything else that made the
    // engine busy would put them out.
    const movesOff = (now.busy && !now.watching) || painting;
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
    ui.rewindButton.disabled = movesOff || now.cursor === 0;

    // Two different reasons, and only one of them ever goes away: a cube
    // already solved has nothing to solve, and a cube of a size no solver
    // here handles never will have. The second is written out, because a
    // control that is out of reach for good should say so.
    const solvable = engine.canSolve();
    ui.solveButton.disabled = movesOff || !solvable || engine.isSolved();
    writeSolverNote(solvable);

    ui.shareButton.disabled = !canShare(now);

    // Off the screen and out of reach together: it is not a control that is
    // sometimes unavailable but one that only exists while there is a rewind
    // to break off, and the two attributes are how that is said to a person
    // looking and to a person tabbing.
    //
    // A walk is stoppable the whole way along, between its steps as well as
    // during them: Stop drops what is left of it.
    if (!now.busy) rewinding = false;
    const stoppable = rewinding || walk !== null;
    ui.stopButton.hidden = !stoppable;
    ui.stopButton.disabled = !stoppable;

    // Stop takes Scramble's place rather than appearing beside it: the dock
    // has one obvious thing in the middle of it, and while a rewind is
    // playing the obvious thing is the way out of it. Only the rewind family
    // and a walk reach here -- a scramble and a watched pattern are not
    // stoppable, so Scramble stays where it is through both of those.
    ui.scrambleButton.hidden = stoppable;

    // The same reading the controls are set from, so the lists can never be
    // describing a different moment than the buttons above them. They draw
    // only when the record has changed, which is what makes calling them
    // every frame and after every command the simple thing to do.
    moveLog.update(now);
    timeline.update(now);

    ui.ambientButton.setAttribute('aria-pressed', String(now.watching));
    ui.ambientButton.disabled =
      painting || (!now.watching && !WATCHABLE.has(session.state));

    // A draft is taken only of a cube at rest, and watching gives way to one;
    // an open draft can always be put away, and nothing plays while it is.
    ui.paintButton.disabled = now.busy && !now.watching;

    // Whether the bar is up is the engine's to say, and it says so without
    // being asked: a scramble, a reset and a new size each put a different
    // cube there and close the draft on their way. Read here, where every
    // command and every frame passes, rather than left to the bar's own
    // buttons, which none of those is.
    if (painting === ui.paintBar.hidden) updatePaintControls();
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
      // Whatever was being walked to is not where this command is going. A
      // step already landing finishes; the ones after it are not asked for.
      walk = null;
      // Nor is the cube the link on the card opens, or not for long.
      hideShareCard();
      leaveAmbient();
      action();
      updateEngineControls();
    });
  };

  /**
   * The layers a face button or a keyboard letter turns, as depths.
   *
   * Wide takes everything above the depth with it, which is the whole of the
   * difference between `Rw` and the slice behind `R`. A drag needs none of
   * this: a finger lands on one layer and turns that one.
   */
  const turnRange = (): { readonly first: number; readonly last: number } => ({
    first: turnWide ? 1 : turnDepth,
    last: turnDepth,
  });

  const startFaceTurn = (face: CubeFace, turns: FaceTurns): void => {
    cubeCommand((): void => {
      const range = turnRange();
      if (engine.turnFace(face, range.first, range.last, turns)) {
        startFrameLoop();
      }
    });
  };

  // The last value each box held that the engine would accept. Kept here so a
  // refused edit has something to go back to.
  let scrambleMoves = DEFAULT_SCRAMBLE_MOVES;
  ui.scrambleMovesInput.value = String(scrambleMoves);

  // How deep the face controls reach, and whether they bring the layers above
  // that depth with them. Held here rather than read off the boxes, so a
  // half-typed number never reaches the cube.
  let turnDepth = 1;
  let turnWide = false;

  /**
   * Marks a box as having just refused an edit.
   *
   * Not `aria-invalid`: the value put back is a valid one, so the field is not
   * in an invalid state -- what happened is an event, and this is how long it
   * stays visible. It has to be visible on its own because the spoken half of
   * the refusal goes to the shared status line, and pressing Scramble right
   * after a refusal overwrites that line in the same interaction.
   *
   * Cleared out and re-set so that refusing twice running shows twice; setting
   * an attribute that is already there restarts no animation.
   *
   * Not set at all on a box inside a closed panel -- which is where Escape
   * leaves it, since closing is what takes the focus away and commits the
   * edit. No animation runs on a box that is not drawn, so none would end,
   * and the mark would flash again the next time the panel opened.
   */
  const flashRefusal = (box: HTMLInputElement): void => {
    delete box.dataset.refused;
    if (box.closest('[hidden]') !== null) return;
    void box.offsetWidth;
    box.dataset.refused = '';
  };

  // The mark lasts as long as its animation, which is why one always runs --
  // the reduced-motion variant fades instead of moving rather than not being
  // there. Ended or cancelled, since a panel closed while the box is flashing
  // cancels it, and the mark must not outlive it either way.
  const refusalBoxes = [
    ui.scrambleMovesInput,
    ui.cubeSizeInput,
    ui.turnDepthInput,
  ];
  const REFUSAL_FLASH_ENDS = ['animationend', 'animationcancel'] as const;
  const onRefusalFlashEnd = (event: Event): void => {
    const box = event.currentTarget;
    if (box instanceof HTMLInputElement) delete box.dataset.refused;
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
    flashRefusal(ui.scrambleMovesInput);
    session.announce(
      `A scramble is 1 to ${MAX_SCRAMBLE_MOVES} moves. Kept ${scrambleMoves}.`,
    );
  };

  /**
   * Writes the depth control out for the cube in hand.
   *
   * The deepest a range may reach is one short of the cube, because the whole
   * of it at once is a rotation and no command makes one. A depth left over
   * from a larger cube is brought back inside rather than refused: nobody
   * typed it at this cube, so there is nothing to tell them about.
   */
  const updateTurnDepthControl = (): void => {
    const deepest = Math.max(1, engine.cubeSize() - 1);
    turnDepth = Math.min(turnDepth, deepest);
    ui.turnDepthInput.max = String(deepest);
    ui.turnDepthInput.value = String(turnDepth);

    // A cube with one layer to choose from has nothing to say about depth,
    // and a 2x2 is that cube: both of its layers are outer faces.
    ui.turnDepthInput.disabled = deepest === 1;
    ui.turnWideButton.disabled = deepest === 1;
    ui.turnWideButton.setAttribute('aria-pressed', String(turnWide));
  };

  const onTurnDepthChange = (): void => {
    run((): void => {
      const typed = Number(ui.turnDepthInput.value);
      const deepest = Math.max(1, engine.cubeSize() - 1);
      if (Number.isInteger(typed) && typed >= 1 && typed <= deepest) {
        turnDepth = typed;
        delete ui.turnDepthInput.dataset.refused;
        return;
      }

      ui.turnDepthInput.value = String(turnDepth);
      flashRefusal(ui.turnDepthInput);
      session.announce(
        `A turn on this cube reaches 1 to ${deepest} layers deep. ` +
          `Kept ${turnDepth}.`,
      );
    });
  };

  const onTurnWide = (): void => {
    turnWide = !turnWide;
    ui.turnWideButton.setAttribute('aria-pressed', String(turnWide));
  };

  const updateCubeSizeControl = (): void => {
    const size = engine.cubeSize();
    ui.cubeSizeInput.value = String(size);
    ui.cubeSizeLabel.textContent = `${size}×${size}×${size}`;
  };

  /**
   * Puts a different cube on the table, which starts the session over.
   *
   * A cube command like Reset, and the same shape: watching gives way to it,
   * the engine builds the new cube, one frame draws it -- nothing is animating
   * afterwards, so there is no loop to leave it to -- and the session restarts.
   */
  const onCubeSizeChange = (): void => {
    const typed = Number(ui.cubeSizeInput.value);
    if (
      !Number.isInteger(typed) ||
      typed < MIN_CUBE_SIZE ||
      typed > MAX_CUBE_SIZE
    ) {
      // Through `run` like any other read of the engine, so a failure here
      // takes the same way out rather than escaping the page.
      run((): void => {
        const kept = engine.cubeSize();
        ui.cubeSizeInput.value = String(kept);
        flashRefusal(ui.cubeSizeInput);
        session.announce(
          `Cubes here are ${MIN_CUBE_SIZE} to ${MAX_CUBE_SIZE} layers. ` +
            `Kept ${kept}×${kept}.`,
        );
      });
      return;
    }

    delete ui.cubeSizeInput.dataset.refused;
    cubeCommand((): void => {
      if (!engine.setCubeSize(typed)) return;

      engine.render();
      session.restart();
      updateCubeSizeControl();
      updateTurnDepthControl();
    });
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

  const onRewind = (): void => {
    rewindCommand(
      () => engine.solveRewind(),
      'Rewinding to the solved cube. Press Stop to break off.',
    );
  };

  /**
   * Hands the cube to the solver and plays what it works out.
   *
   * The sitting is marked before the frames start rather than when the cube
   * comes out solved: the help was asked for here, and stopping the solve part
   * way through does not unask it.
   */
  const onSolve = (): void => {
    rewindCommand((): boolean => {
      if (!engine.solve()) return false;
      session.beginSolve();
      return true;
    }, 'Solving the cube. Press Stop to break off.');
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
      // A walk still waiting for a scramble to finish has not begun, and the
      // scramble cannot be stopped: the walk is all Stop takes away, and the
      // cube goes on turning, so "Stopped." would not be true.
      const waiting = walk !== null && session.state === 'scrambling';
      walk = null;
      engine.stopPlayback();
      rewinding = false;
      session.announce(
        waiting ? 'Cancelled. The scramble still plays out.' : 'Stopped.',
      );
      updateEngineControls();
    });
  };

  /**
   * Asks for the next step of the walk, if the last one has landed.
   *
   * Called when a walk begins and again after every frame, so each step goes
   * in on the first idle frame after the one before it. The walk ends rather
   * than going on when the cube is somewhere a person has taken it: a step
   * the engine refuses, or a commit the walk did not make.
   *
   * The second is how a drag shows. A typed turn is an ordinary turn and not
   * a sequence the engine plays, so a press on the cube while one is turning
   * is taken -- the turn is committed on the spot and the drag goes on from
   * there -- and nothing is refused afterwards. Only the record says it
   * happened: one commit more than the walk asked for.
   */
  const stepWalk = (): void => {
    if (walk === null || engine.isBusy()) return;

    const cursor = engine.timelineCursor();
    if (walk.landsAt !== null && cursor !== walk.landsAt) {
      walk = null;
      session.announce('Stopped playing: a layer was turned by hand.');
      return;
    }

    if (walk.kind === 'seek') {
      if (cursor === walk.target) {
        walk = null;
        return;
      }
      // One move at a time, played the way Undo and Redo play it, so a walk
      // back through twelve moves is watched as twelve moves coming off.
      const back = cursor > walk.target;
      if (!(back ? engine.undo() : engine.redo())) {
        walk = null;
        return;
      }
      walk.landsAt = back ? cursor - 1 : cursor + 1;
      rewinding = true;
      startFrameLoop();
      return;
    }

    const next = walk.turns.shift();
    if (next === undefined) {
      walk = null;
      return;
    }
    if (
      !engine.turnFace(
        FACE_BY_NOTATION[next.face],
        next.firstDepth,
        next.lastDepth,
        next.turns,
      )
    ) {
      walk = null;
      return;
    }
    walk.landsAt = cursor + 1;
    startFrameLoop();
  };

  /**
   * Sets off on a walk, breaking off whatever rewind was playing.
   *
   * A cube command like any other, so watching gives way first. The newest
   * request is the one the cube follows: a rewind still playing is stopped
   * where it stands, and a scramble -- which cannot be stopped -- is waited
   * out, the first step going in on the frame it finishes.
   */
  const startWalk = (next: Walk): void => {
    cubeCommand((): void => {
      if (engine.isPainting()) {
        session.announce('Finish or cancel the colouring first.');
        return;
      }
      if (rewinding) {
        engine.stopPlayback();
        rewinding = false;
      }
      walk = next;
      stepWalk();
    });
  };

  /** Walks the cube to the point in the record just past `cursor` moves. */
  const seekTo = (cursor: number): void => {
    if (cursor === engine.timelineCursor() && !engine.isBusy()) return;
    startWalk({ kind: 'seek', target: cursor, landsAt: null });
  };

  /** Reads a line of typed moves and walks through them, or says why not. */
  const onPlayMoves = (event: Event): void => {
    const detail = (event as CustomEvent<PlayMovesDetail>).detail;
    run((): void => {
      const typed = parseMoves(detail?.text ?? '', engine.cubeSize());
      if (!typed.ok) {
        session.announce(typed.reason);
        return;
      }
      if (typed.turns.length === 0) return;

      const count = typed.turns.length;
      startWalk({ kind: 'turns', turns: [...typed.turns], landsAt: null });
      if (walk !== null) {
        session.announce(`Playing ${count} ${count === 1 ? 'move' : 'moves'}.`);
      }
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

  // What the net's canvas is called when nothing is being coloured, which is
  // put back when a draft closes.
  const netLabel = ui.netCanvas.getAttribute('aria-label') ?? '';

  /** The view that was up when a draft opened, and the one it was changed to. */
  type StageView = { readonly mode: CubeViewMode; readonly style: CubeFlatStyle };
  let paintView: { readonly before: StageView; readonly shown: StageView } | null =
    null;

  /**
   * Makes the net the place a draft is coloured, and stops it being one.
   *
   * On the way in the net is put on the stage if the view had left it off --
   * the cube alone, or the rings -- since a draft is coloured nowhere else,
   * and its canvas joins the tab order under a name that says how the keys
   * colour it. On the way out both are undone: the view only if it is still
   * the one put up here, since a view somebody chose while colouring is one
   * they meant.
   *
   * Called on every change of the paint bar and doing anything only when the
   * draft has opened or closed since, which is what `paintView` remembers.
   */
  const followDraft = (painting: boolean): void => {
    if (painting && paintView === null) {
      const before = { mode: engine.viewMode(), style: engine.flatStyle() };
      const shown = {
        mode:
          before.mode === CubeViewMode.Cube3D ? CubeViewMode.Both : before.mode,
        style:
          before.style === CubeFlatStyle.Rings ? CubeFlatStyle.Net : before.style,
      };
      paintView = { before, shown };
      if (shown.mode !== before.mode || shown.style !== before.style) {
        engine.setViewMode(shown.mode);
        engine.setFlatStyle(shown.style);
        engine.render();
        updateViewControls();
      }

      ui.netCanvas.tabIndex = 0;
      ui.netCanvas.setAttribute('role', 'application');
      ui.netCanvas.setAttribute('aria-roledescription', 'colouring net');
      ui.netCanvas.setAttribute('aria-label', PAINT_NET_LABEL);
      return;
    }

    if (!painting && paintView !== null) {
      const { before, shown } = paintView;
      paintView = null;
      const changed = shown.mode !== before.mode || shown.style !== before.style;
      if (
        changed &&
        engine.viewMode() === shown.mode &&
        engine.flatStyle() === shown.style
      ) {
        engine.setViewMode(before.mode);
        engine.setFlatStyle(before.style);
        engine.render();
        updateViewControls();
      }

      ui.netCanvas.removeAttribute('tabindex');
      ui.netCanvas.removeAttribute('role');
      ui.netCanvas.removeAttribute('aria-roledescription');
      ui.netCanvas.setAttribute('aria-label', netLabel);
    }
  };

  /** How the paint bar reads right now, engine and page kept in one place. */
  const updatePaintControls = (): void => {
    const painting = engine.isPainting();
    ui.paintButton.setAttribute('aria-pressed', String(painting));
    ui.paintBar.hidden = !painting;
    followDraft(painting);

    if (!painting) {
      ui.paintNote.textContent = '';
      return;
    }

    const brush = engine.brush();
    const wanted = engine.cubeSize() * engine.cubeSize();
    for (const swatch of ui.paintSwatches) {
      const colour = stickerOf(swatch);
      if (colour === null) continue;
      swatch.setAttribute('aria-pressed', String(colour === brush));

      const tally = swatch.querySelector<HTMLElement>('[data-sticker-tally]');
      if (!tally) continue;
      const found = engine.paintedCount(colour);
      tally.textContent = `${found}/${wanted}`;
      // Said in the markup as well as in the number, so that "this one is
      // short" does not rest on colour alone.
      tally.dataset.short = String(found !== wanted);
    }

    ui.paintFillButton.setAttribute('aria-pressed', String(engine.isFilling()));

    const fault = engine.paintFault();
    ui.paintNote.textContent = PAINT_FAULT_SENTENCE[fault];
  };

  /** The colour a number names, as the colour bar writes it. */
  const colourName = (colour: CubeStickerColour): string =>
    ui.paintSwatches
      .find((swatch) => stickerOf(swatch) === colour)
      ?.querySelector('.swatch__name')
      ?.textContent?.trim() ?? '';

  /** Says where the keyboard's place is, and what colour is there. */
  const announceCursor = (): void => {
    const at = engine.paintCursor();
    if (at === null) return;
    session.announce(
      `${FACE_NAMES[at.face]} face, row ${at.row + 1}, column ${at.col + 1}: ` +
        `${colourName(at.colour)}.`,
    );
  };

  /**
   * The keys the net takes while it is being coloured.
   *
   * The arrows walk a place across the net, Enter or Space lays the brush
   * there -- on the square, or the whole face while filling -- and the digits
   * pick a colour in the bar's order, so a face can be copied without the
   * keyboard leaving the net. Anything else is left alone.
   */
  const onNetKeyDown = (event: KeyboardEvent): void => {
    if (event.altKey || event.ctrlKey || event.metaKey) return;
    if (!active || !engine.isPainting()) return;

    const step = NET_STEPS[event.key];
    const swatch = /^[1-6]$/.test(event.key)
      ? ui.paintSwatches[Number(event.key) - 1]
      : undefined;
    const lay = event.key === 'Enter' || event.key === ' ';
    if (step === undefined && swatch === undefined && !lay) return;

    event.preventDefault();
    event.stopPropagation();
    run((): void => {
      engine.setPaintCursorShown(true);
      if (step !== undefined) {
        if (engine.paintCursorStep(step[0], step[1])) announceCursor();
        else session.announce('The net ends there.');
      } else if (swatch !== undefined) {
        swatch.click();
        session.announce(`${colourName(engine.brush())} brush.`);
      } else if (engine.paintAtCursor()) {
        updatePaintControls();
        announceCursor();
      }
      startFrameLoop();
    });
  };

  // The place is put down the first time the net is reached, and drawn while
  // the keyboard is there -- and not for a press, which focuses the net too.
  const onNetFocus = (): void => {
    if (!active || !engine.isPainting()) return;
    run((): void => {
      if (engine.paintCursor() === null) engine.paintCursorStep(0, 0);
      if (!focusVisible(ui.netCanvas)) return;
      engine.setPaintCursorShown(true);
      announceCursor();
      startFrameLoop();
    });
  };

  const onNetBlur = (): void => {
    if (!active || !engine.isPainting()) return;
    run((): void => {
      engine.setPaintCursorShown(false);
      startFrameLoop();
    });
  };

  /**
   * Opens or closes a draft of the cube to colour.
   *
   * A cube command, so watching gives way to it first: a draft taken while a
   * pattern is running would be a copy of a moment nobody chose.
   */
  const onPaint = (): void => {
    run((): void => {
      if (engine.isPainting()) {
        engine.cancelPainting();
        session.announce('Colouring cancelled. The cube is as it was.');
      } else {
        leaveAmbient();
        if (engine.beginPainting()) {
          session.announce(
            'Colour your own cube onto the net, then press Use this cube.',
          );
        }
      }

      updatePaintControls();
      updateEngineControls();
    });
  };

  /** Makes the colouring the cube, or says what is stopping it. */
  const onPaintApply = (): void => {
    run((): void => {
      if (engine.applyPainting()) {
        // A different cube, which is not what a link on the card opens.
        hideShareCard();
        session.announce('That is your cube now. Press Solve to work it out.');
      } else {
        session.announce('That colouring is not a cube yet.');
      }

      updatePaintControls();
      updateEngineControls();
    });
  };

  const onPaintFill = (): void => {
    run((): void => {
      engine.setFilling(!engine.isFilling());
      updatePaintControls();
    });
  };

  const onPaintCancel = (): void => {
    run((): void => {
      engine.cancelPainting();
      session.announce('Colouring cancelled. The cube is as it was.');
      updatePaintControls();
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

      // The plain address for an untouched cube of the opening size: there is
      // no record to encode and nothing about the size to say, so a fragment
      // would only be a longer way of saying the same thing.
      let link: string = pageUrl(shareTarget.currentUrl());
      let opens = 'It opens a fresh cube.';

      if (hasStateToShare(now)) {
        // A cube somebody painted cannot be written as moves -- nothing from
        // solved arrives at it without solving it first -- so its link carries
        // the colours instead, and the moves made since go on the end of them.
        // Which of the two is right is read off the session rather than
        // remembered: a colouring is there or it is not.
        const painting = engine.originPainting();
        const encoded =
          painting.length > 0
            ? encodePainting({
                size: engine.cubeSize(),
                painting,
                user: recordedBetween(now.scrambleEnd, now.cursor),
              })
            : encodeSession(sharableNow(now));
        if (encoded === null) {
          session.announce('This cube cannot be written into a link.');
          return;
        }
        link = shareUrl(shareTarget.currentUrl(), encoded);
        opens = 'It opens this cube.';
      }

      // Guarded on the way back rather than on the way out: a controller torn
      // down while the clipboard was thinking has no status line left to
      // write to, and the elements are no longer this controller's. Nor is a
      // link overtaken while it was on its way -- by a command, a drag or a
      // newer share, each of which leaves `sharedAt` somewhere else -- one to
      // show or announce: the cube it opens is not this one, and whatever
      // overtook it has had its own say.
      //
      // Either way the link goes up on the card by the button. Copied, it is
      // there to read and to copy again; refused, it is there to be copied
      // by hand, which is the only way it still reaches anybody.
      const pending = { cursor: now.cursor, length: now.length };
      sharedAt = pending;
      void shareTarget.copy(link).then(
        (): void => {
          if (!active || sharedAt !== pending) return;
          session.announce(`Link copied. ${opens}`);
          shareCard.show({ link, copied: true, opens });
        },
        (): void => {
          if (!active || sharedAt !== pending) return;
          session.announce('Could not copy the link.');
          shareCard.show({ link, copied: false, opens });
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
    ...bindChoices(ui.paintSwatches, stickerOf, (colour) => {
      run((): void => {
        engine.setBrush(colour);
        updatePaintControls();
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
    // Under a dialog the letters are the dialog's, wherever the focus is.
    // Asking where the key landed would not do: a press on a dialog's label
    // leaves the focus on the page itself, outside the dialog.
    if (
      options.blocked?.() === true ||
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
  ui.cubeSizeInput.addEventListener('change', onCubeSizeChange);
  ui.turnDepthInput.addEventListener('change', onTurnDepthChange);
  for (const box of refusalBoxes) {
    for (const type of REFUSAL_FLASH_ENDS) {
      box.addEventListener(type, onRefusalFlashEnd);
    }
  }
  ui.turnWideButton.addEventListener('click', onTurnWide);
  ui.scrambleButton.addEventListener('click', onScramble);
  ui.resetButton.addEventListener('click', onReset);
  ui.undoButton.addEventListener('click', onUndo);
  ui.redoButton.addEventListener('click', onRedo);
  ui.rewindButton.addEventListener('click', onRewind);
  ui.solveButton.addEventListener('click', onSolve);
  ui.stopButton.addEventListener('click', onStop);
  ui.shareButton.addEventListener('click', onShare);
  ui.ambientButton.addEventListener('click', onAmbient);
  ui.paintButton.addEventListener('click', onPaint);
  ui.paintFillButton.addEventListener('click', onPaintFill);
  ui.paintApplyButton.addEventListener('click', onPaintApply);
  ui.paintCancelButton.addEventListener('click', onPaintCancel);
  ui.homeViewButton.addEventListener('click', onHomeView);
  ui.muteButton.addEventListener('click', onMute);
  // `input` rather than `change`, so the reading follows the handle while it
  // is being dragged. Nothing in flight is re-timed, so the stream is safe.
  ui.speedInput.addEventListener('input', onSpeedChange);
  ui.root.addEventListener(PLAY_MOVES_EVENT, onPlayMoves);
  ui.netCanvas.addEventListener('keydown', onNetKeyDown);
  ui.netCanvas.addEventListener('focus', onNetFocus);
  ui.netCanvas.addEventListener('blur', onNetBlur);
  keyboardTarget.addEventListener('keydown', onKeyDown);

  /**
   * Lets go of everything this function took hold of, once.
   *
   * A name of its own rather than only a method of what is returned, because
   * the last step of setting up can fail with everything already listening,
   * and is unwound by this same call.
   */
  const teardown = (): void => {
    if (!active) return;
    active = false;
    walk = null;
    session.teardown();
    moveLog.teardown();
    timeline.teardown();
    shareCard.teardown();
    // The one moment none of them is the engine's or the session's to
    // decide: there is nothing left to answer a press.
    for (const control of controllerControls(ui)) control.disabled = true;
    ui.scrambleMovesInput.removeEventListener(
      'change',
      onScrambleMovesChange,
    );
    ui.cubeSizeInput.removeEventListener('change', onCubeSizeChange);
    ui.turnDepthInput.removeEventListener('change', onTurnDepthChange);
    for (const box of refusalBoxes) {
      for (const type of REFUSAL_FLASH_ENDS) {
        box.removeEventListener(type, onRefusalFlashEnd);
      }
    }
    ui.turnWideButton.removeEventListener('click', onTurnWide);
    ui.scrambleButton.removeEventListener('click', onScramble);
    ui.resetButton.removeEventListener('click', onReset);
    ui.undoButton.removeEventListener('click', onUndo);
    ui.redoButton.removeEventListener('click', onRedo);
    ui.rewindButton.removeEventListener('click', onRewind);
    ui.solveButton.removeEventListener('click', onSolve);
    ui.stopButton.removeEventListener('click', onStop);
    ui.shareButton.removeEventListener('click', onShare);
    ui.ambientButton.removeEventListener('click', onAmbient);
    ui.paintButton.removeEventListener('click', onPaint);
    ui.paintFillButton.removeEventListener('click', onPaintFill);
    ui.paintApplyButton.removeEventListener('click', onPaintApply);
    ui.paintCancelButton.removeEventListener('click', onPaintCancel);
    ui.homeViewButton.removeEventListener('click', onHomeView);
    ui.muteButton.removeEventListener('click', onMute);
    ui.speedInput.removeEventListener('input', onSpeedChange);
    ui.root.removeEventListener(PLAY_MOVES_EVENT, onPlayMoves);
    ui.netCanvas.removeEventListener('keydown', onNetKeyDown);
    ui.netCanvas.removeEventListener('focus', onNetFocus);
    ui.netCanvas.removeEventListener('blur', onNetBlur);
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
  };

  // The last step, and the first with every listener in place. A failure here
  // goes back out through the teardown a caller would have called, which
  // touches nothing of the engine's -- which may be what failed -- and turns
  // back off the controls this step has just turned on.
  try {
    for (const control of commandControls(ui)) control.disabled = false;
    updateCubeSizeControl();
    updateTurnDepthControl();
    updateViewControls();
    updatePaletteControls();
    updateMuteControl();
    engine.setSpeedScale(initialSpeedScale());
    updateSpeedControl();
    updateEngineControls();
  } catch (error) {
    try {
      teardown();
    } catch {
      // The setup's failure is the one to report.
    }
    throw error;
  }

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
        if (
          sharedAt !== null &&
          (now.cursor !== sharedAt.cursor || now.length !== sharedAt.length)
        ) {
          hideShareCard();
        }

        // A walk takes its next step as soon as the last one has landed, and
        // before the controls are read, so Stop does not blink out for the
        // one idle frame between two steps. The step changed the engine, so
        // the controls take a fresh reading rather than this frame's.
        if (walk !== null && !now.busy) {
          stepWalk();
          updateEngineControls();
        } else {
          updateEngineControls(now);
        }

        // The tallies follow the colouring, and a press on the net is the one
        // thing that changes it without coming through a command. A press asks
        // for a frame, so this is where it arrives -- and only while there is
        // a draft, since counting six colours over every sticker is not work
        // to do on a frame nobody is painting in.
        if (engine.isPainting()) updatePaintControls();
      });
    },

    teardown,
  };
}
