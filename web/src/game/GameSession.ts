import type { CommitSound } from './ClickSound.ts';
import {
  formatElapsed,
  SolveTimer,
  type TimerEnvironment,
} from './SolveTimer.ts';

/**
 * One local solve session inside a ready application.
 *
 * `scrambling` is the sequence being played into the cube. Nothing can be
 * turned by hand during it and the clock is not yet armed, so it is a state of
 * its own rather than an early `ready`.
 */
export type GameState =
  | 'idle'
  | 'scrambling'
  | 'ready'
  | 'running'
  | 'completed';

/**
 * What a session has to ask the engine outside of a frame, and nothing more.
 *
 * What a frame says is not here: the controller takes that reading once for
 * the controls as well and hands it in, and one reading shared between them
 * cannot disagree with itself halfway through a frame.
 */
export type SessionEngine = {
  isSolved(): boolean;
  committedMoveCount(): number;
  timelineCursor(): number;
};

/**
 * One reading of the engine, taken once a frame and shared by everything.
 *
 * A commit is `cursor` having changed: every move that lands moves it by
 * exactly one, so there is no counter to keep beside it and no question of the
 * two describing different frames. A watched pattern does not move it at all,
 * which is what makes an interlude leave nothing behind without anyone asking
 * whether one is running.
 */
export type EngineFrame = {
  readonly busy: boolean;
  readonly watching: boolean;
  readonly cursor: number;
  readonly length: number;
  readonly scrambleEnd: number;
  /** Moves of the user's own currently on the cube, as the engine derives it. */
  readonly userMoves: number;
};

/** The elements a session writes to. */
export type SessionUi = {
  /** Carries the state as `data-game-state`, which the styles read. */
  readonly root: HTMLElement;
  readonly timer: HTMLOutputElement;
  readonly status: HTMLParagraphElement;
};

export type GameSessionOptions = {
  readonly engine: SessionEngine;
  readonly ui: SessionUi;
  readonly timerEnvironment?: TimerEnvironment;
  /** Sounded once on every frame a move committed; absent is silent. */
  readonly sound?: CommitSound;
};

/**
 * The clock and the state a solve passes through, apart from the controls.
 *
 * Split out because this is the part that keeps growing: the move log, the
 * sound, the records and the history all watch a solve rather than a button,
 * and a controller that owned both would end up half DOM wiring and half state
 * machine. What is left in the controller is the wiring.
 *
 * It reads the engine and never commands it. Every state change here follows
 * either a command the controller has already given or a frame that has
 * already run, so there is no order between the two to get wrong.
 */
export class GameSession {
  private readonly engine: SessionEngine;
  private readonly ui: SessionUi;
  private readonly timer: SolveTimer;
  private readonly sound: CommitSound | null;

  private currentState: GameState = 'idle';
  // Replaced by the constructor's own baseline before a frame is ever read;
  // written here as well because the reading is taken in a method.
  private previousCursor = 0;
  private previousUserMoves = 0;

  constructor(options: GameSessionOptions) {
    this.engine = options.engine;
    this.ui = options.ui;
    this.sound = options.sound ?? null;

    // Assigning value on an <output> publishes the text too, so the DOM only
    // has to be written once per tick.
    this.timer = new SolveTimer((elapsedMs: number): void => {
      this.ui.timer.value = formatElapsed(elapsedMs);
    }, options.timerEnvironment);

    this.takeBaseline();
    this.timer.reset();
    this.setState('idle');
  }

  get state(): GameState {
    return this.currentState;
  }

  /** Puts a message in the status line, which is the only thing that does. */
  announce(message: string): void {
    this.ui.status.textContent = message;
  }

  /** A scramble has been accepted and is now being turned into the cube. */
  beginScramble(): void {
    this.takeBaseline();
    this.timer.reset();
    this.setState('scrambling');
    this.announce('Scrambling the cube…');
  }

  /** The cube has been restored, so there is nothing under way. */
  restart(): void {
    this.takeBaseline();
    this.timer.reset();
    this.setState('idle');
    this.announce('Cube reset.');
  }

  /**
   * Reads one frame of the engine, after it has run.
   *
   * The reading is handed in rather than asked for again, so the session and
   * the controls are always describing the same moment.
   *
   * Two things are watched, and they are watched for different reasons. A move
   * committing is the cursor changing, whichever direction it went, because
   * that is what finishing a solve has to be seen through -- undoing the last
   * wrong move can be what solves the cube. The clock, though, starts on the
   * user's own moves: a rewind commits moves too, and a solve pressed from a
   * ready cube would otherwise start the clock it is about to stop.
   *
   * The sound is that same observation heard rather than a second one. Most of
   * its behaviour falls out of that: a scramble and a rewind move the cursor
   * and so are played with sound, a reset takes a new baseline here and is
   * silent, and a watched pattern never touches the record at all -- so
   * nobody has to ask whether one is running.
   */
  observe(frame: EngineFrame): void {
    const committed = frame.cursor !== this.previousCursor;
    const played = frame.userMoves > this.previousUserMoves;
    this.previousCursor = frame.cursor;
    this.previousUserMoves = frame.userMoves;

    // Once for the frame, not once per move. Two commits inside one
    // observation window are a sixtieth of a second apart, which is one click
    // to an ear, so nothing here counts how many landed.
    if (committed) this.sound?.play();

    // Asked as a state rather than as a change, so a one-move scramble that
    // has already finished by the first observed frame is still seen to
    // finish. That the call was accepted is what says it began.
    if (this.currentState === 'scrambling' && !frame.busy) {
      this.timer.arm();
      this.setState('ready');
      this.announce('Scramble ready. The timer starts after your first move.');
    }

    if (played && this.currentState === 'ready') {
      this.timer.start();
      this.setState('running');
    }

    if (committed && this.isUnderWay() && this.engine.isSolved()) {
      const finalMs = this.timer.stop();
      this.setState('completed');

      // Nothing is kept to tell the two apart. A cube with none of its record
      // applied, on a scramble that exists, can only have been rewound there:
      // an undo stops at the end of the scramble, so a solve is the one thing
      // that can take the cursor below it, and a cube the user finished
      // themselves always has moves of their own still on it.
      this.announce(
        frame.cursor === 0 && frame.scrambleEnd > 0
          ? `Rewound to solved in ${formatElapsed(finalMs)}. Not a solve of your own.`
          : `Solved in ${formatElapsed(finalMs)}.`,
      );
    }
  }

  /** Stops the clock's own frame loop; the DOM is the controller's to undo. */
  teardown(): void {
    this.timer.teardown();
  }

  /**
   * A solve that can still be finished, whether or not the clock has started.
   *
   * `ready` is in here because Solve may be pressed on a cube nobody has
   * touched yet: the cube ends up solved, and a session left waiting for a
   * first move on a solved cube would arm a clock with nothing to time.
   */
  private isUnderWay(): boolean {
    return this.currentState === 'ready' || this.currentState === 'running';
  }

  /**
   * Takes the two readings a frame is measured against.
   *
   * The commands that replace the record whole -- a scramble, a reset -- call
   * this themselves, because they know the record they left behind is not the
   * one the last frame saw. Nothing else has to: every other change to it goes
   * through a commit, which is exactly what observe() is watching for.
   */
  private takeBaseline(): void {
    this.previousCursor = this.engine.timelineCursor();
    this.previousUserMoves = this.engine.committedMoveCount();
  }

  private setState(state: GameState): void {
    this.currentState = state;
    this.ui.root.dataset.gameState = state;
  }
}
