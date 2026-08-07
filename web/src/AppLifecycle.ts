import {
  computeDrawingBufferSize,
  CubeEngine,
  type CubeEngineSize,
} from './wasm/CubeEngine.ts';

/** UI states surfaced by the Phase 1 page. */
export type AppState = 'loading' | 'ready' | 'error';

/** Engine surface the lifecycle needs; CubeEngine satisfies it. */
export type EngineLike = {
  resize(size: CubeEngineSize): void;
  render(): void;
  dispose(): void;
};

/** Minimal ResizeObserver surface, injectable for unit tests. */
export type ObserverLike = {
  observe(target: Element): void;
  disconnect(): void;
};

/** Minimal window surface, injectable for unit tests. */
export type WindowLike = {
  readonly devicePixelRatio: number;
  addEventListener(type: string, listener: (event: Event) => void): void;
  removeEventListener(type: string, listener: (event: Event) => void): void;
};

export type StartAppOptions = {
  readonly canvas: HTMLCanvasElement;
  readonly setState: (state: AppState, message: string) => void;
  /** Called for failures after the app reached the ready state. */
  readonly onError: (error: unknown) => void;
  /** Test seams; production uses CubeEngine and the real globals. */
  readonly createEngine?: (canvas: HTMLCanvasElement) => Promise<EngineLike>;
  readonly createObserver?: (callback: () => void) => ObserverLike;
  readonly targetWindow?: WindowLike;
};

export type AppController = {
  /** Releases the observer, the listeners, and the engine together. */
  teardown(): void;
};

/**
 * Owns the page lifecycle around one engine instance: initial render,
 * resize handling, BFCache transitions, and a single teardown path.
 *
 * Rejects when startup fails (the engine is disposed first); failures
 * after the ready state tear everything down and report through onError.
 */
export async function startApp(
  options: StartAppOptions,
): Promise<AppController> {
  const { canvas, setState, onError } = options;
  const createEngine =
    options.createEngine ??
    ((target: HTMLCanvasElement) => CubeEngine.create(target));
  const createObserver =
    options.createObserver ??
    ((callback: () => void) => new ResizeObserver(callback));
  const win: WindowLike = options.targetWindow ?? window;

  setState('loading', 'Loading engine…');

  const engine = await createEngine(canvas);

  try {
    engine.render();
  } catch (error) {
    engine.dispose();
    throw error;
  }

  // Flipped first during teardown so repeated teardowns and callbacks
  // already queued by the observer or the event loop become no-ops.
  let active = true;
  let observer: ObserverLike | null = null;

  const applySize = (): void => {
    if (!active) return;

    const size = computeDrawingBufferSize(
      canvas.clientWidth,
      canvas.clientHeight,
      win.devicePixelRatio,
    );

    // Only an actual drawing buffer change reaches the engine.
    if (size.width === canvas.width && size.height === canvas.height) return;

    try {
      engine.resize(size);
      engine.render();
    } catch (error) {
      // Whether the native side survived the failure is not observable
      // from here, so tear everything down instead of retrying.
      teardown();
      onError(error);
    }
  };

  const onPageHide = (event: Event): void => {
    // A BFCache entry keeps the page alive for restoration; dispose only
    // on real unloads.
    if (!(event as PageTransitionEvent).persisted) teardown();
  };

  const onPageShow = (event: Event): void => {
    // A restored page may have missed viewport changes while cached.
    if ((event as PageTransitionEvent).persisted) applySize();
  };

  const teardown = (): void => {
    if (!active) return;
    active = false;

    // Best-effort cleanup: every release step runs even when an earlier
    // one throws, and teardown itself never throws so the operational
    // error that triggered it always reaches onError().
    let cleanupError: unknown = null;
    const attempt = (release: () => void): void => {
      try {
        release();
      } catch (error) {
        cleanupError ??= error;
      }
    };

    attempt(() => observer?.disconnect());
    attempt(() => win.removeEventListener('resize', applySize));
    attempt(() => win.removeEventListener('pagehide', onPageHide));
    attempt(() => win.removeEventListener('pageshow', onPageShow));
    attempt(() => engine.dispose());

    if (cleanupError !== null) {
      // Cleanup failures must not mask the operational error, so they
      // are only recorded once every resource was attempted.
      console.error(cleanupError);
    }
  };

  // The engine is live from here on, so the remaining setup runs as one
  // transaction: any failure unwinds whatever was already installed.
  try {
    observer = createObserver(applySize);
    observer.observe(canvas);
    // Device pixel ratio changes arrive with window resize events.
    win.addEventListener('resize', applySize);
    win.addEventListener('pagehide', onPageHide);
    win.addEventListener('pageshow', onPageShow);

    setState('ready', 'ThorVG software renderer');
  } catch (error) {
    teardown();
    throw error;
  }

  return { teardown };
}
