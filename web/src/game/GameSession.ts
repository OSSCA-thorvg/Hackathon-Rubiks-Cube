import type { CommitSound } from './ClickSound.ts';
import type { SolveRecord } from './SessionRecords.ts';
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
  /** Which cube was solved, which a record is meaningless without. */
  cubeSize(): number;
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
  /**
   * Told about a solve the person finished themselves.
   *
   * Called only where the announcement below says "Solved", so what counts as
   * a solve worth keeping is decided once, in the branch that already had to
   * tell the two apart. A cube a rewind took down, and one opened from a
   * shared link, never reach it -- the first fails that branch and the second
   * is never under way, because a session only arms its clock after a
   * scramble it started.
   *
   * @returns whether this is the fastest so far, which is said as part of the
   *          completion message rather than announced over the top of it.
   */
  readonly recordSolve?: (record: SolveRecord) => boolean;
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
  private readonly recordSolve: ((record: SolveRecord) => boolean) | null;

  private currentState: GameState = 'idle';
  /**
   * Whether the solver has been asked for the answer during this sitting.
   *
   * Kept here rather than derived from the record, because the record cannot
   * hold it: a solve writes ordinary moves and the cube it leaves looks
   * exactly like one somebody finished themselves. A rewind is told apart by
   * the cursor being at nothing, and this one has no such tell.
   *
   * It stays set even when the solve is broken off and the cube is finished
   * by hand. A rewind stopped part way leaves nothing behind -- those were the
   * person's own moves coming back off -- but half a solution is still a
   * solution somebody was shown.
   */
  private assisted = false;
  // Replaced by the constructor's own baseline before a frame is ever read;
  // written here as well because the reading is taken in a method.
  private previousCursor = 0;
  private previousUserMoves = 0;

  constructor(options: GameSessionOptions) {
    this.engine = options.engine;
    this.ui = options.ui;
    this.sound = options.sound ?? null;
    this.recordSolve = options.recordSolve ?? null;

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
    this.assisted = false;
    this.setState('scrambling');
    this.announce('Scrambling the cube…');
  }

  /** The cube has been restored, so there is nothing under way. */
  restart(): void {
    this.takeBaseline();
    this.timer.reset();
    this.assisted = false;
    this.setState('idle');
    this.announce('Cube reset.');
  }

  /**
   * The solver has been asked to finish this cube.
   *
   * The clock stops here and is not started again for this sitting: a time
   * that ran while the answer was on screen is not a time, and stopping it at
   * the moment the help was asked for is the honest place. What the cube does
   * next is watched exactly as before -- the solve still finishes, and the
   * finish is still announced, in its own words.
   */
  beginSolve(): void {
    this.assisted = true;
    this.timer.stop();
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

    // Not for a sitting the solver has been let into: its moves are the
    // user's own in the record, so they would otherwise start a clock that
    // has already been stopped for good.
    if (played && this.currentState === 'ready' && !this.assisted) {
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
      if (frame.cursor === 0 && frame.scrambleEnd > 0) {
        this.announce(
          `Rewound to solved in ${formatElapsed(finalMs)}. Not a solve of your own.`,
        );
        return;
      }

      // The one thing the record cannot say. No time is given, because the
      // clock stopped when the solver was asked and what it holds is the
      // length of the part before that -- a number about nothing.
      if (this.assisted) {
        this.announce('Solved by the solver. Not a solve of your own.');
        return;
      }

      // Kept, and told whether it is the best of the sitting. The answer joins
      // the message rather than following it, because a second announcement
      // would replace this one on the very line it was written to.
      const isBest = this.recordSolve?.({
        elapsedMs: finalMs,
        cubeSize: this.engine.cubeSize(),
        scrambleLength: frame.scrambleEnd,
        userMoveCount: frame.userMoves,
      });
      this.announce(
        `Solved in ${formatElapsed(finalMs)}.${isBest === true ? ' A new best.' : ''}`,
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
