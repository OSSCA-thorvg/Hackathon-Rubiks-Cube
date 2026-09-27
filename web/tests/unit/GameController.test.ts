import { describe, expect, it, vi } from 'vitest';

import {
  attachGameController,
  controllerControls,
  PLAY_MOVES_EVENT,
  type GameEngine,
  type GameUi,
  type KeyboardTarget,
  type PlayMovesDetail,
} from '../../src/game/GameController.ts';
import { decodeSession } from '../../src/game/shareCode.ts';
import type { TimerEnvironment } from '../../src/game/SolveTimer.ts';
import {
  CubeFace,
  CubeFlatStyle,
  CubePaintFault,
  CubePalette,
  CubeStickerColour,
  CubeViewMode,
  MAX_CUBE_SIZE,
  MIN_CUBE_SIZE,
  SOLVE_WARNING_CUBE_SIZE,
} from '../../src/wasm/CubeEngine.ts';

/** Builds semantic controls matching bootstrap's production markup. */
function createUi(): GameUi {
  const root = document.createElement('main');
  root.innerHTML = `
    <section id="stage"><canvas id="view"></canvas></section>
    <output id="timer"></output>
    <p id="status"></p>
    <input id="scramble-moves" type="number" min="1" max="100" value="20">
    <input id="cube-size" type="number" min="2" max="9" value="3">
    <input id="turn-depth" type="number" min="1" max="2" value="1">
    <button id="turn-wide" type="button" aria-pressed="false">Wide</button>
    <button id="scramble" type="button">Scramble</button>
    <button id="reset" type="button">Reset</button>
    <button id="undo" type="button">Undo</button>
    <button id="redo" type="button">Redo</button>
    <button id="rewind" type="button">Rewind</button>
    <button id="solve" type="button">Solve</button>
    <p id="solver-note" hidden>No solver</p>
    <button id="stop" type="button" hidden>Stop</button>
    <button id="share" type="button">Share</button>
    <button id="ambient" type="button" aria-pressed="false">Watch</button>
    <button id="home-view" type="button">Home</button>
    <button data-view="3d" type="button">3D</button>
    <button data-view="both" type="button">Both</button>
    <button data-view="2d" type="button">2D</button>
    <button data-flat="net" type="button">Net</button>
    <button data-flat="rings" type="button">Rings</button>
    <button data-flat="both" type="button">Net + Rings</button>
    <button data-palette="classic" type="button" aria-pressed="true">Classic</button>
    <button data-palette="high-contrast" type="button" aria-pressed="false">High contrast</button>
    <button id="mute" type="button" aria-pressed="false">Mute turns</button>
    <input id="speed" type="range" min="0.25" max="4" step="0.25" value="1">
    <output id="speed-value"></output>
    <button data-face="r" data-turn="1" type="button">R</button>
    <button data-face="r" data-turn="-1" type="button">R prime</button>
    <button id="paint" type="button" aria-pressed="false">Paint</button>
    <section id="paint-bar" hidden>
      <button data-sticker="2" type="button" aria-pressed="true">
        <span data-sticker-tally="2">0/9</span>
      </button>
      <button data-sticker="3" type="button" aria-pressed="false">
        <span data-sticker-tally="3">0/9</span>
      </button>
      <button data-sticker="4" type="button" aria-pressed="false">
        <span data-sticker-tally="4">0/9</span>
      </button>
      <button data-sticker="5" type="button" aria-pressed="false">
        <span data-sticker-tally="5">0/9</span>
      </button>
      <button data-sticker="0" type="button" aria-pressed="false">
        <span data-sticker-tally="0">0/9</span>
      </button>
      <button data-sticker="1" type="button" aria-pressed="false">
        <span data-sticker-tally="1">0/9</span>
      </button>
      <button id="paint-fill" type="button" aria-pressed="false">Fill face</button>
      <button id="paint-apply" type="button">Use this cube</button>
      <button id="paint-cancel" type="button">Cancel</button>
      <p id="paint-note"></p>
    </section>
    <ol id="move-log"></ol>
    <p id="record-best"></p>
    <ol id="record-list"></ol>
    <span id="record-tally"></span>
    <span id="cube-size-label"></span>
    <ol id="timeline-moves"></ol>
    <div id="timeline-track"></div>
    <span id="timeline-scramble"></span>
    <span id="timeline-progress"></span>
    <p id="scramble-text"></p>
    <span id="moves-progress"></span>
    <section id="share-card" hidden>
      <h2 id="share-card-title"></h2>
      <button id="share-card-close" type="button">Close</button>
      <p id="share-card-note"></p>
      <input id="share-card-link" readonly>
      <button id="share-card-copy" type="button">Copy</button>
    </section>
  `;
  document.body.replaceChildren(root);

  return {
    root,
    stage: root.querySelector<HTMLElement>('#stage')!,
    timer: root.querySelector<HTMLOutputElement>('#timer')!,
    status: root.querySelector<HTMLParagraphElement>('#status')!,
    scrambleButton: root.querySelector<HTMLButtonElement>('#scramble')!,
    scrambleMovesInput: root.querySelector<HTMLInputElement>('#scramble-moves')!,
    resetButton: root.querySelector<HTMLButtonElement>('#reset')!,
    cubeSizeInput: root.querySelector<HTMLInputElement>('#cube-size')!,
    turnDepthInput: root.querySelector<HTMLInputElement>('#turn-depth')!,
    turnWideButton: root.querySelector<HTMLButtonElement>('#turn-wide')!,
    undoButton: root.querySelector<HTMLButtonElement>('#undo')!,
    redoButton: root.querySelector<HTMLButtonElement>('#redo')!,
    rewindButton: root.querySelector<HTMLButtonElement>('#rewind')!,
    solveButton: root.querySelector<HTMLButtonElement>('#solve')!,
    solverNote: root.querySelector<HTMLParagraphElement>('#solver-note')!,
    stopButton: root.querySelector<HTMLButtonElement>('#stop')!,
    shareButton: root.querySelector<HTMLButtonElement>('#share')!,
    shareCard: {
      card: root.querySelector<HTMLElement>('#share-card')!,
      title: root.querySelector<HTMLElement>('#share-card-title')!,
      note: root.querySelector<HTMLElement>('#share-card-note')!,
      link: root.querySelector<HTMLInputElement>('#share-card-link')!,
      copyButton: root.querySelector<HTMLButtonElement>('#share-card-copy')!,
      closeButton: root.querySelector<HTMLButtonElement>('#share-card-close')!,
    },
    recordBest: root.querySelector<HTMLParagraphElement>('#record-best')!,
    recordList: root.querySelector<HTMLOListElement>('#record-list')!,
    recordTally: root.querySelector<HTMLElement>('#record-tally')!,
    cubeSizeLabel: root.querySelector<HTMLElement>('#cube-size-label')!,
    moveLogList: root.querySelector<HTMLOListElement>('#move-log')!,
    timeline: {
      strip: root.querySelector<HTMLOListElement>('#timeline-moves')!,
      track: root.querySelector<HTMLElement>('#timeline-track')!,
      scrambleLabel: root.querySelector<HTMLElement>('#timeline-scramble')!,
      progressLabel: root.querySelector<HTMLElement>('#timeline-progress')!,
      scrambleText: root.querySelector<HTMLElement>('#scramble-text')!,
      panelProgress: root.querySelector<HTMLElement>('#moves-progress')!,
    },
    ambientButton: root.querySelector<HTMLButtonElement>('#ambient')!,
    homeViewButton: root.querySelector<HTMLButtonElement>('#home-view')!,
    viewButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-view]')],
    flatButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-flat]')],
    paletteButtons: [
      ...root.querySelectorAll<HTMLButtonElement>('[data-palette]'),
    ],
    muteButton: root.querySelector<HTMLButtonElement>('#mute')!,
    speedInput: root.querySelector<HTMLInputElement>('#speed')!,
    speedValue: root.querySelector<HTMLOutputElement>('#speed-value')!,
    moveButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-face]')],
    paintButton: root.querySelector<HTMLButtonElement>('#paint')!,
    paintBar: root.querySelector<HTMLElement>('#paint-bar')!,
    paintSwatches: [
      ...root.querySelectorAll<HTMLButtonElement>('[data-sticker]'),
    ],
    paintFillButton: root.querySelector<HTMLButtonElement>('#paint-fill')!,
    paintApplyButton: root.querySelector<HTMLButtonElement>('#paint-apply')!,
    paintCancelButton: root.querySelector<HTMLButtonElement>('#paint-cancel')!,
    paintNote: root.querySelector<HTMLElement>('#paint-note')!,
  };
}

/** How many moves the fake engine's scramble is. */
const SCRAMBLE_MOVES = 3;

/** Creates a fake engine and manually controlled timer/keyboard environment. */
function createHarness(
  overrides: {
    prefersReducedMotion?: boolean;
    /**
     * A record already on the cube when the controller is attached.
     *
     * What a shared link leaves behind: the restore happens before anything
     * is wired up, so this is the state the very first frame observes.
     */
    opened?: { readonly scrambleEnd: number; readonly moves: number[] };
    /** Whether the page says a dialog is up. */
    blocked?: () => boolean;
  } = {},
) {
  const ui = createUi();
  let solved = true;
  let busy = false;
  let viewMode = CubeViewMode.Both;
  let flatStyle = CubeFlatStyle.Net;
  let palette = CubePalette.Classic;
  let painting = false;
  let touched = false;
  let filling = false;
  let brush: CubeStickerColour = CubeStickerColour.White;
  let fault: CubePaintFault = CubePaintFault.None;
  // Clamped the way the engine clamps, so the control is tested against the
  // answer it will actually be given rather than the one it asked for.
  let speedScale = 1;
  let watching = false;
  let cubeSize = 3;

  // The record the real engine keeps, in the same shape: one length and two
  // indices into it. The user's move count is derived from them here too,
  // because a fake that counted separately could not show the difference the
  // derivation was made for.
  let length = 0;
  let cursor = 0;
  let scrambleEnd = 0;
  // The moves themselves, packed as the engine packs them, so the log can be
  // read off the same record the counts describe. R, U and F for a scramble
  // and R' for anything the user turns, which is enough to tell an entry that
  // moved from one that was redrawn where it was.
  let moves: number[] = [];
  // Where a rewind that is playing will end up, and whether Stop may reach it.
  let rewindTo: number | null = null;
  let stoppable = false;
  // Whether the sequence being played is a solution rather than a rewind,
  // which is what decides whether the cube comes out solved at its end.
  let solving = false;

  if (overrides.opened !== undefined) {
    moves = [...overrides.opened.moves];
    length = moves.length;
    cursor = moves.length;
    scrambleEnd = overrides.opened.scrambleEnd;
    solved = false;
  }

  const startRewind = (to: number): boolean => {
    rewindTo = to;
    busy = true;
    stoppable = true;
    return true;
  };

  const engine = {
    scramble: vi.fn((): void => {
      // Accepted and busy, with the cube still solved: the moves are turned
      // into it over the frames that follow, and the record holds all of them
      // from the moment it is accepted.
      length = SCRAMBLE_MOVES;
      cursor = 0;
      scrambleEnd = SCRAMBLE_MOVES;
      moves = [0x44, 0x45, 0x46];
      busy = true;
      watching = false;
      // A different cube, so the draft describing the old one closes.
      painting = false;
    }),
    resetCube: vi.fn((): void => {
      solved = true;
      length = 0;
      cursor = 0;
      scrambleEnd = 0;
      moves = [];
      busy = false;
      watching = false;
      painting = false;
    }),
    // Nothing moves the cube behind an open draft, as in the engine.
    undo: vi.fn((): boolean => {
      if (busy || painting || cursor <= scrambleEnd) return false;
      return startRewind(cursor - 1);
    }),
    redo: vi.fn((): boolean => {
      if (busy || painting || cursor >= length) return false;
      return startRewind(cursor + 1);
    }),
    solveRewind: vi.fn((): boolean => {
      if (busy || painting || cursor === 0) return false;
      return startRewind(0);
    }),
    canSolve: vi.fn((): boolean => true),
    solve: vi.fn((): boolean => {
      if (busy || painting || solved) return false;

      // A solution is written in above the cursor and then played forward,
      // so the record grows at once and the cursor walks up to its end.
      const solution = [0x47, 0x48, 0x49];
      moves = [...moves.slice(0, cursor), ...solution];
      length = moves.length;
      scrambleEnd = Math.min(scrambleEnd, cursor);
      solving = true;
      return startRewind(length);
    }),
    stopPlayback: vi.fn((): void => {
      if (!stoppable) return;

      // The turn already in flight is confirmed rather than dropped, so a
      // rewind broken off has moved the cursor one step towards its target.
      if (rewindTo !== null && rewindTo !== cursor) {
        cursor += rewindTo > cursor ? 1 : -1;
      }
      rewindTo = null;
      stoppable = false;
      busy = false;
    }),
    timelineLength: vi.fn((): number => length),
    timelineCursor: vi.fn((): number => cursor),
    timelineScrambleEnd: vi.fn((): number => scrambleEnd),
    timelineMove: vi.fn((index: number): number => moves[index] ?? 0),
    ambientStart: vi.fn((): boolean => {
      if (watching || painting) return false;
      // A pattern that never runs out: busy, and staying so.
      watching = true;
      busy = true;
      return true;
    }),
    ambientStop: vi.fn((): void => {
      watching = false;
      busy = false;
    }),
    isAmbient: vi.fn((): boolean => watching),

    // The draft, kept the way the engine keeps it: its presence is what "is
    // being painted" means, and the one thing this fake decides for itself is
    // that a colouring which was never touched is a cube -- which is true of
    // the real one too, since a draft opens as a copy of what is on the cube.
    beginPainting: vi.fn((): boolean => {
      if (busy || painting) return false;
      painting = true;
      touched = false;
      fault = CubePaintFault.None;
      return true;
    }),
    cancelPainting: vi.fn((): void => {
      painting = false;
    }),
    isPainting: vi.fn((): boolean => painting),
    // Empty, because this fake's session always begins from a scramble. The
    // painted path has its own tests, where it is not.
    originPainting: vi.fn((): number[] => []),
    setBrush: vi.fn((colour: CubeStickerColour): void => {
      brush = colour;
      touched = true;
    }),
    brush: vi.fn((): CubeStickerColour => brush),
    paintedCount: vi.fn((): number => cubeSize * cubeSize),
    applyPainting: vi.fn((): boolean => {
      if (touched) {
        fault = CubePaintFault.ColourCount;
        return false;
      }
      painting = false;
      fault = CubePaintFault.None;
      return true;
    }),
    paintFault: vi.fn((): CubePaintFault => fault),
    setFilling: vi.fn((wholeFace: boolean): boolean => {
      filling = wholeFace;
      return true;
    }),
    isFilling: vi.fn((): boolean => filling),
    isSolved: vi.fn((): boolean => solved),
    committedMoveCount: vi.fn((): number =>
      cursor > scrambleEnd ? cursor - scrambleEnd : 0,
    ),
    turnFace: vi.fn(
      (
        _face: CubeFace,
        firstDepth: number,
        lastDepth: number,
        _turns: number,
      ): boolean => {
        if (busy || painting) return false;
        if (firstDepth < 1 || lastDepth < firstDepth) return false;
        if (lastDepth - firstDepth + 1 >= cubeSize) return false;

        busy = true;
        return true;
      },
    ),
    setCubeSize: vi.fn((size: number): boolean => {
      if (size < MIN_CUBE_SIZE || size > MAX_CUBE_SIZE) return false;
      if (size !== cubeSize) {
        cubeSize = size;
        engine.resetCube();
      }
      return true;
    }),
    cubeSize: vi.fn((): number => cubeSize),
    setViewMode: vi.fn((mode: CubeViewMode): void => {
      viewMode = mode;
    }),
    viewMode: vi.fn((): CubeViewMode => viewMode),
    setFlatStyle: vi.fn((style: CubeFlatStyle): void => {
      flatStyle = style;
    }),
    flatStyle: vi.fn((): CubeFlatStyle => flatStyle),
    setPalette: vi.fn((chosen: CubePalette): void => {
      palette = chosen;
    }),
    palette: vi.fn((): CubePalette => palette),
    setSpeedScale: vi.fn((scale: number): void => {
      speedScale = Math.min(4, Math.max(0.25, scale));
    }),
    speedScale: vi.fn((): number => speedScale),
    resetView: vi.fn(),
    isBusy: vi.fn((): boolean => busy),
    render: vi.fn(),
  } satisfies GameEngine;

  let now = 0;
  let timerFrame: FrameRequestCallback | null = null;
  const timerEnvironment: TimerEnvironment = {
    now: () => now,
    requestFrame: vi.fn((callback: FrameRequestCallback): number => {
      timerFrame = callback;
      return 1;
    }),
    cancelFrame: vi.fn((): void => {
      timerFrame = null;
    }),
  };

  const keyListeners = new Set<(event: KeyboardEvent) => void>();
  const keyboardTarget: KeyboardTarget = {
    addEventListener: (_type, listener) => keyListeners.add(listener),
    removeEventListener: (_type, listener) => keyListeners.delete(listener),
  };
  const startFrameLoop = vi.fn();
  const onError = vi.fn();

  // A clipboard that keeps what it was handed, and can be made to refuse the
  // way a browser outside a secure context does.
  const copied: string[] = [];
  let copyFails = false;
  const shareTarget = {
    currentUrl: (): string => 'https://example.test/cube/',
    copy: vi.fn((text: string): Promise<void> => {
      if (copyFails) return Promise.reject(new Error('No clipboard.'));
      copied.push(text);
      return Promise.resolve();
    }),
  };
  // A sound that only counts, so the tests can hear it and jsdom is never
  // asked for an AudioContext it does not have.
  let muted = false;
  const sound = {
    play: vi.fn((): void => {}),
    setMuted: vi.fn((next: boolean): void => {
      muted = next;
    }),
    isMuted: vi.fn((): boolean => muted),
    teardown: vi.fn((): void => {}),
  };

  // Where the page leaves them before any controller attaches, so every test
  // here is also a test that attaching turns on what it should.
  for (const control of controllerControls(ui)) control.disabled = true;

  const controller = attachGameController({
    engine,
    ui,
    startFrameLoop,
    onError,
    randomSource: () => 1234,
    timerEnvironment,
    keyboardTarget,
    sound,
    shareTarget,
    prefersReducedMotion: overrides.prefersReducedMotion ?? false,
    blocked: overrides.blocked,
  });

  return {
    ui,
    engine,
    sound,
    controller,
    startFrameLoop,
    onError,
    shareTarget,
    copied,
    failCopy: (value: boolean): void => {
      copyFails = value;
    },
    setNow: (value: number): void => {
      now = value;
    },
    setSolved: (value: boolean): void => {
      solved = value;
    },
    /** Plays an accepted scramble out and lets the controller see it end. */
    finishScramble: (): void => {
      solved = false;
      cursor = SCRAMBLE_MOVES;
      busy = false;
      controller.afterEngineFrame();
    },
    /**
     * Commits one move of the user's own.
     *
     * Recorded at the cursor with everything past it discarded, which is the
     * one rule the engine applies -- so a move made after an undo throws away
     * what there was to redo here as well.
     */
    commitMove: (): void => {
      length = cursor;
      scrambleEnd = Math.min(scrambleEnd, cursor);
      moves = moves.slice(0, cursor);
      moves.push(0x40);
      length += 1;
      cursor += 1;
      busy = false;
    },
    /** Plays a rewind through to the end of its plan. */
    finishRewind: (): void => {
      if (rewindTo !== null) cursor = rewindTo;
      rewindTo = null;
      stoppable = false;
      busy = false;
      // Two ways to arrive at a solved cube: every move taken back off, or a
      // solution played all the way through.
      if (cursor === 0 && scrambleEnd > 0) solved = true;
      if (solving && cursor === length) solved = true;
      solving = false;
      controller.afterEngineFrame();
    },
    /** Frames of a rewind still playing, which the controls have to survive. */
    runRewindFrame: (): void => {
      controller.afterEngineFrame();
    },
    dispatchKey: (
      init: KeyboardEventInit,
      target?: EventTarget,
    ): KeyboardEvent => {
      const event = new KeyboardEvent('keydown', {
        cancelable: true,
        ...init,
      });
      // An undispatched event has no target, so tests that care about where
      // the key landed set one here.
      if (target !== undefined) {
        Object.defineProperty(event, 'target', { value: target });
      }
      for (const listener of keyListeners) listener(event);
      return event;
    },
    runTimerFrame: (): void => {
      const callback = timerFrame;
      timerFrame = null;
      callback?.(now);
    },
    keyListenerCount: (): number => keyListeners.size,
  };
}

describe('attachGameController', () => {
  it('starts idle with Both selected', () => {
    const harness = createHarness();

    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');
    expect(
      harness.ui.viewButtons.find((button) => button.dataset.view === 'both')
        ?.getAttribute('aria-pressed'),
    ).toBe('true');
  });

  it('scrambles into ready and reset restores only the solve session', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    expect(harness.engine.scramble).toHaveBeenCalledWith(1234, 20);

    // Not ready yet: the cube is being turned, and the frame loop that turns
    // it has to be asked for, because nothing else was running.
    expect(harness.controller.state).toBe('scrambling');
    expect(harness.startFrameLoop).toHaveBeenCalledTimes(1);
    expect(harness.ui.timer.value).toBe('00:00.00');

    harness.finishScramble();
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.status.textContent).toContain('timer starts');

    harness.ui.resetButton.click();
    expect(harness.engine.resetCube).toHaveBeenCalledTimes(1);
    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');
  });

  it('takes a scramble length and refuses one outside the range', () => {
    const harness = createHarness();
    const input = harness.ui.scrambleMovesInput;

    input.value = '7';
    input.dispatchEvent(new Event('change'));
    harness.ui.scrambleButton.click();
    expect(harness.engine.scramble).toHaveBeenLastCalledWith(1234, 7);

    // Put back rather than clamped, and the box says so.
    for (const refused of ['0', '101', '-3', '2.5', 'twenty']) {
      input.value = refused;
      input.dispatchEvent(new Event('change'));
      expect(input.value).toBe('7');
    }
    expect(harness.ui.status.textContent).toContain('Kept 7');

    harness.ui.scrambleButton.click();
    expect(harness.engine.scramble).toHaveBeenLastCalledWith(1234, 7);
  });

  it('puts a different cube on the table and starts the session over', () => {
    const harness = createHarness();

    // A session under way, so there is something for the new cube to clear.
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');

    const size = harness.ui.cubeSizeInput;
    size.value = '5';
    size.dispatchEvent(new Event('change'));

    expect(harness.engine.setCubeSize).toHaveBeenCalledWith(5);
    expect(harness.engine.cubeSize()).toBe(5);
    // Drawn here rather than left to a loop: nothing is animating after this,
    // so there would be no next frame to draw the new cube.
    expect(harness.engine.render).toHaveBeenCalled();
    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.moveLogList.children).toHaveLength(0);

    // And the depth control now reaches as far as the new cube allows.
    expect(harness.ui.turnDepthInput.max).toBe('4');
  });

  it('ends watching before it changes the cube, the way Reset does', () => {
    const harness = createHarness();

    harness.ui.ambientButton.click();
    expect(harness.engine.isAmbient()).toBe(true);

    harness.ui.cubeSizeInput.value = '4';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));

    expect(harness.engine.ambientStop).toHaveBeenCalled();
    expect(harness.engine.isAmbient()).toBe(false);
    expect(harness.engine.cubeSize()).toBe(4);
  });

  it('puts back a size no cube is built at, and says so', () => {
    const harness = createHarness();
    const size = harness.ui.cubeSizeInput;

    for (const refused of [
      '1',
      String(MAX_CUBE_SIZE + 1),
      '-3',
      '2.5',
      'four',
    ]) {
      size.value = refused;
      size.dispatchEvent(new Event('change'));
      expect(size.value).toBe('3');
      expect(size.dataset.refused).toBeDefined();
    }

    expect(harness.engine.setCubeSize).not.toHaveBeenCalled();
    expect(harness.ui.status.textContent).toContain('Kept 3×3');
  });

  it('turns as deep as the depth box says, wide or on its own', () => {
    const harness = createHarness();

    harness.ui.cubeSizeInput.value = '5';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));

    harness.ui.turnDepthInput.value = '2';
    harness.ui.turnDepthInput.dispatchEvent(new Event('change'));

    // Depth alone is the slice behind the face.
    harness.ui.moveButtons[0]!.click();
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
      2,
      2,
      1,
    );

    harness.commitMove();
    harness.controller.afterEngineFrame();

    // Wide brings everything above that depth with it, and the keyboard uses
    // the same setting the buttons do.
    harness.ui.turnWideButton.click();
    expect(harness.ui.turnWideButton.getAttribute('aria-pressed')).toBe('true');
    harness.dispatchKey({ key: 'r' });
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
      1,
      2,
      1,
    );
  });

  it('brings a depth left over from a larger cube back inside', () => {
    const harness = createHarness();

    harness.ui.cubeSizeInput.value = '7';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));
    harness.ui.turnDepthInput.value = '5';
    harness.ui.turnDepthInput.dispatchEvent(new Event('change'));

    harness.ui.cubeSizeInput.value = '3';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));

    // Nobody typed a five at a 3x3, so there is nothing to tell them about:
    // the depth is simply what this cube can do.
    expect(harness.ui.turnDepthInput.value).toBe('2');
    expect(harness.ui.turnDepthInput.dataset.refused).toBeUndefined();

    harness.ui.moveButtons[0]!.click();
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
      2,
      2,
      1,
    );
  });

  it('has nothing to ask about depth on a cube with two layers', () => {
    const harness = createHarness();

    harness.ui.cubeSizeInput.value = '2';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));

    // Both layers of a 2x2 are outer faces, so there is no depth to choose.
    expect(harness.ui.turnDepthInput.disabled).toBe(true);
    expect(harness.ui.turnWideButton.disabled).toBe(true);
    expect(harness.ui.turnDepthInput.value).toBe('1');
  });

  it('puts back a depth the cube in hand cannot reach', () => {
    const harness = createHarness();
    const depth = harness.ui.turnDepthInput;

    for (const refused of ['0', '3', '-1', '1.5', 'two']) {
      depth.value = refused;
      depth.dispatchEvent(new Event('change'));
      expect(depth.value).toBe('1');
    }
    expect(harness.ui.status.textContent).toContain('Kept 1');
  });

  it('leaves a refusal on the box, where scrambling cannot overwrite it', () => {
    const harness = createHarness();
    const input = harness.ui.scrambleMovesInput;

    input.value = '101';
    input.dispatchEvent(new Event('change'));
    expect(input.dataset.refused).toBeDefined();

    // The spoken half goes to the shared line, which the very next press
    // rewrites -- so the mark on the box has to outlive it.
    harness.ui.scrambleButton.click();
    expect(harness.ui.status.textContent).not.toContain('Kept');
    expect(input.dataset.refused).toBeDefined();

    // It lasts exactly as long as its flash.
    input.dispatchEvent(new Event('animationend'));
    expect(input.dataset.refused).toBeUndefined();
  });

  it('does not outlive a flash that was cancelled', () => {
    const harness = createHarness();
    const input = harness.ui.scrambleMovesInput;

    input.value = '101';
    input.dispatchEvent(new Event('change'));
    expect(input.dataset.refused).toBeDefined();

    // A panel closed while the box is flashing cancels the animation rather
    // than ending it, and the mark goes with it all the same.
    input.dispatchEvent(new Event('animationcancel'));
    expect(input.dataset.refused).toBeUndefined();
  });

  it('marks nothing on a box inside a panel that has closed', () => {
    const harness = createHarness();
    const input = harness.ui.scrambleMovesInput;
    // Escape closes the panel first, and the edit is committed by the focus
    // leaving it: the box is refused where nobody can see it.
    const panel = document.createElement('div');
    input.replaceWith(panel);
    panel.append(input);
    panel.hidden = true;

    input.value = '101';
    input.dispatchEvent(new Event('change'));
    expect(harness.ui.status.textContent).toContain('Kept 20');
    expect(input.value).toBe('20');
    expect(input.dataset.refused).toBeUndefined();
  });

  it('puts the refusal out as soon as the length is corrected', () => {
    const harness = createHarness();
    const input = harness.ui.scrambleMovesInput;

    input.value = '0';
    input.dispatchEvent(new Event('change'));
    expect(input.dataset.refused).toBeDefined();

    input.value = '9';
    input.dispatchEvent(new Event('change'));
    expect(input.dataset.refused).toBeUndefined();

    harness.ui.scrambleButton.click();
    expect(harness.engine.scramble).toHaveBeenLastCalledWith(1234, 9);
  });

  it('holds at scrambling while the cube is still turning', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();

    // Frames observed while the sequence plays change nothing, and no layer
    // can be turned by hand during them.
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('scrambling');
    expect(harness.ui.moveButtons[0]?.disabled).toBe(true);

    harness.finishScramble();
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.moveButtons[0]?.disabled).toBe(false);
  });

  it('starts on first commit and stops on a later solved commit', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');

    harness.setNow(1334);
    harness.runTimerFrame();
    expect(harness.ui.timer.value).toBe('00:01.23');

    harness.setSolved(true);
    harness.commitMove();
    harness.setNow(2445);
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.timer.value).toBe('00:02.34');
    expect(harness.ui.status.textContent).toBe(
      'Solved in 00:02.34. A new best.',
    );
  });

  it('does not start without a committed move', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.timer.value).toBe('00:00.00');
  });

  it('turn buttons and keyboard use the same animated engine command', () => {
    const harness = createHarness();
    harness.ui.moveButtons[0]!.click();

    expect(harness.engine.turnFace).toHaveBeenCalledWith(
      CubeFace.Right,
      1,
      1,
      1,
    );
    expect(harness.startFrameLoop).toHaveBeenCalledTimes(1);
    expect(harness.ui.moveButtons[0]!.disabled).toBe(true);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    const event = harness.dispatchKey({ key: 'R', shiftKey: true });
    expect(event.defaultPrevented).toBe(true);
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
      1,
      1,
      -1,
    );
    expect(harness.startFrameLoop).toHaveBeenCalledTimes(2);
  });

  it('ignores repeated, modified, and editable keyboard input', () => {
    const harness = createHarness();

    harness.dispatchKey({ key: 'r', repeat: true });
    harness.dispatchKey({ key: 'r', ctrlKey: true });
    harness.dispatchKey({ key: 'r', metaKey: true });
    harness.dispatchKey({ key: 'r' }, document.createElement('input'));
    harness.dispatchKey({ key: 'r' }, document.createElement('textarea'));

    // jsdom does not derive isContentEditable from the attribute, so the
    // property a browser would compute is supplied directly.
    const editable = document.createElement('div');
    Object.defineProperty(editable, 'isContentEditable', { value: true });
    harness.dispatchKey({ key: 'r' }, editable);

    expect(harness.engine.turnFace).not.toHaveBeenCalled();

    // The same key with nothing in the way still reaches the cube, so the
    // guards above are what rejected it rather than a listener that is gone.
    harness.dispatchKey({ key: 'r' });
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);
  });

  it('leaves the letters to a dialog that is up, wherever the focus is', () => {
    let dialogUp = true;
    const harness = createHarness({ blocked: () => dialogUp });

    // On a button in the dialog, and on the page itself -- which is where a
    // press on a dialog's label leaves the focus.
    const onButton = harness.dispatchKey(
      { key: 'r' },
      document.createElement('button'),
    );
    const onPage = harness.dispatchKey({ key: 'u' });
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
    expect(onButton.defaultPrevented).toBe(false);
    expect(onPage.defaultPrevented).toBe(false);

    // Closed, the same key turns: the guard refused it, not a missing listener.
    dialogUp = false;
    harness.dispatchKey({ key: 'r' });
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);
  });

  it('sends the slider to the engine and writes back what it took', () => {
    const harness = createHarness();

    expect(harness.ui.speedValue.value).toBe('1.00×');

    harness.ui.speedInput.value = '2';
    harness.ui.speedInput.dispatchEvent(new Event('input'));
    expect(harness.engine.setSpeedScale).toHaveBeenCalledWith(2);
    expect(harness.ui.speedValue.value).toBe('2.00×');

    // Read back rather than echoed: the engine clamps, so a value past the end
    // has to come back as the end or the number would be describing a speed
    // the cube is not running at.
    harness.ui.speedInput.value = '9';
    harness.ui.speedInput.dispatchEvent(new Event('input'));
    expect(harness.ui.speedValue.value).toBe('4.00×');
    expect(harness.ui.speedInput.value).toBe('4');
  });

  it('opens quicker for someone who asked for less motion, but not fixed', () => {
    const harness = createHarness({ prefersReducedMotion: true });

    // Offered, not imposed: the animation is what the app is for, so the
    // slider opens at the quick end and moves back like any other.
    expect(harness.engine.setSpeedScale).toHaveBeenCalledWith(2);
    expect(harness.ui.speedValue.value).toBe('2.00×');

    harness.ui.speedInput.value = '0.5';
    harness.ui.speedInput.dispatchEvent(new Event('input'));
    expect(harness.ui.speedValue.value).toBe('0.50×');
  });

  it('leaves the speed live while a sequence is playing', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.controller.afterEngineFrame();

    // Like the palette, and for the same reason: it is not a command to the
    // cube. A turn already running keeps its own duration, so the frames of
    // the sequence it is changed under do not jump.
    expect(harness.ui.speedInput.disabled).toBe(false);

    harness.ui.speedInput.value = '4';
    harness.ui.speedInput.dispatchEvent(new Event('input'));
    expect(harness.engine.setSpeedScale).toHaveBeenCalledWith(4);
  });

  it('releases the sound when setup never finishes', () => {
    const harness = createHarness();
    harness.controller.teardown();

    const sound = {
      play: vi.fn(),
      setMuted: vi.fn(),
      isMuted: vi.fn((): boolean => false),
      teardown: vi.fn(),
    };
    const broken = {
      ...harness.engine,
      palette: vi.fn((): CubePalette => {
        throw new Error('engine gone');
      }),
    };

    // The sound is listening for a gesture from the moment it is made, which
    // is before there is a controller to hand back. A setup that throws leaves
    // the caller with an exception and nothing to call teardown on, so the
    // release has to happen on the way out.
    expect(() =>
      attachGameController({
        engine: broken,
        ui: harness.ui,
        startFrameLoop: vi.fn(),
        onError: vi.fn(),
        sound,
      }),
    ).toThrow('engine gone');
    expect(sound.teardown).toHaveBeenCalledTimes(1);
  });

  it('leaves nothing listening when the last step of setup fails', () => {
    const harness = createHarness();
    harness.controller.teardown();
    for (const control of controllerControls(harness.ui)) {
      control.disabled = true;
    }

    // Everything is wired by the time the controls are first read, so this
    // is the failure with the most to leave behind: listeners on the keyboard
    // and every control, and the controls just turned on.
    const keys = new Set<(event: KeyboardEvent) => void>();
    const sound = {
      play: vi.fn(),
      setMuted: vi.fn(),
      isMuted: vi.fn((): boolean => false),
      teardown: vi.fn(),
    };
    const broken = {
      ...harness.engine,
      palette: vi.fn((): CubePalette => {
        throw new Error('engine gone');
      }),
    };
    expect(() =>
      attachGameController({
        engine: broken,
        ui: harness.ui,
        startFrameLoop: vi.fn(),
        onError: vi.fn(),
        sound,
        keyboardTarget: {
          addEventListener: (_type, listener) => keys.add(listener),
          removeEventListener: (_type, listener) => keys.delete(listener),
        },
      }),
    ).toThrow('engine gone');

    expect(keys.size).toBe(0);
    expect(sound.teardown).toHaveBeenCalledTimes(1);
    for (const control of controllerControls(harness.ui)) {
      expect(control.disabled, control.id || control.textContent).toBe(true);
    }
    // Pressed anyway, nothing reaches the engine.
    for (const method of [broken.resetCube, broken.scramble]) {
      (method as ReturnType<typeof vi.fn>).mockClear();
    }
    harness.ui.resetButton.dispatchEvent(new MouseEvent('click'));
    harness.ui.scrambleButton.dispatchEvent(new MouseEvent('click'));
    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail: { text: 'R' } }),
    );
    expect(broken.resetCube).not.toHaveBeenCalled();
    expect(broken.scramble).not.toHaveBeenCalled();
    expect(broken.turnFace).not.toHaveBeenCalled();
  });

  it('reports the setup failure, not the sound failing on the way out', () => {
    const harness = createHarness();
    harness.controller.teardown();

    for (const failing of ['palette', 'timelineCursor'] as const) {
      const sound = {
        play: vi.fn(),
        setMuted: vi.fn(),
        isMuted: vi.fn((): boolean => false),
        teardown: vi.fn((): void => {
          throw new Error('sound broke');
        }),
      };
      const broken = {
        ...harness.engine,
        [failing]: vi.fn(() => {
          throw new Error('engine gone');
        }),
      };
      // The palette fails in the last step, with everything wired; the
      // cursor as the session is made, before anything is.
      expect(
        () =>
          attachGameController({
            engine: broken,
            ui: harness.ui,
            startFrameLoop: vi.fn(),
            onError: vi.fn(),
            sound,
            keyboardTarget: {
              addEventListener: vi.fn(),
              removeEventListener: vi.fn(),
            },
          }),
        failing,
      ).toThrow('engine gone');
    }
  });

  it('finishes unhooking even when closing the sound fails', () => {
    const harness = createHarness();
    expect(harness.keyListenerCount()).toBe(1);

    harness.sound.teardown.mockImplementation((): void => {
      throw new Error('context would not close');
    });

    // Closing an audio context is the one step of teardown that is a browser
    // call rather than a listener being unhooked, so it goes last: a throw
    // from it reaches the caller with everything else already undone.
    expect(() => harness.controller.teardown()).toThrow(
      'context would not close',
    );
    expect(harness.keyListenerCount()).toBe(0);
  });

  it('sounds once on every frame a move committed, and not otherwise', () => {
    const harness = createHarness();

    // A frame with nothing on it is silent, however many of them run.
    harness.controller.afterEngineFrame();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).not.toHaveBeenCalled();

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    // The frames after it are silent again: it is the change that sounds, not
    // the cube being somewhere other than where it started.
    harness.controller.afterEngineFrame();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(2);
  });

  it('sounds a rewind, in the direction it is going', () => {
    const harness = createHarness();

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    // Undo moves the cursor too, so taking a move back is heard exactly as
    // making it was. Which way it went is not something an ear can tell.
    harness.ui.undoButton.click();
    harness.finishRewind();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(2);
  });

  it('stays silent through a reset and through watching', () => {
    const harness = createHarness();

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    // A reset throws the record away, which is a far bigger jump than any
    // commit -- and it is not one. The baseline the session takes when the
    // command runs is what keeps it from being heard as one.
    harness.ui.resetButton.click();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    // Watching turns the cube without touching the record, so the frames it
    // runs are silent with nothing here having to ask whether it is on.
    harness.ui.ambientButton.click();
    harness.controller.afterEngineFrame();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);
  });

  it('mutes and unmutes, and says which it is', () => {
    const harness = createHarness();

    expect(harness.ui.muteButton.getAttribute('aria-pressed')).toBe('false');

    harness.ui.muteButton.click();
    expect(harness.sound.setMuted).toHaveBeenCalledWith(true);
    expect(harness.ui.muteButton.getAttribute('aria-pressed')).toBe('true');

    // Muting is the sound's own business, not a reason to stop watching the
    // record: the session goes on asking and the sound goes on refusing.
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).toHaveBeenCalledTimes(1);

    harness.ui.muteButton.click();
    expect(harness.sound.setMuted).toHaveBeenLastCalledWith(false);
    expect(harness.ui.muteButton.getAttribute('aria-pressed')).toBe('false');
  });

  it('releases the sound when the controller goes', () => {
    const harness = createHarness();

    harness.controller.teardown();
    expect(harness.sound.teardown).toHaveBeenCalledTimes(1);
    expect(harness.ui.muteButton.disabled).toBe(true);
  });

  it('chooses a palette without touching the cube, and says which is on', () => {
    const harness = createHarness();
    const classic = harness.ui.paletteButtons.find(
      (button) => button.dataset.palette === 'classic',
    )!;
    const highContrast = harness.ui.paletteButtons.find(
      (button) => button.dataset.palette === 'high-contrast',
    )!;

    expect(classic.getAttribute('aria-pressed')).toBe('true');
    expect(highContrast.getAttribute('aria-pressed')).toBe('false');

    highContrast.click();
    expect(harness.engine.setPalette).toHaveBeenCalledWith(
      CubePalette.HighContrast,
    );
    expect(highContrast.getAttribute('aria-pressed')).toBe('true');
    expect(classic.getAttribute('aria-pressed')).toBe('false');

    // Drawn at once: with nothing moving there is no next frame to wait for.
    expect(harness.engine.render).toHaveBeenCalled();

    // A palette is not a command to the cube.
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
    expect(harness.engine.resetCube).not.toHaveBeenCalled();
    expect(harness.engine.scramble).not.toHaveBeenCalled();

    classic.click();
    expect(harness.engine.setPalette).toHaveBeenLastCalledWith(
      CubePalette.Classic,
    );
    expect(classic.getAttribute('aria-pressed')).toBe('true');
  });

  it('leaves the palette live while a sequence is playing', () => {
    const harness = createHarness();
    const highContrast = harness.ui.paletteButtons.find(
      (button) => button.dataset.palette === 'high-contrast',
    )!;

    harness.ui.scrambleButton.click();
    harness.controller.afterEngineFrame();

    // The cube's own controls are out while it plays, and this one is not:
    // reading the board is most wanted exactly while it is moving.
    expect(harness.ui.moveButtons[0]!.disabled).toBe(true);
    expect(highContrast.disabled).toBe(false);

    highContrast.click();
    expect(harness.engine.setPalette).toHaveBeenCalledWith(
      CubePalette.HighContrast,
    );
  });

  it('switches regions and flat style separately, and resets only the camera', () => {
    const harness = createHarness();
    const flat = harness.ui.viewButtons.find(
      (button) => button.dataset.view === '2d',
    )!;
    const cube = harness.ui.viewButtons.find(
      (button) => button.dataset.view === '3d',
    )!;
    const rings = harness.ui.flatButtons.find(
      (button) => button.dataset.flat === 'rings',
    )!;

    flat.click();
    expect(harness.engine.setViewMode).toHaveBeenCalledWith(CubeViewMode.Flat);
    expect(flat.getAttribute('aria-pressed')).toBe('true');

    // The style is the other axis, so choosing it does not touch the regions.
    rings.click();
    expect(harness.engine.setFlatStyle).toHaveBeenCalledWith(
      CubeFlatStyle.Rings,
    );
    expect(rings.getAttribute('aria-pressed')).toBe('true');
    expect(flat.getAttribute('aria-pressed')).toBe('true');
    expect(harness.ui.stage.dataset.flatStyle).toBe('rings');
    expect(harness.ui.stage.dataset.viewMode).toBe('2d');

    // And it survives the flat view going away and coming back, while the
    // toggle itself is put out of the way meanwhile.
    cube.click();
    expect(rings.hidden).toBe(true);
    flat.click();
    expect(rings.hidden).toBe(false);
    expect(rings.getAttribute('aria-pressed')).toBe('true');

    harness.ui.homeViewButton.click();
    expect(harness.engine.resetView).toHaveBeenCalledTimes(1);
    expect(harness.engine.resetCube).not.toHaveBeenCalled();
  });

  it('teardown removes controls, keyboard, and timer work', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.keyListenerCount()).toBe(1);

    harness.controller.teardown();
    expect(harness.keyListenerCount()).toBe(0);

    harness.ui.resetButton.click();
    harness.dispatchKey({ key: 'r' });
    expect(harness.engine.resetCube).not.toHaveBeenCalled();
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
  });

  it('watches from a session that is not under way, and not from one that is', () => {
    const harness = createHarness();
    const watch = harness.ui.ambientButton;

    expect(watch.disabled).toBe(false);

    watch.click();
    expect(harness.engine.ambientStart).toHaveBeenCalledWith(1234);
    expect(watch.getAttribute('aria-pressed')).toBe('true');
    expect(harness.startFrameLoop).toHaveBeenCalledTimes(1);
    expect(harness.ui.status.textContent).toContain('Look around freely');

    // Watching makes the engine busy, but the move buttons stay live through
    // it: pressing one is a way out of it, the same as the letter it carries.
    expect(harness.ui.moveButtons[0]?.disabled).toBe(false);

    // Pressing again stops it rather than stopping and starting it again.
    watch.click();
    expect(harness.engine.ambientStop).toHaveBeenCalledTimes(1);
    expect(harness.engine.ambientStart).toHaveBeenCalledTimes(1);
    expect(watch.getAttribute('aria-pressed')).toBe('false');
    expect(harness.ui.status.textContent).toBe('Watching stopped.');
    expect(harness.ui.moveButtons[0]?.disabled).toBe(false);

    // A scramble arms the clock, and from there the offer is withdrawn.
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(harness.controller.state).toBe('ready');
    expect(watch.disabled).toBe(true);
  });

  it('is offered again once a solve is over', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(harness.ui.ambientButton.disabled).toBe(true);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');
    expect(harness.ui.ambientButton.disabled).toBe(true);

    harness.setSolved(true);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.ambientButton.disabled).toBe(false);
  });

  it('stops watching before doing what a cube command was pressed for', () => {
    const harness = createHarness();

    for (const press of [
      (): void => harness.ui.resetButton.click(),
      (): void => harness.ui.scrambleButton.click(),
      (): void => harness.ui.moveButtons[0]!.click(),
      (): void => {
        harness.dispatchKey({ key: 'r' });
      },
    ]) {
      harness.ui.ambientButton.click();
      expect(harness.engine.isAmbient()).toBe(true);

      press();
      expect(harness.engine.isAmbient()).toBe(false);
      expect(harness.ui.ambientButton.getAttribute('aria-pressed')).toBe(
        'false',
      );

      // Back to a cube that can be watched again, whatever the press did.
      harness.ui.resetButton.click();
    }

    // Every one of them did its own work as well as stopping the watching.
    expect(harness.engine.scramble).toHaveBeenCalledTimes(1);
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(2);
  });

  it('keeps watching through the controls that only change the view', () => {
    const harness = createHarness();
    harness.ui.ambientButton.click();

    // The same answer these give a scramble part way through: which regions
    // are on screen, which drawing fills the flat one and where the camera
    // sits are ways of looking at the cube, not things done to it.
    for (const press of [
      (): void => harness.ui.viewButtons[0]!.click(),
      (): void => harness.ui.flatButtons[1]!.click(),
      (): void => harness.ui.homeViewButton.click(),
    ]) {
      press();
      expect(harness.engine.isAmbient()).toBe(true);
      expect(harness.ui.ambientButton.getAttribute('aria-pressed')).toBe(
        'true',
      );
    }

    expect(harness.engine.setViewMode).toHaveBeenCalledTimes(1);
    expect(harness.engine.setFlatStyle).toHaveBeenCalledTimes(1);
    expect(harness.engine.resetView).toHaveBeenCalledTimes(1);
    expect(harness.engine.ambientStop).not.toHaveBeenCalled();
  });

  it('lets a control say its own thing over the end of watching', () => {
    const harness = createHarness();

    harness.ui.ambientButton.click();
    harness.ui.resetButton.click();

    expect(harness.ui.status.textContent).toBe('Cube reset.');
    harness.controller.afterEngineFrame();
    expect(harness.ui.status.textContent).toBe('Cube reset.');
  });

  it('leaves nothing of a watched pattern in the session', () => {
    const harness = createHarness();

    harness.ui.ambientButton.click();
    // Frames of a pattern playing: busy throughout, and no move of it is the
    // user's, so the session stays exactly where it was.
    harness.controller.afterEngineFrame();
    harness.controller.afterEngineFrame();

    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');

    harness.ui.ambientButton.click();
    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');
  });

  it('teardown puts the watch toggle out with the rest', () => {
    const harness = createHarness();
    harness.controller.teardown();

    expect(harness.ui.ambientButton.disabled).toBe(true);
    harness.ui.ambientButton.click();
    expect(harness.engine.ambientStart).not.toHaveBeenCalled();
  });

  it('offers each rewind exactly where the record allows it', () => {
    const harness = createHarness();
    const { undoButton, redoButton, rewindButton } = harness.ui;

    // Nothing has happened, so there is nothing to walk back along.
    expect(undoButton.disabled).toBe(true);
    expect(redoButton.disabled).toBe(true);
    expect(rewindButton.disabled).toBe(true);

    harness.ui.scrambleButton.click();
    harness.finishScramble();

    // A scramble can be rewound but not undone: what is below the end of the
    // scramble is not the user's to take back.
    expect(undoButton.disabled).toBe(true);
    expect(rewindButton.disabled).toBe(false);
    expect(redoButton.disabled).toBe(true);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(undoButton.disabled).toBe(false);

    harness.ui.undoButton.click();
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
    expect(harness.startFrameLoop).toHaveBeenCalled();

    // While it plays, nothing else may be asked for.
    harness.runRewindFrame();
    expect(undoButton.disabled).toBe(true);
    expect(rewindButton.disabled).toBe(true);

    harness.finishRewind();
    expect(harness.engine.committedMoveCount()).toBe(0);
    expect(undoButton.disabled).toBe(true);
    expect(redoButton.disabled).toBe(false);

    harness.ui.redoButton.click();
    harness.finishRewind();
    expect(harness.engine.committedMoveCount()).toBe(1);
    expect(redoButton.disabled).toBe(true);
    expect(undoButton.disabled).toBe(false);
  });

  it('shows Stop only while a rewind is playing, and breaks it off', () => {
    const harness = createHarness();
    const stop = harness.ui.stopButton;

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(stop.hidden).toBe(true);

    // A scramble is playing here and Stop is still not on offer: what it can
    // break off is a rewind, and the engine is what says so.
    harness.ui.scrambleButton.click();
    expect(stop.hidden).toBe(true);
    harness.finishScramble();

    harness.commitMove();
    harness.controller.afterEngineFrame();
    harness.ui.rewindButton.click();
    expect(harness.ui.status.textContent).toContain('Press Stop');
    expect(stop.hidden).toBe(false);

    stop.click();
    expect(harness.engine.stopPlayback).toHaveBeenCalledTimes(1);
    expect(stop.hidden).toBe(true);
    expect(harness.ui.status.textContent).toBe('Stopped.');

    // Stopped where it was, so both directions are open again from there.
    expect(harness.ui.rewindButton.disabled).toBe(false);
    expect(harness.ui.redoButton.disabled).toBe(false);
  });

  it('does not start the clock for a rewind, only for a move of your own', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(harness.controller.state).toBe('ready');

    // Solve from a ready cube: the rewind commits moves all the way down, and
    // not one of them is the user's, so the clock never starts.
    harness.setNow(100);
    harness.ui.rewindButton.click();
    harness.runRewindFrame();
    expect(harness.controller.state).toBe('ready');

    harness.setNow(5000);
    harness.finishRewind();
    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.timer.value).toBe('00:00.00');
    expect(harness.ui.status.textContent).toContain('Not a solve of your own');
  });

  it('tells a cube it rewound apart from one you finished yourself', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');

    // One move too many, then taken back: the cube is solved by an undo, and
    // it is still the user's own solve because moves of theirs are on it.
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.setSolved(true);
    harness.ui.undoButton.click();
    harness.setNow(2445);
    harness.finishRewind();

    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.timer.value).toBe('00:02.34');
    expect(harness.ui.status.textContent).toBe(
      'Solved in 00:02.34. A new best.',
    );
  });

  it('offers Solve only for a cube a solver here can take on', () => {
    const harness = createHarness();
    const { solveButton, solverNote } = harness.ui;

    // Nothing to solve on a cube already solved, and the note stays away:
    // this is a wait rather than a refusal.
    expect(solveButton.disabled).toBe(true);
    expect(solverNote.hidden).toBe(true);

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(solveButton.disabled).toBe(false);

    // Every size this application builds has a solver now, so the engine is
    // asked to say otherwise -- what is under test is the screen's answer to
    // "no solver for this one", which stays worth having whether or not any
    // size in this build gives it.
    harness.engine.canSolve.mockReturnValue(false);
    harness.controller.afterEngineFrame();
    expect(solveButton.disabled).toBe(true);
    expect(solverNote.hidden).toBe(false);

    harness.engine.canSolve.mockReturnValue(true);
    harness.controller.afterEngineFrame();
    expect(solverNote.hidden).toBe(true);
  });

  it('has a solver for every size it builds', () => {
    const harness = createHarness();

    for (const size of ['2', '4', '6', '9']) {
      harness.ui.cubeSizeInput.value = size;
      harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));
      harness.controller.afterEngineFrame();

      expect(harness.engine.canSolve()).toBe(true);
      expect(harness.ui.solverNote.hidden).toBe(true);
    }
  });

  it('warns about the wait before a big cube is handed to the solver', () => {
    const harness = createHarness();
    const { solveButton, solverNote } = harness.ui;

    const setSize = (size: number): void => {
      harness.ui.cubeSizeInput.value = String(size);
      harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));
      harness.controller.afterEngineFrame();
    };

    setSize(SOLVE_WARNING_CUBE_SIZE - 1);
    expect(solverNote.hidden).toBe(true);

    setSize(MAX_CUBE_SIZE);

    // Readable before the press, which is the only moment it can be read:
    // the solve is one call into the engine and the page is gone until it
    // comes back. And Solve stays offered -- it does work, it is slow.
    expect(solverNote.hidden).toBe(false);
    expect(solverNote.textContent).toContain(
      `${MAX_CUBE_SIZE}×${MAX_CUBE_SIZE}`,
    );
    expect(solverNote.textContent).toContain('will not respond');
    expect(solverNote.textContent).toContain('Rewind');

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(solveButton.disabled).toBe(false);
    expect(solverNote.hidden).toBe(false);

    // And a build with no solver for the size still says the other thing,
    // which is a different fact and a different sentence.
    harness.engine.canSolve.mockReturnValue(false);
    harness.controller.afterEngineFrame();
    expect(solverNote.textContent).toContain('No solver for this cube size');
  });

  it('plays a solution and says the cube was not solved by you', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');

    harness.setNow(3000);
    harness.ui.solveButton.click();
    expect(harness.engine.solve).toHaveBeenCalledTimes(1);
    expect(harness.ui.status.textContent).toContain('Press Stop');
    expect(harness.ui.stopButton.hidden).toBe(false);

    // The clock stops the moment the help is asked for, not when the cube
    // comes out solved.
    expect(harness.ui.timer.value).toBe('00:02.90');

    harness.setNow(9000);
    harness.finishRewind();

    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.timer.value).toBe('00:02.90');
    expect(harness.ui.status.textContent).toBe(
      'Solved by the solver. Not a solve of your own.',
    );
    expect(harness.ui.recordList.children).toHaveLength(0);
  });

  it('keeps a sitting the solver was let into out of the records for good', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    expect(harness.controller.state).toBe('ready');

    // Asked for from a ready cube, so the clock was never started -- and the
    // solver's own moves are the user's in the record, which is exactly what
    // would otherwise start it.
    harness.setNow(100);
    harness.ui.solveButton.click();
    harness.runRewindFrame();
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.timer.value).toBe('00:00.00');

    // Broken off part way and finished by hand. Half a solution is still a
    // solution somebody was shown, so the sitting does not come back.
    harness.ui.stopButton.click();
    harness.setNow(4000);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.timer.value).toBe('00:00.00');

    harness.setSolved(true);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.status.textContent).toContain('Not a solve of your own');
    expect(harness.ui.recordList.children).toHaveLength(0);
  });

  it('a scramble gives the sitting back to whoever is playing it', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.ui.solveButton.click();
    harness.finishRewind();
    expect(harness.ui.status.textContent).toContain('Not a solve of your own');

    // A new scramble is a new sitting, and this one is the person's own.
    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('running');

    harness.setSolved(true);
    harness.setNow(1600);
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.ui.status.textContent).toContain('Solved in');
    expect(harness.ui.recordList.children).toHaveLength(1);
  });

  it('stops watching before a rewind, the way every cube command does', () => {
    const harness = createHarness();

    // A move made outside a solve session, so the cube can still be watched
    // and there is something on the record to take back.
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('idle');

    harness.ui.ambientButton.click();
    expect(harness.engine.isAmbient()).toBe(true);
    expect(harness.ui.undoButton.disabled).toBe(false);

    // Taking a move back is something done to the cube, so the interlude ends
    // first and the undo lands on the cube it gave back.
    harness.ui.undoButton.click();
    expect(harness.engine.isAmbient()).toBe(false);
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
  });

  it('teardown puts the rewinds out with the rest', () => {
    const harness = createHarness();
    harness.controller.teardown();

    for (const button of [
      harness.ui.undoButton,
      harness.ui.redoButton,
      harness.ui.rewindButton,
      harness.ui.stopButton,
    ]) {
      expect(button.disabled).toBe(true);
      button.click();
    }

    expect(harness.engine.undo).not.toHaveBeenCalled();
    expect(harness.engine.redo).not.toHaveBeenCalled();
    expect(harness.engine.solveRewind).not.toHaveBeenCalled();
    expect(harness.engine.stopPlayback).not.toHaveBeenCalled();
  });

  it('writes your own moves out and follows them as the cursor moves', () => {
    const harness = createHarness();
    const log = harness.ui.moveLogList;

    /** What each entry says and where it stands, for one readable row. */
    const entries = (): string[] =>
      [...log.children].map((child) => {
        const item = child as HTMLElement;
        return `${item.textContent} ${item.dataset.state}`;
      });

    // Nothing has been turned, so there is nothing written down.
    expect(entries()).toEqual([]);

    // A scramble is the cube you were handed rather than anything you did,
    // and it stays out of the list from the moment it is accepted to the
    // moment it has all arrived.
    harness.ui.scrambleButton.click();
    expect(entries()).toEqual([]);
    harness.finishScramble();
    expect(entries()).toEqual([]);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(entries()).toEqual(["R' current"]);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(entries()).toEqual(["R' applied", "R' current"]);

    // Taken back: the move stays on the list, because it is still there to be
    // put back, and the mark moves down to what is left on the cube.
    harness.ui.undoButton.click();
    harness.finishRewind();
    expect(entries()).toEqual(["R' current", "R' pending"]);

    harness.ui.redoButton.click();
    harness.finishRewind();
    expect(entries()).toEqual(["R' applied", "R' current"]);

    // A solve walks the cursor down past the end of the scramble. Both moves
    // are waiting to be put back and neither is on the cube, so nothing is
    // marked -- and the scramble it rewound through is still not on the list.
    harness.ui.rewindButton.click();
    harness.finishRewind();
    expect(entries()).toEqual(["R' pending", "R' pending"]);

    // A new cube has no record at all.
    harness.ui.resetButton.click();
    expect(entries()).toEqual([]);
  });

  it('routes command failures to the lifecycle error handler', () => {
    const harness = createHarness();
    harness.engine.scramble.mockImplementationOnce(() => {
      throw new Error('scramble failed');
    });

    harness.ui.scrambleButton.click();
    expect(harness.onError).toHaveBeenCalledWith(
      expect.objectContaining({ message: 'scramble failed' }),
    );
  });
});

describe('sharing the cube', () => {
  /** A copy the test finishes when it chooses, the way a slow clipboard would. */
  function held(): {
    promise: Promise<void>;
    resolve: () => void;
    reject: () => void;
  } {
    let resolve = (): void => {};
    let reject = (): void => {};
    const promise = new Promise<void>((done, fail) => {
      resolve = (): void => done();
      reject = (): void => fail(new Error('No clipboard.'));
    });
    return { promise, resolve, reject };
  }

  /** Lets every promise that has settled run its callbacks. */
  const settle = async (): Promise<void> => {
    for (let turn = 0; turn < 5; turn += 1) await Promise.resolve();
  };

  it('shows nothing for a copy that finishes after the cube has moved on', async () => {
    for (const ending of ['resolve', 'reject'] as const) {
      const harness = createHarness();
      harness.commitMove();
      harness.controller.afterEngineFrame();

      const copy = held();
      harness.shareTarget.copy.mockImplementationOnce(() => copy.promise);
      harness.ui.shareButton.click();

      // Reset starts no frame, so nothing else would take a card down again.
      harness.ui.resetButton.click();
      const said = harness.ui.status.textContent;
      copy[ending]();
      await settle();

      expect(harness.ui.shareCard.card.hidden, ending).toBe(true);
      expect(harness.ui.status.textContent, ending).toBe(said);
      expect(document.activeElement, ending).not.toBe(
        harness.ui.shareCard.link,
      );
    }
  });

  it('shows the newest of two copies, whichever finishes first', async () => {
    const harness = createHarness();
    harness.commitMove();
    harness.controller.afterEngineFrame();

    const first = held();
    harness.shareTarget.copy.mockImplementationOnce(() => first.promise);
    harness.ui.shareButton.click();
    harness.commitMove();
    harness.controller.afterEngineFrame();
    const second = held();
    harness.shareTarget.copy.mockImplementationOnce(() => second.promise);
    harness.ui.shareButton.click();

    second.resolve();
    await settle();
    first.resolve();
    await settle();

    // Two of the user's own moves: the second link, and still up a frame on.
    const link = harness.ui.shareCard.link.value;
    expect(harness.ui.shareCard.card.hidden).toBe(false);
    expect(decodeSession(link.split('#s=')[1]!)?.user).toHaveLength(2);
    harness.controller.afterEngineFrame();
    expect(harness.ui.shareCard.card.hidden).toBe(false);
  });

  it('carries the size of an untouched cube that is not the opening one', async () => {
    const harness = createHarness();
    harness.ui.cubeSizeInput.value = '4';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    // The plain address would open a three by three.
    expect(decodeSession(harness.copied[0]!.split('#s=')[1]!)).toEqual({
      size: 4,
      scramble: [],
      user: [],
    });
    expect(harness.ui.status.textContent).toBe(
      'Link copied. It opens this cube.',
    );
  });

  it('takes the card down when a colouring becomes the cube', async () => {
    const harness = createHarness();
    harness.ui.shareButton.click();
    await vi.waitFor(() =>
      expect(harness.ui.shareCard.card.hidden).toBe(false),
    );

    harness.ui.paintButton.click();
    harness.ui.paintApplyButton.click();
    expect(harness.engine.isPainting()).toBe(false);
    expect(harness.ui.shareCard.card.hidden).toBe(true);
  });

  it('sends the plain address for a cube nothing has happened to', async () => {
    const harness = createHarness();

    // Pressable from the first moment. There is no record to encode, and the
    // page on its own opens the cube on the screen, so that is what travels.
    expect(harness.ui.shareButton.disabled).toBe(false);

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    expect(harness.copied[0]).toBe('https://example.test/cube/');
    expect(harness.copied[0]).not.toContain('#');
    expect(harness.ui.status.textContent).toBe(
      'Link copied. It opens a fresh cube.',
    );

    // And the link itself goes up on the card by the button.
    const card = harness.ui.shareCard;
    await vi.waitFor(() => expect(card.card.hidden).toBe(false));
    expect(card.card.dataset.copied).toBe('true');
    expect(card.title.textContent).toBe('Link copied');
    expect(card.note.textContent).toBe('It opens a fresh cube.');
    expect(card.link.value).toBe('https://example.test/cube/');
  });

  it('carries the record once there is one', async () => {
    const harness = createHarness();

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.ui.shareButton.disabled).toBe(false);

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    expect(harness.copied[0]).toContain('#s=');
    expect(harness.ui.status.textContent).toBe(
      'Link copied. It opens this cube.',
    );
  });

  it('copies a link carrying the record as it stands', async () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    const url = new URL(harness.copied[0]!);
    expect(url.origin + url.pathname).toBe('https://example.test/cube/');

    // The scramble the cube was handed and the one move made on top of it,
    // which is exactly what the record holds.
    const shared = decodeSession(url.hash.replace('#s=', ''));
    expect(shared).toEqual({
      size: 3,
      scramble: [0x44, 0x45, 0x46],
      user: [0x40],
    });
    expect(harness.ui.status.textContent).toBe('Link copied. It opens this cube.');
  });

  it('leaves out the moves the sender took back', async () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.commitMove();
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.ui.undoButton.click();
    harness.finishRewind();

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    // Two were made and one was withdrawn, so one travels: what is shared is
    // a cube, and a move its sender undid is not part of one.
    const shared = decodeSession(
      new URL(harness.copied[0]!).hash.replace('#s=', ''),
    );
    expect(shared?.user).toEqual([0x40]);
  });

  it('says so when the clipboard will not take it', async () => {
    const harness = createHarness();
    harness.failCopy(true);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    harness.ui.shareButton.click();

    // Sharing is an extra, so a browser that cannot copy is something to
    // mention rather than a failure that takes the application down.
    await vi.waitFor(() =>
      expect(harness.ui.status.textContent).toBe('Could not copy the link.'),
    );
    expect(harness.onError).not.toHaveBeenCalled();

    // The link still reaches the card, where it can be copied by hand, and
    // the keyboard is put on it with the whole of it selected.
    const card = harness.ui.shareCard;
    expect(card.card.hidden).toBe(false);
    expect(card.card.dataset.copied).toBe('false');
    expect(card.title.textContent).toBe('Copy this link');
    expect(card.link.value).toContain('#s=');
    expect(document.activeElement).toBe(card.link);
  });

  it('takes the card down once the cube is somewhere else', async () => {
    const harness = createHarness();
    harness.failCopy(true);
    harness.commitMove();
    harness.controller.afterEngineFrame();

    // A refused copy stays up until it is closed -- or until the cube it
    // opens is no longer the one on the table, here by a drag.
    harness.ui.shareButton.click();
    const card = harness.ui.shareCard;
    await vi.waitFor(() => expect(card.card.hidden).toBe(false));
    harness.controller.afterEngineFrame();
    expect(card.card.hidden).toBe(false);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(card.card.hidden).toBe(true);

    // And by a command, which says so before it runs.
    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(card.card.hidden).toBe(false));
    harness.ui.resetButton.click();
    expect(card.card.hidden).toBe(true);
  });

  it('is out of reach while anything is playing', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.controller.afterEngineFrame();
    expect(harness.ui.shareButton.disabled).toBe(true);

    harness.finishScramble();
    expect(harness.ui.shareButton.disabled).toBe(false);
  });

  it('is out of reach for a solve stopped inside the scramble', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();

    harness.ui.rewindButton.click();
    harness.ui.stopButton.click();

    // Below the boundary the payload has no shape for: the two counts say
    // where the scramble stops, and nothing in them says how much of it is on
    // the cube.
    expect(harness.ui.shareButton.disabled).toBe(true);
  });

  it('stops watching before it reads the cube', async () => {
    const harness = createHarness();

    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.ui.ambientButton.click();
    expect(harness.engine.isAmbient()).toBe(true);

    harness.ui.shareButton.click();
    await vi.waitFor(() => expect(harness.copied).toHaveLength(1));

    // What travels is the session, never the position an interlude happened
    // to leave the cube in.
    expect(harness.engine.isAmbient()).toBe(false);
  });
});

describe('the records of a sitting', () => {
  it('keeps a solve you finished and says when it is the best', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.setSolved(true);
    harness.commitMove();
    harness.setNow(20_100);
    harness.controller.afterEngineFrame();

    expect(harness.ui.status.textContent).toBe('Solved in 00:20.00. A new best.');
    expect(harness.ui.recordBest.textContent).toBe('Best 3×3 00:20.00');
    expect(harness.ui.recordList.children).toHaveLength(1);

    // A slower second solve joins the list without taking the best.
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.setNow(30_000);
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.setSolved(true);
    harness.commitMove();
    harness.setNow(60_000);
    harness.controller.afterEngineFrame();

    expect(harness.ui.status.textContent).toBe('Solved in 00:30.00.');
    expect(harness.ui.recordBest.textContent).toBe('Best 3×3 00:20.00');
    expect(harness.ui.recordList.children).toHaveLength(2);
  });

  it('keeps nothing for a cube a rewind took down', () => {
    const harness = createHarness();

    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.setNow(100);
    harness.commitMove();
    harness.controller.afterEngineFrame();

    harness.ui.rewindButton.click();
    harness.setNow(5000);
    harness.finishRewind();

    expect(harness.controller.state).toBe('completed');
    expect(harness.ui.status.textContent).toContain('Not a solve of your own');
    expect(harness.ui.recordBest.textContent).toBe('No solves yet.');
    expect(harness.ui.recordList.children).toHaveLength(0);
  });

  it('keeps nothing for a cube that was opened rather than scrambled', () => {
    const harness = createHarness();

    // What a shared link leaves behind: a record with moves on it and a
    // session that was never started, because starting one is a scramble.
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.controller.state).toBe('idle');

    harness.setSolved(true);
    harness.commitMove();
    harness.controller.afterEngineFrame();

    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.recordBest.textContent).toBe('No solves yet.');
    expect(harness.ui.recordList.children).toHaveLength(0);
  });
});

describe('a controller attached to a cube that was already restored', () => {
  /** A scramble of two with one move of the user's own on top of it. */
  const OPENED = { scrambleEnd: 2, moves: [0x44, 0x45, 0x40] };

  it('does not hear the restore as a move landing', () => {
    const harness = createHarness({ opened: OPENED });

    // The baseline is taken when the session is built, and the restore was
    // already done by then -- so the first frame sees no change at all.
    harness.controller.afterEngineFrame();
    expect(harness.sound.play).not.toHaveBeenCalled();
  });

  it('does not complete, start a clock, or keep a record for it', () => {
    const harness = createHarness({ opened: OPENED });
    harness.setSolved(true);

    harness.controller.afterEngineFrame();

    // A shared link opens an idle session: the clock is armed by a scramble
    // this application played, and there was none.
    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');
    expect(harness.ui.recordBest.textContent).toBe('No solves yet.');
  });

  it('writes out the moves it was given, and lets them be taken back', () => {
    const harness = createHarness({ opened: OPENED });

    // The log is a reading of the record, so a session that arrived from
    // somewhere else draws exactly like one that was played here.
    expect(harness.ui.moveLogList.children).toHaveLength(1);
    expect(harness.ui.moveLogList.children[0]?.textContent).toBe("R'");
    expect(harness.ui.undoButton.disabled).toBe(false);
    expect(harness.ui.redoButton.disabled).toBe(true);
    expect(harness.ui.shareButton.disabled).toBe(false);
  });
});

describe('the controls a controller owns', () => {
  it('are every button and field of the game UI, listed once', () => {
    // Flattened out of the typed shell rather than out of the markup, so the
    // next control added to GameUi cannot be missed from the list that turns
    // it off before an engine arrives and after one has gone.
    const ui = createUi();
    const fields = Object.values(ui).flatMap((value: unknown) =>
      Array.isArray(value) ? value : [value],
    );
    const controls = fields.filter(
      (value): value is HTMLButtonElement | HTMLInputElement =>
        value instanceof HTMLButtonElement || value instanceof HTMLInputElement,
    );
    expect(new Set(controllerControls(ui))).toEqual(new Set(controls));
    expect(controllerControls(ui)).toHaveLength(controls.length);
  });

  it('are all off again once the controller has gone', () => {
    const harness = createHarness();
    harness.controller.teardown();
    for (const control of controllerControls(harness.ui)) {
      expect(control.disabled, control.id || control.textContent).toBe(true);
    }
  });
});

describe('colouring a real cube onto the net', () => {
  it('opens a draft, shows the picker, and puts it away again', () => {
    const harness = createHarness();

    expect(harness.ui.paintBar.hidden).toBe(true);
    expect(harness.ui.paintButton.getAttribute('aria-pressed')).toBe('false');

    harness.ui.paintButton.click();

    expect(harness.engine.beginPainting).toHaveBeenCalled();
    expect(harness.ui.paintBar.hidden).toBe(false);
    expect(harness.ui.paintButton.getAttribute('aria-pressed')).toBe('true');

    // Pressed again means put it away, and the cube was never touched.
    harness.ui.paintButton.click();

    expect(harness.engine.cancelPainting).toHaveBeenCalled();
    expect(harness.ui.paintBar.hidden).toBe(true);
  });

  it('puts the bar away when a new cube closes the draft', () => {
    // A scramble, a reset and a new size each put a different cube there,
    // and the engine closes the draft on its own as they do. The bar and the
    // toggle follow it, so one press on Paint afterwards opens a fresh draft
    // rather than closing one that is already gone.
    const cases: [string, (harness: ReturnType<typeof createHarness>) => void][] = [
      ['a scramble', (harness): void => {
        harness.ui.scrambleButton.click();
        harness.finishScramble();
      }],
      ['a reset', (harness): void => harness.ui.resetButton.click()],
      ['a new size', (harness): void => {
        harness.ui.cubeSizeInput.value = '4';
        harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));
      }],
    ];

    for (const [name, closeIt] of cases) {
      const harness = createHarness();
      harness.ui.paintButton.click();
      expect(harness.ui.paintBar.hidden, name).toBe(false);

      closeIt(harness);
      expect(harness.engine.isPainting(), name).toBe(false);
      expect(harness.ui.paintBar.hidden, name).toBe(true);
      expect(harness.ui.paintButton.getAttribute('aria-pressed'), name).toBe(
        'false',
      );

      harness.ui.paintButton.click();
      expect(harness.engine.isPainting(), name).toBe(true);
      expect(harness.ui.paintBar.hidden, name).toBe(false);
    }
  });

  it('holds the cube still while a draft is open', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.ui.undoButton.disabled).toBe(false);

    harness.ui.paintButton.click();

    // Everything that would turn the cube behind the draft is out of reach,
    // since the engine would refuse it; Scramble stays, since it closes the
    // draft instead.
    for (const control of [
      ...harness.ui.moveButtons,
      harness.ui.undoButton,
      harness.ui.redoButton,
      harness.ui.rewindButton,
      harness.ui.solveButton,
      harness.ui.ambientButton,
    ]) {
      expect(control.disabled, control.id || control.textContent).toBe(true);
    }
    expect(harness.ui.scrambleButton.disabled).toBe(false);
    expect(harness.ui.paintButton.disabled).toBe(false);

    harness.dispatchKey({ key: 'r' });
    expect(harness.engine.committedMoveCount()).toBe(1);

    harness.ui.paintCancelButton.click();
    expect(harness.ui.undoButton.disabled).toBe(false);
    expect(harness.ui.moveButtons[0]!.disabled).toBe(false);
  });

  it('offers Paint only on a cube at rest', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();

    // The engine will not take a draft of a cube mid-scramble, so the press
    // is not offered rather than answered with nothing.
    expect(harness.ui.paintButton.disabled).toBe(true);
    harness.finishScramble();
    expect(harness.ui.paintButton.disabled).toBe(false);

    // Watching gives way to a draft, so Paint stays live through it.
    const watched = createHarness();
    watched.ui.ambientButton.click();
    expect(watched.engine.isAmbient()).toBe(true);
    expect(watched.ui.paintButton.disabled).toBe(false);
  });

  it('stops watching before it takes a copy of the cube', () => {
    const harness = createHarness();
    harness.ui.ambientButton.click();
    expect(harness.engine.isAmbient()).toBe(true);

    harness.ui.paintButton.click();

    // A draft taken mid-pattern would be a copy of a moment nobody chose.
    expect(harness.engine.isAmbient()).toBe(false);
    expect(harness.engine.isPainting()).toBe(true);
  });

  it('presses one colour at a time and counts what each still needs', () => {
    const harness = createHarness();
    harness.ui.paintButton.click();

    const white = harness.ui.paintSwatches.find(
      (button) => button.dataset.sticker === '2',
    )!;
    const red = harness.ui.paintSwatches.find(
      (button) => button.dataset.sticker === '0',
    )!;

    expect(white.getAttribute('aria-pressed')).toBe('true');
    red.click();

    expect(harness.engine.setBrush).toHaveBeenCalledWith(0);
    expect(red.getAttribute('aria-pressed')).toBe('true');
    expect(white.getAttribute('aria-pressed')).toBe('false');

    // Nine of nine on a three by three, written where the colour is picked so
    // that a face copied down short is caught before it is submitted.
    const tally = red.querySelector<HTMLElement>('[data-sticker-tally]')!;
    expect(tally.textContent).toBe('9/9');
    expect(tally.dataset.short).toBe('false');
  });

  it('takes a colouring that is a cube and leaves the draft behind', () => {
    const harness = createHarness();
    harness.ui.paintButton.click();
    harness.ui.paintApplyButton.click();

    expect(harness.engine.applyPainting).toHaveBeenCalled();
    expect(harness.ui.paintBar.hidden).toBe(true);
    expect(harness.ui.status.textContent).toContain('your cube now');
  });

  it('keeps the draft open when the colouring is not a cube, and says why', () => {
    const harness = createHarness();
    harness.ui.paintButton.click();

    // Any change at all is refused by the fake, which is how a refusal is
    // reached without this test having to know what makes a cube.
    harness.ui.paintSwatches[1]!.click();
    harness.ui.paintApplyButton.click();

    expect(harness.ui.paintBar.hidden).toBe(false);
    expect(harness.ui.paintNote.textContent).toContain('too many squares');
  });

  it('presses Fill face on and off', () => {
    const harness = createHarness();
    harness.ui.paintButton.click();

    expect(harness.ui.paintFillButton.getAttribute('aria-pressed')).toBe(
      'false',
    );
    harness.ui.paintFillButton.click();
    expect(harness.ui.paintFillButton.getAttribute('aria-pressed')).toBe('true');
    expect(harness.engine.isFilling()).toBe(true);

    harness.ui.paintFillButton.click();
    expect(harness.ui.paintFillButton.getAttribute('aria-pressed')).toBe(
      'false',
    );
  });
})

describe('walking the cube along its record', () => {
  /** A scramble played out, then `count` moves of the user's own. */
  function withMoves(count: number) {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.finishScramble();
    for (let made = 0; made < count; made += 1) {
      harness.dispatchKey({ key: 'r' });
      harness.commitMove();
      harness.controller.afterEngineFrame();
    }
    return harness;
  }

  /** The chip for one of the user's own moves, counting from one. */
  function chip(harness: ReturnType<typeof createHarness>, ordinal: number) {
    return harness.ui.moveLogList.querySelectorAll<HTMLButtonElement>(
      'button[data-index]',
    )[ordinal - 1]!;
  }

  it('walks back to a move pressed on the list, one undo at a time', () => {
    const harness = withMoves(3);
    expect(harness.engine.timelineCursor()).toBe(6);

    // The first of the three: two to take back.
    chip(harness, 1).click();
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
    expect(harness.ui.stopButton.hidden).toBe(false);

    harness.finishRewind();
    expect(harness.engine.undo).toHaveBeenCalledTimes(2);
    // Stop stays up between the steps, not only during them.
    expect(harness.ui.stopButton.hidden).toBe(false);

    harness.finishRewind();
    expect(harness.engine.timelineCursor()).toBe(4);
    expect(harness.engine.undo).toHaveBeenCalledTimes(2);
    expect(harness.ui.stopButton.hidden).toBe(true);
    expect(harness.ui.scrambleButton.hidden).toBe(false);
  });

  it('walks forward to a move that was taken back', () => {
    const harness = withMoves(3);
    chip(harness, 1).click();
    harness.finishRewind();
    harness.finishRewind();

    chip(harness, 3).click();
    expect(harness.engine.redo).toHaveBeenCalledTimes(1);
    harness.finishRewind();
    harness.finishRewind();
    expect(harness.engine.timelineCursor()).toBe(6);
    expect(harness.engine.redo).toHaveBeenCalledTimes(2);
  });

  it('draws the timeline from the same record', () => {
    const harness = withMoves(2);
    const { timeline } = harness.ui;

    expect(timeline.scrambleLabel.textContent).toBe('Scramble · 3');
    expect(timeline.progressLabel.textContent).toBe('2 / 2');
    expect(timeline.panelProgress.textContent).toBe('2 / 2');
    expect(timeline.strip.querySelectorAll('li')).toHaveLength(2);
    expect(timeline.track.style.getPropertyValue('--scramble')).toBe('60%');
    expect(timeline.track.style.getPropertyValue('--cursor')).toBe('100%');

    // The fake scrambles with R, U and F.
    expect(timeline.scrambleText.textContent).toBe('R U F');

    // A chip on the strip goes to the same place as one in the panel.
    timeline.strip
      .querySelector<HTMLButtonElement>('button[data-index="3"]')!
      .click();
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
  });

  it('breaks off a rewind that is playing to follow a pressed move', () => {
    const harness = withMoves(3);

    harness.ui.rewindButton.click();
    expect(harness.engine.solveRewind).toHaveBeenCalled();

    // Stopping keeps the turn in flight, which leaves the cube one move down
    // from six; the first of the user's moves is one further.
    chip(harness, 1).click();
    expect(harness.engine.stopPlayback).toHaveBeenCalled();
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
    harness.finishRewind();
    expect(harness.engine.timelineCursor()).toBe(4);
  });

  it('drops the rest of a walk on Stop, and on any other command', () => {
    const harness = withMoves(3);

    chip(harness, 1).click();
    harness.ui.stopButton.click();
    harness.finishRewind();
    expect(harness.engine.undo).toHaveBeenCalledTimes(1);
    expect(harness.ui.stopButton.hidden).toBe(true);

    chip(harness, 1).click();
    // A turn of the user's own is somewhere else the cube is going.
    harness.finishRewind();
    harness.engine.undo.mockClear();
    harness.dispatchKey({ key: 'u' });
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.engine.undo).not.toHaveBeenCalled();
  });

  it('plays a line of typed moves one turn at a time', () => {
    const harness = withMoves(0);
    harness.engine.turnFace.mockClear();

    const detail: PlayMovesDetail = { text: "R U' 2F2" };
    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail }),
    );
    expect(harness.ui.status.textContent).toBe('Playing 3 moves.');
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
      1,
      1,
      1,
    );
    expect(harness.ui.stopButton.hidden).toBe(false);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Up,
      1,
      1,
      -1,
    );

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Front,
      2,
      2,
      2,
    );

    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(3);
    expect(harness.ui.stopButton.hidden).toBe(true);
    // The first of them started the clock, as a turn by hand would.
    expect(harness.controller.state).toBe('running');
  });

  it('ends a typed line when a layer is turned by hand in the middle of it', () => {
    const harness = withMoves(0);
    harness.engine.turnFace.mockClear();

    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail: { text: 'R U F' } }),
    );
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);

    // A press while R is turning commits it on the spot and drags a layer of
    // its own, which commits too: two moves on the record where the walk
    // asked for one. Nothing refuses anything, so only the record tells.
    harness.commitMove();
    harness.commitMove();
    harness.controller.afterEngineFrame();

    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);
    expect(harness.ui.stopButton.hidden).toBe(true);
    expect(harness.ui.status.textContent).toBe(
      'Stopped playing: a layer was turned by hand.',
    );
  });

  it('cancels a typed line waiting on a scramble, and says the scramble goes on', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();
    harness.controller.afterEngineFrame();

    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail: { text: 'R U' } }),
    );
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
    expect(harness.ui.stopButton.hidden).toBe(false);

    harness.ui.stopButton.click();
    expect(harness.ui.status.textContent).toBe(
      'Cancelled. The scramble still plays out.',
    );

    harness.finishScramble();
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
  });

  it('asks for no further step once the controller is gone', () => {
    const harness = withMoves(0);
    harness.engine.turnFace.mockClear();

    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail: { text: 'R U F' } }),
    );
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);

    // Torn down with the first step still landing: the frame that lands it
    // is the last one anybody hears of, and the rest are never asked for.
    harness.controller.teardown();
    harness.commitMove();
    harness.controller.afterEngineFrame();
    expect(harness.engine.turnFace).toHaveBeenCalledTimes(1);
  });

  it('says why a typed line was refused, and plays none of it', () => {
    const harness = withMoves(0);
    harness.engine.turnFace.mockClear();

    harness.ui.root.dispatchEvent(
      new CustomEvent(PLAY_MOVES_EVENT, { detail: { text: 'R Q' } }),
    );
    expect(harness.ui.status.textContent).toBe(`Can't read "Q" as a move.`);
    expect(harness.engine.turnFace).not.toHaveBeenCalled();
  });

  it('writes the size and the palette where the page shows them', () => {
    const harness = createHarness();
    expect(harness.ui.cubeSizeLabel.textContent).toBe('3×3×3');
    expect(harness.ui.root.dataset.stickerPalette).toBe('classic');

    harness.ui.paletteButtons
      .find((button) => button.dataset.palette === 'high-contrast')!
      .click();
    expect(harness.ui.root.dataset.stickerPalette).toBe('high-contrast');

    harness.ui.cubeSizeInput.value = '5';
    harness.ui.cubeSizeInput.dispatchEvent(new Event('change'));
    expect(harness.ui.cubeSizeLabel.textContent).toBe('5×5×5');
  });
});
