/** Monotonic clock and scheduling seams used by SolveTimer. */
export type TimerEnvironment = {
  readonly now: () => number;
  readonly requestFrame: (callback: FrameRequestCallback) => number;
  readonly cancelFrame: (handle: number) => void;
};

/** Lifecycle of one browser-owned solve timer. */
export type SolveTimerState = 'idle' | 'armed' | 'running' | 'stopped';

/** Formats milliseconds as unbounded minutes, seconds, and centiseconds. */
export function formatElapsed(elapsedMs: number): string {
  const safe = Number.isFinite(elapsedMs) ? Math.max(0, elapsedMs) : 0;
  const centiseconds = Math.floor(safe / 10);
  const minutes = Math.floor(centiseconds / 6000);
  const seconds = Math.floor((centiseconds % 6000) / 100);
  const fraction = centiseconds % 100;
  return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${String(fraction).padStart(2, '0')}`;
}

/** Browser-owned elapsed-time model for one solve session. */
export class SolveTimer {
  private readonly environment: TimerEnvironment;
  private readonly onTick: (elapsedMs: number) => void;
  private currentState: SolveTimerState = 'idle';
  private startedAt = 0;
  private elapsedMs = 0;
  private frameHandle: number | null = null;

  /** Creates a timer that reports display updates through `onTick`. */
  constructor(
    onTick: (elapsedMs: number) => void,
    environment: TimerEnvironment = {
      now: () => performance.now(),
      requestFrame: (callback) => requestAnimationFrame(callback),
      cancelFrame: (handle) => cancelAnimationFrame(handle),
    },
  ) {
    this.onTick = onTick;
    this.environment = environment;
  }

  /** Current timer lifecycle state. */
  get state(): SolveTimerState {
    return this.currentState;
  }

  /** Arms a zeroed timer without starting it. */
  arm(): void {
    this.cancelScheduledFrame();
    this.currentState = 'armed';
    this.elapsedMs = 0;
    this.onTick(0);
  }

  /**
   * Starts a timer that arm() prepared.
   *
   * Only an armed timer starts, so repeated calls once it is running keep
   * the timestamp of the move that actually began the solve.
   */
  start(): void {
    if (this.currentState !== 'armed') return;

    this.currentState = 'running';
    this.startedAt = this.environment.now();
    this.elapsedMs = 0;
    this.onTick(0);
    this.scheduleFrame();
  }

  /** Freezes and returns the current elapsed milliseconds. */
  stop(): number {
    if (this.currentState !== 'running') return this.elapsedMs;

    this.elapsedMs = Math.max(0, this.environment.now() - this.startedAt);
    this.currentState = 'stopped';
    this.cancelScheduledFrame();
    this.onTick(this.elapsedMs);
    return this.elapsedMs;
  }

  /** Returns to the idle 00:00.00 state. */
  reset(): void {
    this.cancelScheduledFrame();
    this.currentState = 'idle';
    this.startedAt = 0;
    this.elapsedMs = 0;
    this.onTick(0);
  }

  /** Cancels scheduling without publishing another display update. */
  teardown(): void {
    this.cancelScheduledFrame();
  }

  private scheduleFrame(): void {
    if (this.currentState !== 'running' || this.frameHandle !== null) return;

    this.frameHandle = this.environment.requestFrame((): void => {
      this.frameHandle = null;
      if (this.currentState !== 'running') return;
      this.elapsedMs = Math.max(
        0,
        this.environment.now() - this.startedAt,
      );
      this.onTick(this.elapsedMs);
      this.scheduleFrame();
    });
  }

  private cancelScheduledFrame(): void {
    if (this.frameHandle === null) return;
    this.environment.cancelFrame(this.frameHandle);
    this.frameHandle = null;
  }
}
