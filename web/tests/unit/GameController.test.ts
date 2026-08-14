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
    <button id="scramble" type="button">Scramble</button>
    <button id="reset" type="button">Reset</button>
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
    resetButton: root.querySelector<HTMLButtonElement>('#reset')!,
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

  const engine = {
    scramble: vi.fn((): void => {
      solved = false;
      moveCount = 0;
      busy = false;
    }),
    resetCube: vi.fn((): void => {
      solved = true;
      moveCount = 0;
      busy = false;
    }),
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
    seedSource: () => 1234,
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
    expect(harness.engine.scramble).toHaveBeenCalledWith(1234);
    expect(harness.controller.state).toBe('ready');
    expect(harness.ui.status.textContent).toContain('timer starts');

    harness.ui.resetButton.click();
    expect(harness.engine.resetCube).toHaveBeenCalledTimes(1);
    expect(harness.controller.state).toBe('idle');
    expect(harness.ui.timer.value).toBe('00:00.00');
  });

  it('starts on first commit and stops on a later solved commit', () => {
    const harness = createHarness();
    harness.ui.scrambleButton.click();

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
