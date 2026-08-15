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

/** What a session has to ask the engine, and nothing more. */
export type SessionEngine = {
  isSolved(): boolean;
  committedMoveCount(): number;
  isBusy(): boolean;
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

  private currentState: GameState = 'idle';
  private previousMoveCount: number;

  constructor(options: GameSessionOptions) {
    this.engine = options.engine;
    this.ui = options.ui;

    // Assigning value on an <output> publishes the text too, so the DOM only
    // has to be written once per tick.
    this.timer = new SolveTimer((elapsedMs: number): void => {
      this.ui.timer.value = formatElapsed(elapsedMs);
    }, options.timerEnvironment);

    this.previousMoveCount = this.engine.committedMoveCount();
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
    this.previousMoveCount = this.engine.committedMoveCount();
    this.timer.reset();
    this.setState('scrambling');
    this.announce('Scrambling the cube…');
  }

  /** The cube has been restored, so there is nothing under way. */
  restart(): void {
    this.previousMoveCount = this.engine.committedMoveCount();
    this.timer.reset();
    this.setState('idle');
    this.announce('Cube reset.');
  }

  /** Reads the engine once, after a frame of it has run. */
  observe(): void {
    const moveCount = this.engine.committedMoveCount();
    const committed = moveCount > this.previousMoveCount;
    this.previousMoveCount = moveCount;

    // Asked as a state rather than as a change, so a one-move scramble that
    // has already finished by the first observed frame is still seen to
    // finish. That the call was accepted is what says it began.
    if (this.currentState === 'scrambling' && !this.engine.isBusy()) {
      this.timer.arm();
      this.setState('ready');
      this.announce('Scramble ready. The timer starts after your first move.');
    }

    if (committed && this.currentState === 'ready') {
      this.timer.start();
      this.setState('running');
    }

    if (committed && this.currentState === 'running' && this.engine.isSolved()) {
      const finalMs = this.timer.stop();
      this.setState('completed');
      this.announce(`Solved in ${formatElapsed(finalMs)}.`);
    }
  }

  /** Stops the clock's own frame loop; the DOM is the controller's to undo. */
  teardown(): void {
    this.timer.teardown();
  }

  private setState(state: GameState): void {
    this.currentState = state;
    this.ui.root.dataset.gameState = state;
  }
}
