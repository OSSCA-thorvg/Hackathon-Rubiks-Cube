import {
  attachPointer,
  type PointerController,
  type PointerTarget,
} from './input/PointerController.ts';
import {
  computeDrawingBufferSize,
  CubeEngine,
  type CubeCanvasTheme,
  type CubeEngineSize,
} from './wasm/CubeEngine.ts';
import {
  canvasThemeOf,
  type EffectiveTheme,
  type ThemeListener,
} from './ui/ThemeController.ts';
import {
  attachLightingControls,
  type LightingControls,
  type LightingUi,
} from './ui/LightingControls.ts';
import {
  attachGameController,
  type GameController,
  type GameControllerOptions,
  type GameEngine,
  type GameUi,
} from './game/GameController.ts';
import { decodePainting, decodeSession } from './game/shareCode.ts';
import {
  clearShareFragment,
  readShareFragment,
  type HistoryLike,
  type LocationLike,
} from './game/shareLink.ts';

/** Top-level availability of the page. */
export type AppState = 'loading' | 'ready' | 'unsupported' | 'error';

/** Engine surface the lifecycle needs; CubeEngine satisfies it. */
export type EngineLike = PointerTarget &
  GameEngine & {
    resize(size: CubeEngineSize): void;
    render(): void;
    dispose(): void;
    /** @returns true while further frames still have to be drawn. */
    advance(elapsedMs: number): boolean;
    /** The ground the cube is drawn against, which the page decides. */
    setCanvasTheme(theme: CubeCanvasTheme): void;
    setLighting(values: readonly number[]): boolean;
    lighting(): number[];
    /** @returns false when the engine would not take the shared record. */
    restoreSession(
      size: number,
      scramble: readonly number[],
      user: readonly number[],
    ): boolean;
    /** @returns false when the colouring is not a cube any turning reaches. */
    restorePainting(
      size: number,
      painting: readonly number[],
      user: readonly number[],
    ): boolean;
  };

/**
 * What a fragment turned out to hold, which the ready message is made from.
 *
 * Carried out to the one place that writes a status line rather than being
 * announced from here. An announcement of its own would be overwritten by the
 * "Ready" that always follows it, and making the two depend on the order they
 * happen to run in is exactly the thing this avoids.
 */
type OpeningState = 'fresh' | 'shared' | 'unreadable';

/** What the status line says about how the page opened. */
const READY_MESSAGE: Readonly<Record<OpeningState, string>> = {
  fresh: 'Ready. Scramble the cube to begin.',
  shared: 'Ready. This cube came from a shared link.',
  unreadable: 'That shared link could not be read. Ready with a fresh cube.',
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

/**
 * Minimal document surface, injectable for unit tests.
 *
 * Separate from WindowLike because visibility is the document's, and reading
 * `hidden` off the same object that carries the listener is what keeps the two
 * from being able to disagree.
 */
export type DocumentLike = {
  readonly hidden: boolean;
  addEventListener(type: string, listener: (event: Event) => void): void;
  removeEventListener(type: string, listener: (event: Event) => void): void;
};

/**
 * The part of the theme controller the lifecycle needs.
 *
 * A reader and a subscription rather than the controller itself, because the
 * lifecycle has no business setting a theme -- it only has to paint the
 * canvas the color the page already is, and keep painting it that color.
 */
export type ThemeSource = {
  effective(): EffectiveTheme;
  subscribe(listener: ThemeListener): () => void;
};

export type StartAppOptions = {
  readonly canvas: HTMLCanvasElement;
  /** Gameplay controls the lifecycle hands to the game controller. */
  readonly gameUi: GameUi;
  /** The lighting sliders; absent on a page without them. */
  readonly lightingUi?: LightingUi;
  readonly setState: (state: AppState, message: string) => void;
  /** Called for failures after the app reached the ready state. */
  readonly onError: (error: unknown) => void;
  /** Where the canvas ground comes from; absent leaves the engine default. */
  readonly theme?: ThemeSource;
  /** Test seams; production uses CubeEngine and the real globals. */
  readonly createEngine?: (canvas: HTMLCanvasElement) => Promise<EngineLike>;
  readonly createObserver?: (callback: () => void) => ObserverLike;
  readonly targetWindow?: WindowLike;
  readonly targetDocument?: DocumentLike;
  readonly targetLocation?: LocationLike;
  readonly targetHistory?: HistoryLike;
  readonly requestFrame?: (callback: (timestamp: number) => void) => number;
  readonly cancelFrame?: (handle: number) => void;
  readonly createGameController?: (
    options: GameControllerOptions,
  ) => GameController;
};

export type AppController = {
  /** Releases the observer, the listeners, and the engine together. */
  teardown(): void;
};

/**
 * The `lighting` query parameter as a list of numbers, or null without one.
 *
 * `?lighting=a,b,c,...` in the engine's own order (see CubeEngine.setLighting).
 * Anything that is not a finite number makes the whole list null rather than
 * a partly applied setup.
 */
export function readLightingQuery(href: string): number[] | null {
  let raw: string | null;
  try {
    raw = new URL(href).searchParams.get('lighting');
  } catch {
    return null;
  }
  if (raw === null || raw.trim() === '') return null;

  const values = raw.split(',').map((item) => Number(item.trim()));
  return values.every((value) => Number.isFinite(value)) ? values : null;
}

/**
 * Opens whatever state the address carries, if it carries any.
 *
 * The fragment is taken off the address whether or not it could be read: one
 * left behind would replay the same refusal on every reload, and a reload here
 * is always a fresh start.
 *
 * There is no rollback and no second attempt. This runs before anything has
 * happened to the cube, so what a refusal falls back to is the cube the engine
 * has just made -- and reset_cube() is called for the one case where the
 * engine looked at a record and turned it down, because the answer for a
 * damaged link is a clean session and there is nothing else to try.
 */
function openSharedState(
  engine: EngineLike,
  location: LocationLike,
  history: HistoryLike,
): OpeningState {
  const encoded = readShareFragment(location.hash);
  if (encoded === null) return 'fresh';

  clearShareFragment(location, history);

  // The two layouts are asked in turn and whichever answers is the one the
  // link is in. Neither can be mistaken for the other -- each refuses a
  // version that is not its own before it reads a byte further -- so the order
  // is only a matter of which is tried first.
  const painted = decodePainting(encoded);
  if (painted !== null) {
    if (
      !engine.restorePainting(painted.size, painted.painting, painted.user)
    ) {
      engine.resetCube();
      return 'unreadable';
    }
    return 'shared';
  }

  const shared = decodeSession(encoded);
  if (shared === null) return 'unreadable';

  if (!engine.restoreSession(shared.size, shared.scramble, shared.user)) {
    engine.resetCube();
    return 'unreadable';
  }
  return 'shared';
}

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
  const doc: DocumentLike = options.targetDocument ?? document;
  const loc: LocationLike = options.targetLocation ?? window.location;
  const hist: HistoryLike = options.targetHistory ?? window.history;
  const requestFrame =
    options.requestFrame ??
    ((callback: (timestamp: number) => void) =>
      requestAnimationFrame(callback));
  const cancelFrame =
    options.cancelFrame ?? ((handle: number) => cancelAnimationFrame(handle));
  const createGameController =
    options.createGameController ?? attachGameController;

  setState('loading', 'Loading engine…');

  const engine = await createEngine(canvas);

  const theme: ThemeSource | null = options.theme ?? null;

  let opening: OpeningState = 'fresh';
  try {
    // Ahead of the first render as well, and for the same reason the restore
    // is: a frame drawn on the engine's default ground would show a dark
    // rectangle on a light page for exactly as long as it takes the first
    // theme change to arrive, which on a light machine is forever.
    if (theme !== null) engine.setCanvasTheme(canvasThemeOf(theme.effective()));
    // A lighting setup in the address, for tuning the lamps by eye without a
    // rebuild. Ahead of the first render like the theme, and never fatal: a
    // list the engine refuses simply leaves the default lights on.
    const lighting = readLightingQuery(loc.href);
    if (lighting !== null && !engine.setLighting(lighting)) {
      console.warn('Ignoring a lighting query the engine refused.', lighting);
    }
    // Ahead of the first render, which is the point of the order. Nothing is
    // animating after a restore, so no frame loop starts on its own -- a
    // restore behind this line would leave the logical cube shared and the
    // canvas still showing the solved one the engine was made with.
    opening = openSharedState(engine, loc, hist);
    engine.render();
  } catch (error) {
    engine.dispose();
    throw error;
  }

  // Flipped first during teardown so repeated teardowns and callbacks
  // already queued by the observer or the event loop become no-ops.
  let active = true;
  let observer: ObserverLike | null = null;
  let pointer: PointerController | null = null;
  let game: GameController | null = null;
  let lighting: LightingControls | null = null;
  let frameHandle: number | null = null;
  let previousTimestamp: number | null = null;
  // Whether a loop was taken away by the tab going out of sight, and so is
  // owed back when it returns. Nothing else may set it: a loop that ended
  // because the cube stopped moving is not owed anything.
  let pausedWhileHidden = false;
  // Handed back by the theme source, and the only way this stops listening.
  let unsubscribeTheme: (() => void) | null = null;

  const drawFrame = (timestamp: number): void => {
    frameHandle = null;
    if (!active) return;

    // The first frame of a run has no previous timestamp to measure from.
    const elapsed =
      previousTimestamp === null ? 0 : timestamp - previousTimestamp;
    previousTimestamp = timestamp;

    try {
      const moreFrames = engine.advance(elapsed);
      engine.render();
      game?.afterEngineFrame();

      // One frame is drawn after the engine stops asking for them, so the
      // final state of a turn always reaches the canvas.
      //
      // The controller may already have asked for the next frame above -- a
      // walk puts its next step in from afterEngineFrame -- and a second
      // request here would run two frames per tick, each advancing the clock.
      if (moreFrames) {
        if (frameHandle === null) frameHandle = requestFrame(drawFrame);
      } else if (frameHandle === null) {
        previousTimestamp = null;
      }
    } catch (error) {
      teardown();
      onError(error);
    }
  };

  /**
   * Runs frames while the engine has work.
   *
   * Nothing animates at rest, so there is no loop then either: a still cube
   * costs no frames at all.
   */
  const startFrameLoop = (): void => {
    if (!active || frameHandle !== null) return;

    previousTimestamp = null;
    frameHandle = requestFrame(drawFrame);
  };

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

  /**
   * Stops the frame loop while the tab is out of sight, and gives it back.
   *
   * Here rather than in the engine or the controls, because the loop is this
   * module's and nobody else's: the engine reads no clock, so a pause is
   * nothing more than advance() not being called, and a watched pattern picks
   * up mid-turn where it left off. The first frame back measures no elapsed
   * time, since startFrameLoop empties the previous timestamp -- the hours a
   * tab spent in the background never arrive as one enormous step.
   */
  const onVisibilityChange = (): void => {
    if (!active) return;

    if (doc.hidden) {
      if (frameHandle === null) return;

      cancelFrame(frameHandle);
      frameHandle = null;
      pausedWhileHidden = true;
      return;
    }

    if (!pausedWhileHidden) return;

    pausedWhileHidden = false;
    startFrameLoop();
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

    attempt(() => {
      if (frameHandle !== null) cancelFrame(frameHandle);
      frameHandle = null;
    });
    // Before the engine is disposed: cancelling a gesture calls into it,
    // and so does every control the game controller still has wired up.
    attempt(() => unsubscribeTheme?.());
    attempt(() => pointer?.teardown());
    attempt(() => game?.teardown());
    attempt(() => lighting?.teardown());
    attempt(() => observer?.disconnect());
    attempt(() => win.removeEventListener('resize', applySize));
    attempt(() => win.removeEventListener('pagehide', onPageHide));
    attempt(() => win.removeEventListener('pageshow', onPageShow));
    attempt(() => doc.removeEventListener('visibilitychange', onVisibilityChange));
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
    pointer = attachPointer({
      canvas,
      engine,
      onGestureStart: startFrameLoop,
    });
    game = createGameController({
      engine,
      ui: options.gameUi,
      startFrameLoop,
      // A failed gameplay command leaves the engine in an unknown state, so
      // it takes the same route out as a failed frame.
      onError: (error: unknown): void => {
        teardown();
        onError(error);
      },
    });
    // The lights are drawn by the engine and nothing else, so the sliders
    // talk to it directly and a failure there is the engine's failure.
    if (options.lightingUi !== undefined) {
      lighting = attachLightingControls({
        engine,
        ui: options.lightingUi,
        onError: (error: unknown): void => {
          teardown();
          onError(error);
        },
      });
    }
    // A theme change is not a cube change, so it asks for one frame rather
    // than starting a loop: nothing is moving, and the ground is repainted
    // by the same render every other still frame goes through.
    unsubscribeTheme =
      theme?.subscribe((next: EffectiveTheme): void => {
        if (!active) return;

        try {
          engine.setCanvasTheme(canvasThemeOf(next));
          engine.render();
        } catch (error) {
          teardown();
          onError(error);
        }
      }) ?? null;
    observer = createObserver(applySize);
    observer.observe(canvas);
    // Device pixel ratio changes arrive with window resize events.
    win.addEventListener('resize', applySize);
    win.addEventListener('pagehide', onPageHide);
    win.addEventListener('pageshow', onPageShow);
    doc.addEventListener('visibilitychange', onVisibilityChange);

    // The controller is attached after the restore, so the cursor it takes as
    // its baseline is the restored one: a restore is not a commit, and
    // nothing here sounds, completes, or starts a clock because of one.
    setState('ready', READY_MESSAGE[opening]);
  } catch (error) {
    teardown();
    throw error;
  }

  return { teardown };
}
