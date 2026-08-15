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
    <button id="ambient" type="button" aria-pressed="false">Watch</button>
    <button id="home-view" type="button">Home</button>
    <button data-view="3d" type="button">3D</button>
    <button data-view="both" type="button">Both</button>
    <button data-view="2d" type="button">2D</button>
    <button data-flat="net" type="button">Net</button>
    <button data-flat="rings" type="button">Rings</button>
    <button data-flat="both" type="button">Net + Rings</button>
    <button data-face="r" data-turn="1" type="button">R</button>
    <button data-face="r" data-turn="-1" type="button">R prime</button>
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
    ambientButton: root.querySelector<HTMLButtonElement>('#ambient')!,
    homeViewButton: root.querySelector<HTMLButtonElement>('#home-view')!,
    viewButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-view]')],
    flatButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-flat]')],
    moveButtons: [...root.querySelectorAll<HTMLButtonElement>('[data-face]')],
  };
}

/** Creates a fake engine and manually controlled timer/keyboard environment. */
function createHarness() {
  const ui = createUi();
  let solved = true;
  let moveCount = 0;
  let busy = false;
  let viewMode = CubeViewMode.Both;
  let flatStyle = CubeFlatStyle.Net;
  let watching = false;

  const engine = {
    scramble: vi.fn((): void => {
      // Accepted and busy, with the cube still solved: the moves are turned
      // into it over the frames that follow.
      moveCount = 0;
      busy = true;
      watching = false;
    }),
    resetCube: vi.fn((): void => {
      solved = true;
      moveCount = 0;
      busy = false;
      watching = false;
    }),
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
    committedMoveCount: vi.fn((): number => moveCount),
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
  const controller = attachGameController({
    engine,
    ui,
    startFrameLoop,
    onError,
    randomSource: () => 1234,
    timerEnvironment,
    keyboardTarget,
  });

  return {
    ui,
    engine,
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
      busy = false;
      controller.afterEngineFrame();
    },
    commitMove: (): void => {
      ++moveCount;
      busy = false;
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
