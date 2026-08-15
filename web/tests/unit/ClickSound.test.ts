import { afterEach, describe, expect, it, vi } from 'vitest';

import { createClickSound } from '../../src/game/ClickSound.ts';

/** A gesture target that hands back the listeners it was given. */
function createGestures() {
  const listeners = new Map<string, () => void>();
  return {
    target: {
      addEventListener: (type: string, listener: () => void): void => {
        listeners.set(type, listener);
      },
      removeEventListener: (type: string, listener: () => void): void => {
        if (listeners.get(type) === listener) listeners.delete(type);
      },
    },
    types: (): string[] => [...listeners.keys()],
    fire: (type: string): void => {
      listeners.get(type)?.();
    },
  };
}

/** The smallest AudioContext a click can be built out of. */
function createFakeAudio() {
  const started: number[] = [];
  const disconnected: string[] = [];

  const param = () => ({
    setValueAtTime: vi.fn(),
    exponentialRampToValueAtTime: vi.fn(),
  });

  const context = {
    currentTime: 0,
    state: 'running',
    destination: { kind: 'destination' },
    resume: vi.fn(() => Promise.resolve()),
    close: vi.fn(() => Promise.resolve()),
    createGain: vi.fn(() => ({
      gain: { value: 0, ...param() },
      connect: vi.fn(),
      disconnect: vi.fn(() => disconnected.push('gain')),
    })),
    createOscillator: vi.fn(() => ({
      type: '',
      frequency: param(),
      connect: vi.fn(),
      disconnect: vi.fn(() => disconnected.push('oscillator')),
      start: vi.fn((at: number) => started.push(at)),
      stop: vi.fn(),
      onended: null as null | (() => void),
    })),
  };

  return { context, started, disconnected };
}

/** Installs a constructor on the window the module reads. */
function withAudio(factory: (() => unknown) | null): void {
  if (factory === null) {
    Reflect.deleteProperty(globalThis as object, 'AudioContext');
    Reflect.deleteProperty(globalThis as object, 'webkitAudioContext');
    return;
  }
  Object.defineProperty(globalThis, 'AudioContext', {
    value: factory,
    configurable: true,
    writable: true,
  });
}

afterEach(() => {
  withAudio(null);
});

describe('createClickSound', () => {
  it('waits for a gesture before making a context', () => {
    const audio = createFakeAudio();
    withAudio(function AudioContextStub() {
      return audio.context;
    });

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);

    // A commit can arrive before anyone has touched the page -- a shared link
    // that starts playing, say. It is silent rather than an error, and no
    // context was made behind the gesture policy's back.
    sound.play();
    expect(audio.started).toHaveLength(0);

    gestures.fire('pointerdown');
    sound.play();
    expect(audio.started).toHaveLength(1);
  });

  it('opens on whichever gesture comes first, and only once', () => {
    const audio = createFakeAudio();
    let contexts = 0;
    withAudio(function AudioContextStub() {
      contexts += 1;
      return audio.context;
    });

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);

    // The cube is turned with the keyboard as readily as with a pointer, so
    // every way in is listened for and the first of them is the one that
    // counts -- including mousedown, which is the press an environment that
    // does not synthesise pointer events still sends.
    expect(gestures.types()).toEqual([
      'pointerdown',
      'mousedown',
      'touchstart',
      'keydown',
    ]);

    gestures.fire('keydown');
    expect(contexts).toBe(1);
    expect(audio.context.resume).toHaveBeenCalled();

    // Both listeners come off together, so a second gesture cannot open a
    // second context over the first.
    expect(gestures.types()).toEqual([]);
    gestures.fire('pointerdown');
    expect(contexts).toBe(1);

    sound.teardown();
  });

  it('goes quiet rather than throwing when there is no WebAudio', () => {
    withAudio(null);

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);

    expect(() => gestures.fire('pointerdown')).not.toThrow();
    expect(() => sound.play()).not.toThrow();
    expect(() => sound.teardown()).not.toThrow();
  });

  it('goes quiet rather than throwing when a context cannot be made', () => {
    withAudio(function AudioContextStub() {
      throw new Error('no audio output');
    });

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);

    expect(() => gestures.fire('pointerdown')).not.toThrow();
    expect(() => sound.play()).not.toThrow();
  });

  it('makes no sound while muted, and makes one again after', () => {
    const audio = createFakeAudio();
    withAudio(function AudioContextStub() {
      return audio.context;
    });

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);
    gestures.fire('pointerdown');

    expect(sound.isMuted()).toBe(false);

    sound.setMuted(true);
    expect(sound.isMuted()).toBe(true);
    sound.play();
    expect(audio.started).toHaveLength(0);

    sound.setMuted(false);
    sound.play();
    expect(audio.started).toHaveLength(1);
  });

  it('drops each voice once it has finished sounding', () => {
    const audio = createFakeAudio();
    withAudio(function AudioContextStub() {
      return audio.context;
    });

    const gestures = createGestures();
    const sound = createClickSound(gestures.target);
    gestures.fire('pointerdown');

    sound.play();
    const oscillator = audio.context.createOscillator.mock.results.at(-1)!
      .value as { onended: (() => void) | null };

    // A rewind is dozens of clicks in a few seconds; each one has to let go of
    // its nodes when it stops, or the run leaves a graph behind it.
    expect(oscillator.onended).toBeTypeOf('function');
    oscillator.onended?.();
    expect(audio.disconnected).toEqual(['oscillator', 'gain']);
  });

  it('closes the context it opened, and stops listening either way', () => {
    const audio = createFakeAudio();
    withAudio(function AudioContextStub() {
      return audio.context;
    });

    const gestures = createGestures();
    const opened = createClickSound(gestures.target);
    gestures.fire('pointerdown');
    opened.teardown();
    expect(audio.context.close).toHaveBeenCalledTimes(1);

    // Torn down before anyone touched the page: nothing to close, and the
    // listeners still have to come off.
    const untouched = createGestures();
    createClickSound(untouched.target).teardown();
    expect(untouched.types()).toEqual([]);
  });
});
