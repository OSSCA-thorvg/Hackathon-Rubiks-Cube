import { describe, expect, it, vi } from 'vitest';

import {
  formatElapsed,
  SolveTimer,
  type TimerEnvironment,
} from '../../src/game/SolveTimer.ts';

/** Creates a manually clocked environment and timer. */
function createTimer() {
  let now = 0;
  let pending: FrameRequestCallback | null = null;
  let handle = 0;
  const ticks: number[] = [];

  const environment: TimerEnvironment = {
    now: () => now,
    requestFrame: vi.fn((callback: FrameRequestCallback): number => {
      pending = callback;
      return ++handle;
    }),
    cancelFrame: vi.fn((): void => {
      pending = null;
    }),
  };
  const timer = new SolveTimer((elapsed) => ticks.push(elapsed), environment);

  return {
    timer,
    environment,
    ticks,
    setNow: (value: number): void => {
      now = value;
    },
    runFrame: (): void => {
      const callback = pending;
      pending = null;
      callback?.(now);
    },
    hasFrame: (): boolean => pending !== null,
  };
}

describe('formatElapsed', () => {
  it('formats centiseconds without capping minutes', () => {
    expect(formatElapsed(0)).toBe('00:00.00');
    expect(formatElapsed(61_239)).toBe('01:01.23');
    expect(formatElapsed(100 * 60_000 + 9_990)).toBe('100:09.99');
  });

  it('normalizes invalid and negative elapsed values', () => {
    expect(formatElapsed(-1)).toBe('00:00.00');
    expect(formatElapsed(Number.NaN)).toBe('00:00.00');
  });
});

describe('SolveTimer', () => {
  it('arms without scheduling and starts only once', () => {
    const harness = createTimer();
    harness.timer.arm();
    expect(harness.timer.state).toBe('armed');
    expect(harness.hasFrame()).toBe(false);

    harness.setNow(100);
    harness.timer.start();
    harness.setNow(250);
    harness.timer.start();
    expect(harness.timer.state).toBe('running');
    expect(harness.hasFrame()).toBe(true);

    // Measured from the first start, so the second call moved nothing.
    harness.runFrame();
    expect(harness.ticks.at(-1)).toBe(150);
  });

  it('ticks while running and freezes when stopped', () => {
    const harness = createTimer();
    harness.timer.arm();
    harness.setNow(10);
    harness.timer.start();
    harness.setNow(1260);
    harness.runFrame();

    expect(harness.ticks.at(-1)).toBe(1250);
    expect(harness.hasFrame()).toBe(true);
    expect(harness.timer.stop()).toBe(1250);
    expect(harness.timer.state).toBe('stopped');
    expect(harness.hasFrame()).toBe(false);
  });

  it('reset and teardown cancel scheduled work', () => {
    const harness = createTimer();
    harness.timer.arm();
    harness.timer.start();
    harness.timer.reset();

    expect(harness.timer.state).toBe('idle');
    expect(harness.ticks.at(-1)).toBe(0);
    expect(harness.hasFrame()).toBe(false);
    expect(harness.environment.cancelFrame).toHaveBeenCalledTimes(1);

    harness.timer.arm();
    harness.timer.start();
    harness.timer.teardown();
    expect(harness.hasFrame()).toBe(false);
  });
});
