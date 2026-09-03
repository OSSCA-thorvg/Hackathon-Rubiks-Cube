/**
 * Engine surface the lighting sliders need; CubeEngine satisfies it.
 *
 * The list is the engine's own flat form -- ambient, attenuation, saturation,
 * then x, y, z, diffuse, specular, shininess per lamp -- and it is read from
 * the engine rather than kept here, so the page never carries a copy of the
 * defaults that could drift from what the cube is drawn under.
 */
export type LightingEngine = {
  lighting(): number[];
  /** @returns false when the engine refused the list and kept its lights. */
  setLighting(values: readonly number[]): boolean;
  render(): void;
};

/** Typed DOM elements the lighting controls own. */
export type LightingUi = {
  /** One slider per edited value, each naming its field in `data-lighting`. */
  readonly inputs: readonly HTMLInputElement[];
  /** The number beside each slider, matched by `data-lighting-value`. */
  readonly readouts: readonly HTMLOutputElement[];
  /** Puts the lights back to what the page opened with. */
  readonly resetButton: HTMLButtonElement;
  /** Copies the list as the address bar would take it. */
  readonly copyButton: HTMLButtonElement;
  /** The same list, written out, for reading off the panel. */
  readonly values: HTMLOutputElement;
};

/** One slider's place in the engine's list, and how it is written out. */
export type LightingField = {
  readonly name: string;
  readonly index: number;
  readonly decimals: number;
};

/**
 * The values the panel edits: the header and the first lamp.
 *
 * Only the first lamp, because it is the one the engine ships with and the
 * one that casts the shadow. A list with more lamps -- the address bar can
 * still pass one -- keeps them: the panel patches its nine indices and writes
 * the rest back untouched.
 */
export const LIGHTING_FIELDS: readonly LightingField[] = [
  { name: 'ambient', index: 0, decimals: 2 },
  { name: 'attenuation', index: 1, decimals: 2 },
  { name: 'saturation', index: 2, decimals: 2 },
  { name: 'x', index: 3, decimals: 1 },
  { name: 'y', index: 4, decimals: 1 },
  { name: 'z', index: 5, decimals: 1 },
  { name: 'diffuse', index: 6, decimals: 2 },
  { name: 'specular', index: 7, decimals: 2 },
  { name: 'shininess', index: 8, decimals: 0 },
];

/** How long the copy button reports what happened before reading Copy again. */
const COPY_NOTE_MS = 1400;

export type LightingControlsOptions = {
  readonly engine: LightingEngine;
  readonly ui: LightingUi;
  /** Called when the engine throws; the caller decides what that means. */
  readonly onError: (error: unknown) => void;
  /** Puts text on the clipboard; the page's navigator.clipboard by default. */
  readonly copy?: (text: string) => Promise<void>;
  /** Timer seams for the copy note; the window's in the page. */
  readonly setTimer?: (callback: () => void, ms: number) => number;
  readonly clearTimer?: (handle: number) => void;
};

export type LightingControls = {
  teardown(): void;
};

/**
 * The list as the `lighting` query parameter takes it.
 *
 * Numbers are written short -- up to three decimals, no trailing zeros -- so
 * a value read off a slider comes back as the slider showed it, and the
 * string is one a person would paste into Lighting::standard() as is.
 */
export function formatLightingQuery(values: readonly number[]): string {
  return values
    .map((value) => {
      const short = Number(value.toFixed(3));
      return Number.isFinite(short) ? String(short) : '0';
    })
    .join(',');
}

/**
 * Wires the lighting sliders to the engine.
 *
 * Every change is written to the engine as it happens and drawn at once: the
 * cube is still while it is being lit, so there is no frame loop to pick the
 * change up, and a slider that only took effect on the next turn would not be
 * tuning by eye. Reading back what the engine holds after each write is what
 * keeps the readouts honest -- the engine is the one place the lights live.
 *
 * This is a tuning tool rather than a setting, which is why nothing here is
 * stored: the values a tuning settles on go into the engine's defaults, and
 * the copy button is how they get there.
 */
export function attachLightingControls(
  options: LightingControlsOptions,
): LightingControls {
  const { engine, ui, onError } = options;
  const copy =
    options.copy ??
    ((text: string): Promise<void> => navigator.clipboard.writeText(text));
  const setTimer =
    options.setTimer ??
    ((callback: () => void, ms: number): number =>
      window.setTimeout(callback, ms));
  const clearTimer =
    options.clearTimer ?? ((handle: number): void => window.clearTimeout(handle));

  // What the page opened with: the engine's defaults, or the address bar's
  // list if there was one. Reset returns here, not to a copy of the defaults.
  const opening = engine.lighting();
  let current = [...opening];
  let active = true;
  let noteHandle: number | null = null;
  const copyLabel = ui.copyButton.textContent;

  const run = (action: () => void): void => {
    if (!active) return;
    try {
      action();
    } catch (error) {
      onError(error);
    }
  };

  const fieldOf = (input: HTMLInputElement): LightingField | null =>
    LIGHTING_FIELDS.find((field) => field.name === input.dataset.lighting) ??
    null;

  const readoutOf = (field: LightingField): HTMLOutputElement | null =>
    ui.readouts.find((output) => output.dataset.lightingValue === field.name) ??
    null;

  /** Writes the engine's list onto every control. */
  const show = (): void => {
    for (const input of ui.inputs) {
      const field = fieldOf(input);
      if (field === null) continue;
      const value = current[field.index];
      if (value === undefined) continue;
      input.value = String(value);
      const readout = readoutOf(field);
      if (readout !== null) readout.value = value.toFixed(field.decimals);
    }
    ui.values.value = formatLightingQuery(current);
  };

  /**
   * Hands a list to the engine and draws it.
   *
   * A refused list leaves the engine as it was, so the controls are simply
   * put back to what it still holds rather than left showing a value that
   * was never taken.
   */
  const apply = (next: readonly number[]): void => {
    if (engine.setLighting(next)) {
      current = [...next];
      engine.render();
    }
    show();
  };

  const onInput = (event: Event): void => {
    run((): void => {
      const input = event.currentTarget;
      if (!(input instanceof HTMLInputElement)) return;
      const field = fieldOf(input);
      if (field === null) return;

      const typed = Number(input.value);
      // A range input cannot produce anything else, but the engine refuses
      // what is not a number and the control should not ask it to.
      if (!Number.isFinite(typed)) {
        show();
        return;
      }

      const next = [...current];
      next[field.index] = typed;
      apply(next);
    });
  };

  const onReset = (): void => {
    run((): void => apply(opening));
  };

  const note = (text: string): void => {
    if (!active) return;
    ui.copyButton.textContent = text;
    if (noteHandle !== null) clearTimer(noteHandle);
    noteHandle = setTimer((): void => {
      noteHandle = null;
      if (active) ui.copyButton.textContent = copyLabel;
    }, COPY_NOTE_MS);
  };

  const onCopy = (): void => {
    if (!active) return;
    copy(formatLightingQuery(current)).then(
      (): void => note('Copied'),
      // A clipboard that will not take the text is not an engine failure and
      // the list is still on the panel to be read, so this says so and stops.
      (): void => note('Copy failed'),
    );
  };

  for (const input of ui.inputs) {
    input.addEventListener('input', onInput);
    input.disabled = false;
  }
  ui.resetButton.addEventListener('click', onReset);
  ui.copyButton.addEventListener('click', onCopy);
  ui.resetButton.disabled = false;
  ui.copyButton.disabled = false;
  show();

  return {
    teardown: (): void => {
      if (!active) return;
      active = false;
      if (noteHandle !== null) {
        clearTimer(noteHandle);
        noteHandle = null;
      }
      ui.copyButton.textContent = copyLabel;
      for (const input of ui.inputs) {
        input.removeEventListener('input', onInput);
      }
      ui.resetButton.removeEventListener('click', onReset);
      ui.copyButton.removeEventListener('click', onCopy);
    },
  };
}
