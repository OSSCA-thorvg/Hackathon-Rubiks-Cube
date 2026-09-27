import {
  attachPointer,
  type GestureLock,
  type PointerController,
  type PointerTarget,
} from './input/PointerController.ts';
import {
  ALL_SCENES,
  computeDrawingBufferSize,
  CubeEngine,
  CubeSurface,
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
export type EngineLike = GameEngine & {
    /** Hands one of the surfaces beyond the first a canvas of its own. */
    presentSurface(id: CubeSurface, canvas: HTMLCanvasElement): void;
    /** Which scenes a surface shows, as CubeScene bits. */
    setSurfaceScenes(id: CubeSurface, scenes: number): void;
    /**
     * Null puts a surface away, for a canvas that is not on the page.
     *
     * @returns whether the size changed, and so a frame is owed.
     */
    resizeSurface(id: CubeSurface, size: CubeEngineSize | null): boolean;
    /** A press on one surface, in that surface's pixels. */
    pointerDownOn(id: CubeSurface, x: number, y: number): boolean;
    pointerMove(x: number, y: number): void;
    pointerUp(): void;
    pointerCancel(): void;
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

/**
 * One canvas beside the cube's, and the scenes the engine draws on it.
 *
 * The page decides where each canvas goes and how big it is; the engine is
 * told only the size, and fits its scenes to it.
 */
export type StageView = {
  readonly id: CubeSurface;
  readonly canvas: HTMLCanvasElement;
  /** CubeScene bits. */
  readonly scenes: number;
  /** Whether a press on it reaches the engine; the axes are only looked at. */
  readonly pressable: boolean;
};

export type StartAppOptions = {
  /** The cube's canvas, which the engine is created with. */
  readonly canvas: HTMLCanvasElement;
  /**
   * The canvases beside it. The cube's canvas keeps every scene none of them
   * shows, so without any it shows them all.
   */
  readonly views?: readonly StageView[];
  /** Gameplay controls the lifecycle hands to the game controller. */
  readonly gameUi: GameUi;
  /** The lighting sliders; absent on a page without them. */
  readonly lightingUi?: LightingUi;
  readonly setState: (state: AppState, message: string) => void;
  /** Called for failures after the app reached the ready state. */
  readonly onError: (error: unknown) => void;
  /** Where the canvas ground comes from; absent leaves the engine default. */
  readonly theme?: ThemeSource;
  /** Whether a dialog is up over the cube, which the game's keys stay out of. */
  readonly blocked?: () => boolean;
  /**
   * Test seams; production uses CubeEngine and the real globals. The engine
   * is created at the size its canvas is first given, which is this
   * module's to decide since the density is shared by every view.
   */
  readonly createEngine?: (
    canvas: HTMLCanvasElement,
    size: CubeEngineSize,
  ) => Promise<EngineLike>;
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

/** The engine as a press on one surface's canvas reaches it. */
function pressingSurface(engine: EngineLike, id: CubeSurface): PointerTarget {
  return {
    pointerDown: (x, y) => engine.pointerDownOn(id, x, y),
    pointerMove: (x, y) => engine.pointerMove(x, y),
    pointerUp: () => engine.pointerUp(),
    pointerCancel: () => engine.pointerCancel(),
  };
}

/**
 * The most pixels the views are drawn at, all of them together.
 *
 * A software renderer's frame costs what its pixels cost, and a large stage
 * on a dense screen asks for more of them than anyone can see: past about two
 * and a half million a frame, the views are drawn at a lower density and
 * scaled up to their boxes, which on a screen that dense is not a difference
 * an eye makes out.
 */
export const MAX_STAGE_PIXELS = 2_600_000;

/**
 * The pixel density every view is drawn at, the same for all of them.
 *
 * The device's own ratio, until the boxes together would ask for more pixels
 * than the budget; past it they come down together, so no view is drawn
 * sharper than the one beside it. A ratio that is not a positive number is
 * taken as one.
 */
export function stageDensity(
  boxes: readonly CubeEngineSize[],
  devicePixelRatio: number,
  pixelBudget: number = MAX_STAGE_PIXELS,
): number {
  const ratio =
    Number.isFinite(devicePixelRatio) && devicePixelRatio > 0
      ? devicePixelRatio
      : 1;
  const area = boxes.reduce((sum, box) => sum + box.width * box.height, 0);
  if (!(area > 0)) return ratio;
  return Math.min(ratio, Math.sqrt(pixelBudget / area));
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
    ((target: HTMLCanvasElement, size: CubeEngineSize) =>
      CubeEngine.create(target, { size }));
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

  const theme: ThemeSource | null = options.theme ?? null;
  const views = options.views ?? [];

  // Every canvas the engine draws on, the cube's first.
  const surfaces: readonly { id: CubeSurface; canvas: HTMLCanvasElement }[] = [
    { id: CubeSurface.Cube, canvas },
    ...views,
  ];

  /**
   * Each canvas's drawing buffer for its box as it is now.
   *
   * All at one density, so a view is never sharper than the one beside it. A
   * canvas with no box is not on the page -- its view is not the one being
   * looked at -- and is null, for its surface to be put away rather than
   * drawn at a pixel.
   */
  const measure = (): (CubeEngineSize | null)[] => {
    const density = stageDensity(
      surfaces.map(({ canvas: target }) => ({
        width: target.clientWidth,
        height: target.clientHeight,
      })),
      win.devicePixelRatio,
    );
    return surfaces.map(({ canvas: target }) =>
      target.clientWidth === 0 || target.clientHeight === 0
        ? null
        : computeDrawingBufferSize(
            target.clientWidth,
            target.clientHeight,
            density,
          ),
    );
  };

  // The engine starts at the size the cube's canvas is about to be given, so
  // it is not made at one size and then straight away remade at another. It
  // needs some size to start at, and a canvas with no box is put away just
  // after.
  const engine = await createEngine(
    canvas,
    measure()[0] ?? { width: 1, height: 1 },
  );

  /**
   * Brings every drawing buffer to its canvas's box.
   *
   * The engine is asked about every surface and says which it changed: it
   * holds the sizes, so this keeps no second copy of them to disagree with.
   *
   * @returns true when any of them changed, so a frame is owed.
   */
  const fitSurfaces = (): boolean => {
    const sizes = measure();
    let changed = false;
    surfaces.forEach(({ id }, index) => {
      if (engine.resizeSurface(id, sizes[index] ?? null)) changed = true;
    });
    return changed;
  };

  let opening: OpeningState = 'fresh';
  try {
    if (views.length > 0) {
      let elsewhere = 0;
      for (const view of views) {
        engine.presentSurface(view.id, view.canvas);
        engine.setSurfaceScenes(view.id, view.scenes);
        elsewhere |= view.scenes;
      }
      engine.setSurfaceScenes(CubeSurface.Cube, ALL_SCENES & ~elsewhere);
    }
    // Every view at its size before anything is drawn, so the first frame is
    // the whole stage rather than the cube's canvas alone.
    fitSurfaces();
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
  const pointers: PointerController[] = [];
  let game: GameController | null = null;
  let lighting: LightingControls | null = null;
  let frameHandle: number | null = null;
  let previousTimestamp: number | null = null;
  // Whether a frame is being drawn this moment, which is what tells a loop
  // asked for from inside one from a loop starting afresh.
  let drawing = false;
  // One gesture at a time across every canvas, as the engine follows it --
  // and, while it is held, no canvas is resized under it.
  const lock: GestureLock = { held: false };
  let sizeOwed = false;
  const onGestureEnd = (): void => {
    if (sizeOwed) applySize();
  };
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

    drawing = true;
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
    } finally {
      drawing = false;
    }
  };

  /**
   * Runs frames while the engine has work.
   *
   * Nothing animates at rest, so there is no loop then either: a still cube
   * costs no frames at all. A loop started from rest measures its first frame
   * from nothing, so the time spent at rest never arrives as one enormous
   * step; one asked for while a frame is being drawn -- a walk putting its
   * next step in -- is the same loop going on, and keeps its clock.
   */
  const startFrameLoop = (): void => {
    if (!active || frameHandle !== null) return;

    if (!drawing) previousTimestamp = null;
    frameHandle = requestFrame(drawFrame);
  };

  /**
   * Brings the canvases to their boxes -- or, while a gesture is in hand,
   * notes that they are owed it.
   *
   * Resizing a surface drops a drag begun on it, since the drag's screen
   * directions were read off the old size. So a box that changes under a
   * finger -- a window resized, a line of text growing, anything at all --
   * waits for the finger: the size is applied the moment the gesture ends,
   * and until then the canvas is only scaled to its new box by the browser.
   */
  const applySize = (): void => {
    if (!active) return;
    if (lock.held) {
      sizeOwed = true;
      return;
    }
    sizeOwed = false;

    try {
      if (fitSurfaces()) engine.render();
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
    for (const pointer of pointers) attempt(() => pointer.teardown());
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
    const cube = pressingSurface(engine, CubeSurface.Cube);
    pointers.push(
      attachPointer({
        canvas,
        engine: cube,
        onGestureStart: startFrameLoop,
        onGestureEnd,
        lock,
      }),
    );
    for (const view of views) {
      if (!view.pressable) continue;
      pointers.push(
        attachPointer({
          canvas: view.canvas,
          engine: pressingSurface(engine, view.id),
          // A press on the empty part of a flat view turns the viewpoint, as
          // empty space round the cube does: offered to the cube's canvas,
          // in its pixels, once this one's engine has turned it down.
          fallback: { canvas, engine: cube },
          onGestureStart: startFrameLoop,
          onGestureEnd,
          lock,
        }),
      );
    }
    game = createGameController({
      engine,
      ui: options.gameUi,
      startFrameLoop,
      blocked: options.blocked,
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
    for (const surface of surfaces) observer.observe(surface.canvas);
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
