import { describe, expect, it, vi } from 'vitest';

import {
  attachLightingControls,
  formatLightingQuery,
  LIGHTING_FIELDS,
  type LightingEngine,
  type LightingUi,
} from '../../src/ui/LightingControls.ts';

/** The engine's standard lights, as the page would read them. */
const STANDARD = [0.75, 1, 1.25, 2.6, 7, 4, 0.38, 0.6, 12];

/** The same markup the shell writes, one slider and readout per field. */
function createUi(): LightingUi {
  const root = document.createElement('div');
  root.innerHTML = `
    ${LIGHTING_FIELDS.map(
      (field) => `
      <label>
        <input type="range" data-lighting="${field.name}" min="-10" max="128" step="0.01" disabled>
        <output data-lighting-value="${field.name}"></output>
      </label>`,
    ).join('')}
    <button type="button" id="lighting-reset" disabled>Reset lights</button>
    <button type="button" id="lighting-copy" disabled>Copy values</button>
    <output id="lighting-values"></output>
  `;
  document.body.replaceChildren(root);

  return {
    inputs: [...root.querySelectorAll<HTMLInputElement>('[data-lighting]')],
    readouts: [
      ...root.querySelectorAll<HTMLOutputElement>('[data-lighting-value]'),
    ],
    resetButton: root.querySelector<HTMLButtonElement>('#lighting-reset')!,
    copyButton: root.querySelector<HTMLButtonElement>('#lighting-copy')!,
    values: root.querySelector<HTMLOutputElement>('#lighting-values')!,
  };
}

/** An engine that holds one list and can be told to refuse the next write. */
function createEngine(opening: readonly number[] = STANDARD) {
  let lighting = [...opening];
  let refuse = false;
  const engine = {
    lighting: vi.fn((): number[] => [...lighting]),
    setLighting: vi.fn((values: readonly number[]): boolean => {
      if (refuse) return false;
      lighting = [...values];
      return true;
    }),
    render: vi.fn(),
  } satisfies LightingEngine;

  return {
    engine,
    current: (): number[] => [...lighting],
    refuseNext(): void {
      refuse = true;
    },
  };
}

/** A clock the test advances by hand. */
function createClock() {
  const pending = new Map<number, () => void>();
  let next = 1;
  return {
    setTimer: (callback: () => void, _ms: number): number => {
      const handle = next++;
      pending.set(handle, callback);
      return handle;
    },
    clearTimer: (handle: number): void => {
      pending.delete(handle);
    },
    fire(): void {
      for (const [handle, callback] of [...pending]) {
        pending.delete(handle);
        callback();
      }
    },
    pendingCount: (): number => pending.size,
  };
}

function slider(ui: LightingUi, name: string): HTMLInputElement {
  return ui.inputs.find((input) => input.dataset.lighting === name)!;
}

function readout(ui: LightingUi, name: string): HTMLOutputElement {
  return ui.readouts.find((output) => output.dataset.lightingValue === name)!;
}

function slide(input: HTMLInputElement, value: string): void {
  input.value = value;
  input.dispatchEvent(new Event('input'));
}

function attach(
  overrides: Partial<Parameters<typeof attachLightingControls>[0]> = {},
) {
  const ui = createUi();
  const fake = createEngine();
  const clock = createClock();
  const onError = vi.fn();
  const copy = vi.fn((_text: string): Promise<void> => Promise.resolve());
  const controls = attachLightingControls({
    engine: fake.engine,
    ui,
    onError,
    copy,
    setTimer: clock.setTimer,
    clearTimer: clock.clearTimer,
    ...overrides,
  });
  return { ui, ...fake, clock, onError, copy, controls };
}

describe('formatLightingQuery', () => {
  it('writes the list the way the address bar takes it', () => {
    expect(formatLightingQuery(STANDARD)).toBe(
      '0.75,1,1.25,2.6,7,4,0.38,0.6,12',
    );
    // Short: a slider's step lands on a value that is exact in decimal, and
    // a float that is not is cut at three places rather than written long.
    expect(formatLightingQuery([0.1 + 0.2, 2 / 3])).toBe('0.3,0.667');
  });
});

describe('attachLightingControls', () => {
  it('shows the lights the engine holds and switches the controls on', () => {
    const { ui, engine } = attach();

    expect(engine.lighting).toHaveBeenCalledTimes(1);
    for (const field of LIGHTING_FIELDS) {
      expect(slider(ui, field.name).disabled).toBe(false);
      expect(Number(slider(ui, field.name).value)).toBe(STANDARD[field.index]);
    }
    expect(readout(ui, 'ambient').value).toBe('0.75');
    expect(readout(ui, 'x').value).toBe('2.6');
    expect(readout(ui, 'shininess').value).toBe('12');
    expect(ui.values.value).toBe('0.75,1,1.25,2.6,7,4,0.38,0.6,12');
    expect(ui.resetButton.disabled).toBe(false);
    expect(ui.copyButton.disabled).toBe(false);
    // Nothing is drawn for merely looking.
    expect(engine.render).not.toHaveBeenCalled();
  });

  it('writes a moved slider into the engine and draws it at once', () => {
    const { ui, engine, current } = attach();

    slide(slider(ui, 'specular'), '0.9');

    expect(engine.setLighting).toHaveBeenCalledWith([
      0.75, 1, 1.25, 2.6, 7, 4, 0.38, 0.9, 12,
    ]);
    expect(engine.render).toHaveBeenCalledTimes(1);
    expect(current()[7]).toBe(0.9);
    expect(readout(ui, 'specular').value).toBe('0.90');
    expect(ui.values.value).toBe('0.75,1,1.25,2.6,7,4,0.38,0.9,12');

    // The next move builds on the last, not on the opening list.
    slide(slider(ui, 'ambient'), '0.5');
    expect(engine.setLighting).toHaveBeenLastCalledWith([
      0.5, 1, 1.25, 2.6, 7, 4, 0.38, 0.9, 12,
    ]);
  });

  it('leaves lamps it does not edit exactly as the address bar set them', () => {
    const ui = createUi();
    const twoLamps = [...STANDARD, -5, 5, -5, 0, 0.6, 24];
    const fake = createEngine(twoLamps);
    attachLightingControls({
      engine: fake.engine,
      ui,
      onError: vi.fn(),
      copy: () => Promise.resolve(),
    });

    slide(slider(ui, 'y'), '6.5');

    expect(fake.current()).toEqual([
      0.75, 1, 1.25, 2.6, 6.5, 4, 0.38, 0.6, 12, -5, 5, -5, 0, 0.6, 24,
    ]);
    expect(ui.values.value).toBe(
      '0.75,1,1.25,2.6,6.5,4,0.38,0.6,12,-5,5,-5,0,0.6,24',
    );
  });

  it('puts a slider back when the engine refuses the list', () => {
    const { ui, engine, refuseNext } = attach();

    refuseNext();
    slide(slider(ui, 'diffuse'), '1.2');

    expect(engine.setLighting).toHaveBeenCalledTimes(1);
    expect(engine.render).not.toHaveBeenCalled();
    expect(Number(slider(ui, 'diffuse').value)).toBe(0.38);
    expect(readout(ui, 'diffuse').value).toBe('0.38');
    expect(ui.values.value).toBe('0.75,1,1.25,2.6,7,4,0.38,0.6,12');
  });

  it('does not ask the engine about a value that is not a number', () => {
    const { ui, engine } = attach();

    // A range input cannot hold this, so the slider is turned into a plain
    // field for the test: the guard is for a value the control did not make.
    slider(ui, 'z').type = 'text';
    slide(slider(ui, 'z'), 'abc');

    expect(engine.setLighting).not.toHaveBeenCalled();
    expect(engine.render).not.toHaveBeenCalled();
    expect(Number(slider(ui, 'z').value)).toBe(4);
  });

  it('resets to what the page opened with, not to the last write', () => {
    const { ui, engine, current } = attach();

    slide(slider(ui, 'ambient'), '0.2');
    slide(slider(ui, 'shininess'), '64');
    ui.resetButton.click();

    expect(current()).toEqual(STANDARD);
    expect(engine.render).toHaveBeenCalledTimes(3);
    expect(readout(ui, 'ambient').value).toBe('0.75');
    expect(readout(ui, 'shininess').value).toBe('12');
  });

  it('copies the list as a query value and says so for a moment', async () => {
    const { ui, copy, clock } = attach();

    slide(slider(ui, 'specular'), '0.8');
    ui.copyButton.click();
    await Promise.resolve();

    expect(copy).toHaveBeenCalledWith('0.75,1,1.25,2.6,7,4,0.38,0.8,12');
    expect(ui.copyButton.textContent).toBe('Copied');
    expect(clock.pendingCount()).toBe(1);

    clock.fire();
    expect(ui.copyButton.textContent).toBe('Copy values');
  });

  it('reports a clipboard that would not take the text, and nothing else', async () => {
    const { ui, onError, clock } = attach({
      copy: () => Promise.reject(new Error('denied')),
    });

    ui.copyButton.click();
    await Promise.resolve();
    await Promise.resolve();

    expect(ui.copyButton.textContent).toBe('Copy failed');
    expect(onError).not.toHaveBeenCalled();
    clock.fire();
    expect(ui.copyButton.textContent).toBe('Copy values');
  });

  it('routes an engine failure to onError', () => {
    const { ui, engine, onError } = attach();
    const failure = new Error('engine gone');
    engine.setLighting.mockImplementationOnce(() => {
      throw failure;
    });

    slide(slider(ui, 'ambient'), '0.9');

    expect(onError).toHaveBeenCalledWith(failure);
  });

  it('stops listening after teardown and clears its note', async () => {
    const { ui, engine, controls, clock } = attach();

    ui.copyButton.click();
    await Promise.resolve();
    expect(ui.copyButton.textContent).toBe('Copied');

    controls.teardown();

    expect(clock.pendingCount()).toBe(0);
    expect(ui.copyButton.textContent).toBe('Copy values');
    slide(slider(ui, 'ambient'), '0.1');
    ui.resetButton.click();
    expect(engine.setLighting).not.toHaveBeenCalled();
    // A second teardown is a no-op rather than a second unhooking.
    expect(() => controls.teardown()).not.toThrow();
  });
});
