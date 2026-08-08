import { startApp, type AppState, type StartAppOptions } from './AppLifecycle.ts';
import type { GameUi } from './game/GameController.ts';

/** Lifecycle entry point, injectable so tests can observe the wiring. */
export type StartAppFn = (options: StartAppOptions) => Promise<unknown>;

/** Browser feature detection seam, injectable for DOM integration tests. */
export type SupportCheckFn = () => boolean;

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
 * Wires the page DOM to the app lifecycle: renders the stage markup,
 * reflects lifecycle states on the container, and routes both startup
 * rejections and post-ready failures to the error UI.
 *
 * The returned promise settles when startup finished either way; it never
 * rejects, because failures are presented through the error state.
 */
export function bootstrap(
  app: HTMLElement,
  start: StartAppFn = startApp,
  supportCheck: SupportCheckFn = supportsBrowser,
): Promise<void> {
  app.innerHTML = `
<main class="game-shell" data-game-state="idle">
  <section class="game-stage" aria-labelledby="game-title">
    <canvas id="view" aria-label="Interactive Rubik's Cube. Drag a sticker to turn a layer, or drag empty space to orbit the view."></canvas>

    <div class="hud">
      <header class="hud__header">
        <h1 id="game-title">ThorVG Rubik's Cube</h1>
        <output id="timer" aria-label="Elapsed time">00:00.00</output>
      </header>

      <div class="view-switch" role="group" aria-label="View mode">
        <button type="button" data-view="3d" aria-pressed="false">3D</button>
        <button type="button" data-view="both" aria-pressed="true">Both</button>
        <button type="button" data-view="net" aria-pressed="false">Net</button>
      </div>

      <div class="game-actions" aria-label="Game actions">
        <button type="button" id="scramble">Scramble</button>
        <button type="button" id="reset">Reset</button>
        <button type="button" id="home-view">Home view</button>
      </div>
    </div>
  </section>

  <details class="move-controls">
    <summary>Keyboard and move controls</summary>
    <p>Use R, L, U, D, F, or B. Hold Shift for a counter-clockwise turn.</p>
    <div class="move-grid" aria-label="Face turns">
      <button type="button" data-face="r" data-turn="1" aria-label="Turn right face clockwise">R</button>
      <button type="button" data-face="r" data-turn="-1" aria-label="Turn right face counter-clockwise">R′</button>
      <button type="button" data-face="l" data-turn="1" aria-label="Turn left face clockwise">L</button>
      <button type="button" data-face="l" data-turn="-1" aria-label="Turn left face counter-clockwise">L′</button>
      <button type="button" data-face="u" data-turn="1" aria-label="Turn upper face clockwise">U</button>
      <button type="button" data-face="u" data-turn="-1" aria-label="Turn upper face counter-clockwise">U′</button>
      <button type="button" data-face="d" data-turn="1" aria-label="Turn down face clockwise">D</button>
      <button type="button" data-face="d" data-turn="-1" aria-label="Turn down face counter-clockwise">D′</button>
      <button type="button" data-face="f" data-turn="1" aria-label="Turn front face clockwise">F</button>
      <button type="button" data-face="f" data-turn="-1" aria-label="Turn front face counter-clockwise">F′</button>
      <button type="button" data-face="b" data-turn="1" aria-label="Turn back face clockwise">B</button>
      <button type="button" data-face="b" data-turn="-1" aria-label="Turn back face counter-clockwise">B′</button>
    </div>
  </details>

  <p id="status" role="status" aria-live="polite">Loading engine…</p>
</main>
`;

  const root = app.querySelector<HTMLElement>('.game-shell')!;
  const statusElement = app.querySelector<HTMLParagraphElement>('#status')!;
  const canvas = app.querySelector<HTMLCanvasElement>('#view')!;
  const gameUi: GameUi = {
    root,
    canvas,
    timer: app.querySelector<HTMLOutputElement>('#timer')!,
    status: statusElement,
    scrambleButton: app.querySelector<HTMLButtonElement>('#scramble')!,
    resetButton: app.querySelector<HTMLButtonElement>('#reset')!,
    homeViewButton: app.querySelector<HTMLButtonElement>('#home-view')!,
    viewButtons: [
      ...app.querySelectorAll<HTMLButtonElement>('[data-view]'),
    ],
    moveButtons: [
      ...app.querySelectorAll<HTMLButtonElement>('[data-face]'),
    ],
  };
  for (const button of app.querySelectorAll<HTMLButtonElement>('button')) {
    button.disabled = true;
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

  return start({ canvas, gameUi, setState, onError: fail }).then(
    () => undefined,
    fail,
  );
}
