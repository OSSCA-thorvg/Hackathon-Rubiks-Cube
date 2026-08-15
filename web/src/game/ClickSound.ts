/**
 * What a session needs from the sound, and nothing more.
 *
 * One method, because a commit is the only thing that makes a noise. The
 * session neither knows nor can find out whether anything was audible, which
 * is what lets a browser without WebAudio, a page nobody has touched yet, and
 * a muted cube all be the same silent case at this end.
 */
export type CommitSound = {
  play(): void;
};

/** The sound plus the controls that belong to whoever owns the toggle. */
export type ClickSound = CommitSound & {
  setMuted(muted: boolean): void;
  isMuted(): boolean;
  /** Releases the audio context, if one was ever made. */
  teardown(): void;
};

/**
 * Where the first user gesture is listened for, injectable for tests.
 *
 * A gesture is what an audio context has to be created inside, so this is not
 * a detail of how the sound is made but the one thing its construction
 * depends on.
 */
export type GestureTarget = {
  addEventListener(type: string, listener: () => void): void;
  removeEventListener(type: string, listener: () => void): void;
};

/** Constructors browsers offer, including the prefix Safari kept. */
type AudioContextSource = {
  AudioContext?: typeof AudioContext;
  webkitAudioContext?: typeof AudioContext;
};

/**
 * The gestures an audio context may be created inside.
 *
 * All of them, because any can be the first thing a person does: the cube is
 * dragged with a pointer or a finger and turned with the keyboard, and
 * whichever comes first has to be the one that opens the sound. `mousedown`
 * is in here as well as `pointerdown` -- they are the same press on an
 * ordinary browser and the listener only ever runs once, but a surface that
 * synthesises only the older of the two would otherwise stay silent forever.
 */
const GESTURES = [
  'pointerdown',
  'mousedown',
  'touchstart',
  'keydown',
] as const;

/** How loud one click is at its peak, before the shared limit below. */
const VOICE_GAIN = 0.12;

/**
 * The ceiling every click passes through.
 *
 * A rewind at four times speed commits about every other frame, so clicks
 * overlap: the envelope of one is still falling when the next begins. Summed
 * at full strength they would clip. With the voices held at 0.12 and the sum
 * scaled by this, several at once stay well inside range and the run reads as
 * a rattle rather than as distortion.
 */
const MASTER_GAIN = 0.35;

/** Seconds a click lasts, envelope and all. */
const CLICK_SECONDS = 0.045;

/** Where the click starts and ends in pitch: a short downward tick. */
const START_HZ = 1750;
const END_HZ = 620;

/**
 * A short click, synthesised, that a commit can ask for.
 *
 * Nothing is fetched or bundled: the sound is two nodes and an envelope, so
 * the build and the deployment are exactly what they were.
 *
 * The audio context is made inside the first gesture rather than at the first
 * click. A commit is observed in a frame, which is outside the call stack of
 * whatever the person did, and an autoplay policy asks for the inside of it.
 * By the time the cube turns, the context is already open and waiting.
 *
 * Every failure here is silence rather than an error. A browser with no
 * WebAudio, a context that will not resume, a machine with no output: none of
 * them is a reason the cube should stop working, so nothing throws outwards.
 */
export function createClickSound(target?: GestureTarget): ClickSound {
  const gestures: GestureTarget = target ?? {
    addEventListener: (type, listener): void => {
      window.addEventListener(type, listener);
    },
    removeEventListener: (type, listener): void => {
      window.removeEventListener(type, listener);
    },
  };

  let context: AudioContext | null = null;
  let master: GainNode | null = null;
  let muted = false;
  let listening = true;

  const stopListening = (): void => {
    if (!listening) return;
    listening = false;
    for (const gesture of GESTURES) {
      gestures.removeEventListener(gesture, onGesture);
    }
  };

  /**
   * Opens the context, once, inside the gesture that reached us.
   *
   * The listeners come off whatever happened, including failure: a browser
   * that cannot give us a context this time will not give us one on the
   * second press either, and going quiet is the whole of the fallback.
   */
  function onGesture(): void {
    stopListening();

    try {
      const source = window as unknown as AudioContextSource;
      const Constructor = source.AudioContext ?? source.webkitAudioContext;
      if (!Constructor) return;

      context = new Constructor();
      master = context.createGain();
      master.gain.value = MASTER_GAIN;
      master.connect(context.destination);

      // Made inside the gesture, so a context that starts suspended is
      // allowed to resume here and nowhere later.
      void context.resume?.().catch((): void => {});
    } catch {
      context = null;
      master = null;
    }
  }

  for (const gesture of GESTURES) {
    gestures.addEventListener(gesture, onGesture);
  }

  return {
    play(): void {
      if (muted || !context || !master) return;

      try {
        const now = context.currentTime;
        const oscillator = context.createOscillator();
        const envelope = context.createGain();

        oscillator.type = 'triangle';
        oscillator.frequency.setValueAtTime(START_HZ, now);
        oscillator.frequency.exponentialRampToValueAtTime(
          END_HZ,
          now + CLICK_SECONDS,
        );

        // Up almost instantly and down over the rest: a turn seating itself
        // is an impact, and an audible attack would make it a beep. The floor
        // is not zero because an exponential ramp cannot reach it.
        envelope.gain.setValueAtTime(0.0001, now);
        envelope.gain.exponentialRampToValueAtTime(VOICE_GAIN, now + 0.002);
        envelope.gain.exponentialRampToValueAtTime(
          0.0001,
          now + CLICK_SECONDS,
        );

        oscillator.connect(envelope);
        envelope.connect(master);
        oscillator.start(now);
        oscillator.stop(now + CLICK_SECONDS);

        // Dropped as soon as it has finished sounding; a played sequence
        // would otherwise leave a node behind for every move of it.
        oscillator.onended = (): void => {
          oscillator.disconnect();
          envelope.disconnect();
        };
      } catch {
        // One click that could not be made is one click nobody hears.
      }
    },

    setMuted(next: boolean): void {
      muted = next;
    },

    isMuted(): boolean {
      return muted;
    },

    teardown(): void {
      stopListening();
      const closing = context;
      context = null;
      master = null;
      try {
        void closing?.close?.().catch((): void => {});
      } catch {
        // A context that will not close is one the page is discarding anyway.
      }
    },
  };
}
