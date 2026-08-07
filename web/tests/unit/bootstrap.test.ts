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

    await bootstrap(app, async (options) => {
      options.setState('ready', 'ThorVG software renderer');
      return { teardown: () => {} };
    });

    expect(app.querySelector('canvas')).not.toBeNull();
    expect(app.dataset.state).toBe('ready');
    expect(app.querySelector('#status')?.textContent).toBe(
      'ThorVG software renderer',
    );
  });

  it('shows the error UI when startup rejects', async () => {
    const app = createApp();

    await bootstrap(app, async () => {
      throw new Error('startup failed');
    });

    expect(app.dataset.state).toBe('error');
    expect(app.querySelector('#status')?.textContent).toBe(
      'Failed to start the ThorVG engine.',
    );
  });

  it('shows the error UI when a failure arrives after ready', async () => {
    const app = createApp();
    let reportError: StartAppOptions['onError'] = () => {};

    await bootstrap(app, async (options) => {
      options.setState('ready', 'ThorVG software renderer');
      reportError = options.onError;
      return { teardown: () => {} };
    });
    expect(app.dataset.state).toBe('ready');

    reportError(new Error('lost the engine'));

    expect(app.dataset.state).toBe('error');
    expect(app.querySelector('#status')?.textContent).toBe(
      'Failed to start the ThorVG engine.',
    );
  });
});
