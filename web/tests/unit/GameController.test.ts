import { describe, expect, it, vi } from 'vitest';

import {
  attachGameController,
  type GameEngine,
  type GameUi,
  type KeyboardTarget,
} from '../../src/game/GameController.ts';
import type { TimerEnvironment } from '../../src/game/SolveTimer.ts';
import {
  CubeFace,
  CubeFlatStyle,
  CubePalette,
  CubeViewMode,
} from '../../src/wasm/CubeEngine.ts';

/** Builds semantic controls matching bootstrap's production markup. */
function createUi(): GameUi {
  const root = document.createElement('main');
  root.innerHTML = `
    <canvas id="view"></canvas>
    <output id="timer"></output>
    <p id="status"></p>
    <input id="scramble-moves" type="number" min="1" max="100" value="20">
    <button id="scramble" type="button">Scramble</button>
    <button id="reset" type="button">Reset</button>
    <button id="undo" type="button">Undo</button>
    <button id="redo" type="button">Redo</button>
    <button id="solve" type="button">Solve</button>
    <button id="stop" type="button" hidden>Stop</button>
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
    <ol id="move-log"></ol>
  `;
  document.body.replaceChildren(root);

  return {
    root,
    canvas: root.querySelector<HTMLCanvasElement>('#view')!,
    timer: root.querySelector<HTMLOutputElement>('#timer')!,
    status: root.querySelector<HTMLParagraphElement>('#status')!,
    scrambleButton: root.querySelector<HTMLButtonElement>('#scramble')!,
    scrambleMovesInput: root.querySelector<HTMLInputElement>('#scramble-moves')!,
    resetButton: root.querySelector<HTMLButtonElement>('#reset')!,
    undoButton: root.querySelector<HTMLButtonElement>('#undo')!,
    redoButton: root.querySelector<HTMLButtonElement>('#redo')!,
    solveButton: root.querySelector<HTMLButtonElement>('#solve')!,
    stopButton: root.querySelector<HTMLButtonElement>('#stop')!,
    moveLogList: root.querySelector<HTMLOListElement>('#move-log')!,
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
  };
}

/** How many moves the fake engine's scramble is. */
const SCRAMBLE_MOVES = 3;

/** Creates a fake engine and manually controlled timer/keyboard environment. */
function createHarness(overrides: { prefersReducedMotion?: boolean } = {}) {
  const ui = createUi();
  let solved = true;
  let busy = false;
  let viewMode = CubeViewMode.Both;
  let flatStyle = CubeFlatStyle.Net;
  let palette = CubePalette.Classic;
  // Clamped the way the engine clamps, so the control is tested against the
  // answer it will actually be given rather than the one it asked for.
  let speedScale = 1;
  let watching = false;

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
    }),
    resetCube: vi.fn((): void => {
      solved = true;
      length = 0;
      cursor = 0;
      scrambleEnd = 0;
      moves = [];
      busy = false;
      watching = false;
    }),
    undo: vi.fn((): boolean => {
      if (busy || cursor <= scrambleEnd) return false;
      return startRewind(cursor - 1);
    }),
    redo: vi.fn((): boolean => {
      if (busy || cursor >= length) return false;
      return startRewind(cursor + 1);
    }),
    solveRewind: vi.fn((): boolean => {
      if (busy || cursor === 0) return false;
      return startRewind(0);
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
      if (watching) return false;
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
    isSolved: vi.fn((): boolean => solved),
    committedMoveCount: vi.fn((): number =>
      cursor > scrambleEnd ? cursor - scrambleEnd : 0,
    ),
    turnFace: vi.fn((): boolean => {
      if (busy) return false;
      busy = true;
      return true;
    }),
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

  const controller = attachGameController({
    engine,
    ui,
    startFrameLoop,
    onError,
    randomSource: () => 1234,
    timerEnvironment,
    keyboardTarget,
    sound,
    prefersReducedMotion: overrides.prefersReducedMotion ?? false,
  });

  return {
    ui,
    engine,
    sound,
    controller,
    startFrameLoop,
    onError,
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
      if (cursor === 0 && scrambleEnd > 0) solved = true;
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
    expect(harness.ui.status.textContent).toBe('Solved in 00:02.34.');
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

    expect(harness.engine.turnFace).toHaveBeenCalledWith(CubeFace.Right, 1);
    expect(harness.startFrameLoop).toHaveBeenCalledTimes(1);
    expect(harness.ui.moveButtons[0]!.disabled).toBe(true);

    harness.commitMove();
    harness.controller.afterEngineFrame();
    const event = harness.dispatchKey({ key: 'R', shiftKey: true });
    expect(event.defaultPrevented).toBe(true);
    expect(harness.engine.turnFace).toHaveBeenLastCalledWith(
      CubeFace.Right,
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
    expect(harness.ui.canvas.dataset.flatStyle).toBe('rings');

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
    const { undoButton, redoButton, solveButton } = harness.ui;

    // Nothing has happened, so there is nothing to walk back along.
    expect(undoButton.disabled).toBe(true);
    expect(redoButton.disabled).toBe(true);
    expect(solveButton.disabled).toBe(true);

    harness.ui.scrambleButton.click();
    harness.finishScramble();

    // A scramble can be rewound but not undone: what is below the end of the
    // scramble is not the user's to take back.
    expect(undoButton.disabled).toBe(true);
    expect(solveButton.disabled).toBe(false);
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
    expect(solveButton.disabled).toBe(true);

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
    harness.ui.solveButton.click();
    expect(harness.ui.status.textContent).toContain('Press Stop');
    expect(stop.hidden).toBe(false);

    stop.click();
    expect(harness.engine.stopPlayback).toHaveBeenCalledTimes(1);
    expect(stop.hidden).toBe(true);
    expect(harness.ui.status.textContent).toBe('Stopped.');

    // Stopped where it was, so both directions are open again from there.
    expect(harness.ui.solveButton.disabled).toBe(false);
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
    harness.ui.solveButton.click();
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
    expect(harness.ui.status.textContent).toBe('Solved in 00:02.34.');
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
      harness.ui.solveButton,
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
    harness.ui.solveButton.click();
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
