import { beforeEach, describe, expect, it, vi } from 'vitest';

import { bootstrap } from '../../src/bootstrap.ts';
import type { StartAppOptions } from '../../src/AppLifecycle.ts';

/**
 * Thin integration tests for the real DOM wiring: the lifecycle itself is
 * stubbed, but state transitions land on the actual #app container.
 */
describe('bootstrap', () => {
  beforeEach(() => {
    vi.spyOn(console, 'error').mockImplementation(() => {});
  });

  function createApp(): HTMLElement {
    const app = document.createElement('div');
    app.id = 'app';
    document.body.replaceChildren(app);
    return app;
  }

  it('renders the stage and reaches the ready UI on success', async () => {
    const app = createApp();

    await bootstrap(
      app,
      async (options) => {
        options.setState('ready', 'ThorVG software renderer');
        return { teardown: () => {} };
      },
      () => true,
      () => null,
    );

    expect(app.querySelector('canvas')).not.toBeNull();
    expect(app.querySelector('#share')).not.toBeNull();

    // The commands that moved into Settings are still here, and still the
    // same ids: what changed is where they are on the page, not what the
    // controller finds when it goes looking for them.
    for (const id of [
      '#reset',
      '#cube-size',
      '#speed',
      '#mute',
      '#home-view',
      '#lighting-ambient',
      '#lighting-reset',
    ]) {
      expect(app.querySelector(id)?.closest('#settings-panel')).not.toBeNull();
    }
    expect(app.querySelector('#settings-panel')?.hasAttribute('hidden')).toBe(
      true,
    );
    expect(
      app.querySelector('#settings-trigger')?.getAttribute('aria-expanded'),
    ).toBe('false');

    // The board a session's solves are written on, which starts empty and
    // says so through the markup rather than waiting for a first draw.
    expect(app.querySelector('#record-best')?.textContent).toBe(
      'No solves yet.',
    );
    expect(app.querySelector('#record-list')?.children).toHaveLength(0);

    // The three detail panels start closed, each behind its own button, and
    // everything they hold is in the document already.
    for (const name of ['moves', 'session', 'turn']) {
      expect(app.querySelector(`#panel-${name}`)?.hasAttribute('hidden')).toBe(
        true,
      );
      expect(
        app.querySelector(`[aria-controls="panel-${name}"]`)?.getAttribute(
          'aria-expanded',
        ),
      ).toBe('false');
    }
    expect(app.querySelector('#move-log')?.closest('#panel-moves')).not.toBeNull();
    expect(app.querySelector('#record-best')?.closest('#panel-session')).not.toBeNull();
    expect(app.querySelector('#turn-depth')?.closest('#panel-turn')).not.toBeNull();

    // A panel opens from its button and stays until the same button.
    app.querySelector<HTMLButtonElement>('#details-moves')?.click();
    expect(app.querySelector('#panel-moves')?.hasAttribute('hidden')).toBe(false);
    document.body.click();
    expect(app.querySelector('#panel-moves')?.hasAttribute('hidden')).toBe(false);

    // Watch sits with Scene and Diagram rather than in the action dock.
    expect(app.querySelector('#ambient')?.closest('.view-rail')).not.toBeNull();
    expect(app.querySelector('#ambient')?.getAttribute('aria-pressed')).toBe(
      'false',
    );
    expect(app.querySelector('[data-view="both"]')?.getAttribute('aria-pressed')).toBe('true');
    expect(app.querySelector('[data-view="2d"]')?.getAttribute('aria-pressed')).toBe('false');
    expect(app.querySelector('[data-flat="net"]')?.getAttribute('aria-pressed')).toBe('true');

    // Split is the visible word for the mode the attribute still calls both,
    // so it cannot be mistaken for the diagram's own Both beside it.
    expect(app.querySelector('[data-view="both"]')?.textContent).toBe('Split');

    // The page's own controls are usable before an engine arrives: a theme
    // is not a thing the cube owns.
    expect(
      app.querySelector<HTMLButtonElement>('#settings-trigger')?.disabled,
    ).toBe(false);
    expect(
      app.querySelector<HTMLButtonElement>('[data-theme-choice="system"]')
        ?.disabled,
    ).toBe(false);
    for (const id of ['#command-trigger', '#cube-size-chip', '#details-turn']) {
      expect(app.querySelector<HTMLButtonElement>(id)?.disabled).toBe(false);
    }
    expect(app.dataset.state).toBe('ready');
    expect(app.querySelector('#status')?.textContent).toBe(
      'ThorVG software renderer',
    );
  });

  it('shows the error UI when startup rejects', async () => {
    const app = createApp();

    await bootstrap(
      app,
      async () => {
        throw new Error('startup failed');
      },
      () => true,
      () => null,
    );

    expect(app.dataset.state).toBe('error');
    expect(app.querySelector('#status')?.textContent).toBe(
      'Failed to start the ThorVG engine.',
    );
  });

  it('shows the error UI when a failure arrives after ready', async () => {
    const app = createApp();
    let reportError: StartAppOptions['onError'] = () => {};

    await bootstrap(
      app,
      async (options) => {
        options.setState('ready', 'ThorVG software renderer');
        reportError = options.onError;
        return { teardown: () => {} };
      },
      () => true,
      () => null,
    );
    expect(app.dataset.state).toBe('ready');

    reportError(new Error('lost the engine'));

    expect(app.dataset.state).toBe('error');
    expect(app.querySelector('#status')?.textContent).toBe(
      'Failed to start the ThorVG engine.',
    );
  });

  it('shows unsupported without starting the engine', async () => {
    const app = createApp();
    const start = vi.fn();

    await bootstrap(app, start, () => false, () => null);

    expect(start).not.toHaveBeenCalled();
    expect(app.dataset.state).toBe('unsupported');
    expect(app.querySelector('#status')?.textContent).toContain(
      'does not support',
    );
  });
});
