import { startApp, type AppState, type StartAppOptions } from './AppLifecycle.ts';

/** Lifecycle entry point, injectable so tests can observe the wiring. */
export type StartAppFn = (options: StartAppOptions) => Promise<unknown>;

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
): Promise<void> {
  app.innerHTML = `
<main class="stage">
  <p id="status" role="status">Loading engine…</p>
  <canvas id="view" aria-label="ThorVG rendered scene"></canvas>
</main>
`;

  const statusElement = app.querySelector<HTMLParagraphElement>('#status')!;
  const canvas = app.querySelector<HTMLCanvasElement>('#view')!;

  const setState = (state: AppState, message: string): void => {
    app.dataset.state = state;
    statusElement.textContent = message;
  };

  const fail = (error: unknown): void => {
    console.error(error);
    setState('error', 'Failed to start the ThorVG engine.');
  };

  return start({ canvas, setState, onError: fail }).then(() => undefined, fail);
}
