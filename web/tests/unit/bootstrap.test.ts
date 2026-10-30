import { beforeEach, describe, expect, it, vi } from 'vitest';

import { appModeOf } from '../../src/AppMode.ts';
import { bootstrap } from '../../src/bootstrap.ts';
import type { StartAppOptions } from '../../src/AppLifecycle.ts';

/**
 * Thin integration tests for the real DOM wiring: the lifecycle itself is
 * stubbed, but state transitions land on the actual #app container.
 *
 * The full app's tests name its mode; the minimal page has a block of its
 * own at the end.
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
      'extended',
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

  /** The game controls, and what the command menu offers, with no engine. */
  function withoutEngine(app: HTMLElement): {
    enabled: string[];
    offered: string[];
  } {
    const shell = app.querySelector('.game-shell')!;
    const enabled = [
      ...shell.querySelectorAll<HTMLButtonElement | HTMLInputElement>(
        '#paint, #paint-fill, #paint-apply, #paint-cancel, [data-sticker], #scramble, #reset, [data-face], #undo, #ambient, [data-lighting], #lighting-reset, #lighting-copy',
      ),
    ]
      .filter((control) => !control.disabled)
      .map((control) => control.id || control.outerHTML.slice(0, 40));
    window.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'k', ctrlKey: true, bubbles: true }),
    );
    const offered = [
      ...app.querySelectorAll('#command-list [role="option"]'),
    ].map((option) => option.textContent ?? '');
    // The menu did open, and offers what the page itself can still do.
    expect(offered.some((label) => label.includes('Settings'))).toBe(true);
    return { enabled, offered };
  }

  it('leaves every game control off while the engine loads', async () => {
    const app = createApp();

    void bootstrap(
      app,
      () => new Promise<never>(() => {}),
      () => true,
      () => null,
      'extended',
    );
    await Promise.resolve();

    // Paint among them: a press on it would reach nothing, and the command
    // menu, which offers only what can be pressed, would offer it too.
    const { enabled, offered } = withoutEngine(app);
    expect(enabled).toEqual([]);
    expect(offered.some((label) => label.includes('Colour your own cube'))).toBe(
      false,
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
      'extended',
    );

    expect(app.dataset.state).toBe('error');
    expect(app.querySelector('#status')?.textContent).toBe(
      'Failed to start the ThorVG engine.',
    );
    const { enabled, offered } = withoutEngine(app);
    expect(enabled).toEqual([]);
    expect(offered.some((label) => label.includes('Colour your own cube'))).toBe(
      false,
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
      'extended',
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

    await bootstrap(app, start, () => false, () => null, 'extended');

    expect(start).not.toHaveBeenCalled();
    expect(app.dataset.state).toBe('unsupported');
    expect(app.querySelector('#status')?.textContent).toContain(
      'does not support',
    );
  });

  describe('the minimal page', () => {
    /** Boots the minimal page against a stubbed lifecycle it can drive. */
    async function bootMinimal(app: HTMLElement): Promise<{
      options: StartAppOptions;
      theme: ReturnType<typeof vi.fn>;
    }> {
      let started: StartAppOptions | null = null;
      const theme = vi.fn(() => null);
      await bootstrap(
        app,
        async (options) => {
          started = options;
          return { teardown: () => {} };
        },
        () => true,
        theme,
        'minimal',
      );
      return { options: started!, theme };
    }

    it('is what an address without ?extended asks for', () => {
      expect(appModeOf('')).toBe('minimal');
      expect(appModeOf('?lighting=1,2')).toBe('minimal');
      expect(appModeOf('?extended')).toBe('extended');
      expect(appModeOf('?extended=')).toBe('extended');
      expect(appModeOf('?lighting=1,2&extended')).toBe('extended');
    });

    it('is the default when the address does not ask for the full app', async () => {
      const app = createApp();
      const start = vi.fn(() => new Promise<never>(() => {}));

      void bootstrap(app, start, () => true, () => null);
      await Promise.resolve();

      expect(app.querySelector('.game-shell')?.getAttribute('data-mode')).toBe(
        'minimal',
      );
      expect(start.mock.calls[0]?.[0]).toMatchObject({ mode: 'minimal' });
    });

    it('hands the lifecycle the cube and nothing of the full app', async () => {
      const app = createApp();
      const { options, theme } = await bootMinimal(app);

      expect(options.mode).toBe('minimal');
      expect(options.lightingUi).toBeUndefined();
      expect(options.blocked).toBeUndefined();
      // The same canvases as the full app, so the scenes are split the same.
      expect(options.views?.map((view) => view.canvas.id)).toEqual([
        'view-net',
        'view-rings',
        'view-axes',
      ]);
      expect(theme).toHaveBeenCalledOnce();
    });

    it('has its own Reset and a link to the full app', async () => {
      const app = createApp();
      await bootMinimal(app);

      const link = app.querySelector<HTMLAnchorElement>('#full-version')!;
      expect(link.getAttribute('href')).toBe('./?extended');
      expect(link.getAttribute('aria-label')).toBe('Full version');
      expect(link.closest('.action-dock')).not.toBeNull();
      expect(app.querySelector('#minimal-reset')?.closest('.action-dock')).not.toBeNull();
    });

    it('resets through the full app\'s Home view and Reset session', async () => {
      const app = createApp();
      const { options } = await bootMinimal(app);
      const reset = app.querySelector<HTMLButtonElement>('#minimal-reset')!;
      const pressed: string[] = [];
      for (const id of ['#home-view', '#reset']) {
        const button = app.querySelector<HTMLButtonElement>(id)!;
        // As the game controller leaves them once it has attached.
        button.disabled = false;
        button.addEventListener('click', () => pressed.push(id));
      }

      // Off until there is a cube to reset.
      expect(reset.disabled).toBe(true);
      options.setState('ready', 'Ready.');
      expect(reset.disabled).toBe(false);

      reset.click();
      expect(pressed).toEqual(['#home-view', '#reset']);

      options.onError(new Error('lost the engine'));
      expect(reset.disabled).toBe(true);
    });

    it('attaches no menu, no Settings and no panels', async () => {
      const app = createApp();
      await bootMinimal(app);

      window.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'k', metaKey: true, bubbles: true }),
      );
      window.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'k', ctrlKey: true, bubbles: true }),
      );
      expect(app.querySelector('#command-palette')?.hasAttribute('hidden')).toBe(
        true,
      );
      expect(app.querySelector('#command-list')?.children).toHaveLength(0);

      app.querySelector<HTMLButtonElement>('#settings-trigger')!.click();
      expect(app.querySelector('#settings-panel')?.hasAttribute('hidden')).toBe(
        true,
      );
      app.querySelector<HTMLButtonElement>('#details-moves')!.click();
      expect(app.querySelector('#panel-moves')?.hasAttribute('hidden')).toBe(
        true,
      );
    });
  });
});
