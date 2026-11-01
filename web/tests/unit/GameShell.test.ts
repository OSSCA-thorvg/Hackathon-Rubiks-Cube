import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  BRAND_MARK,
  createGameShell,
  THORVG_BOLT,
} from '../../src/ui/GameShell.ts';

describe('the brand mark', () => {
  it('draws the same bolt the favicon does', () => {
    // Two copies of one outline, since a favicon cannot reach into the page:
    // held to each other here, so neither can drift on its own.
    // Relative to the web package, which is where the tests are run from.
    const favicon = readFileSync(resolve('public/favicon.svg'), 'utf8');
    const drawn = /<path id="bolt" d="([^"]+)"/.exec(favicon)?.[1];
    expect(drawn).toBe(THORVG_BOLT);
  });

  it('keeps the bolt its margin in from the face, at its stroke', () => {
    const host = document.createElement('div');
    const shell = createGameShell(host);
    const bolt = shell.root.querySelector<SVGPathElement>('.brand__bolt')!;

    const [, x, y, scale] = /translate\(([\d.]+) ([\d.]+)\) scale\(([\d.]+)\)/
      .exec(bolt.getAttribute('transform') ?? '')!
      .map(Number);
    const stroke = Number(bolt.getAttribute('stroke-width'));

    // The outline's own box, as the path above spans it.
    const top = 0.150879;
    const bottom = 184.849;
    const right = 172.32;
    const { size, margin } = BRAND_MARK;
    expect(y! + top * scale!).toBeCloseTo(margin, 3);
    expect(y! + bottom * scale!).toBeCloseTo(size - margin, 3);
    // Centred across, the bolt being narrower than it is tall.
    expect(x! + x! + right * scale!).toBeCloseTo(size, 3);
    expect(stroke * scale!).toBeCloseTo(BRAND_MARK.stroke, 3);
  });
});

describe('the details panels', () => {
  it('come after the buttons that open them, with nothing to tab to between', () => {
    const shell = createGameShell(document.createElement('div'));
    const toggles = [
      ...shell.root.querySelectorAll<HTMLButtonElement>(
        '.details-rail [aria-controls]',
      ),
    ];
    expect(toggles).toHaveLength(3);

    for (const toggle of toggles) {
      const panel = shell.root.querySelector(
        `#${toggle.getAttribute('aria-controls')}`,
      )!;
      expect(
        toggle.compareDocumentPosition(panel) & Node.DOCUMENT_POSITION_FOLLOWING,
      ).toBeTruthy();
    }

    // Tab from the last of the buttons reaches a panel's first control, open
    // panels being the only ones anything can be tabbed to.
    const tabbable = [
      ...shell.root.querySelectorAll<HTMLElement>(
        'button, input, select, textarea, a[href], [tabindex]',
      ),
    ];
    const next = tabbable[tabbable.indexOf(toggles.at(-1)!) + 1];
    expect(next?.closest('.detail-panels')).not.toBeNull();
  });
});

describe('the two pages', () => {
  it('writes the full app by default, without the minimal page\'s controls', () => {
    const shell = createGameShell(document.createElement('div'));
    expect(shell.root.dataset.mode).toBe('extended');
    expect(shell.minimalReset).toBeNull();
    expect(shell.themeToggle).toBeNull();
    expect(shell.root.querySelector('#full-version')).toBeNull();
  });

  it('gives the minimal page a Reset in the dock and no link to the full app', () => {
    const shell = createGameShell(document.createElement('div'), 'minimal');
    expect(shell.root.dataset.mode).toBe('minimal');
    expect(shell.minimalReset?.id).toBe('minimal-reset');
    expect(shell.minimalReset?.disabled).toBe(true);

    // In the order the row reads: Scramble (or Stop), Reset, Solve.
    const dock = [
      ...shell.root.querySelectorAll<HTMLElement>('.action-dock > *'),
    ].map((control) => control.id);
    expect(dock).toEqual([
      'rewind',
      'undo',
      'scramble',
      'stop',
      'minimal-reset',
      'redo',
      'solve',
    ]);
    expect(shell.root.querySelector('a[href]')).toBeNull();
    expect(shell.ui.stage.dataset.viewMode).toBe('3d');
  });

  it('gives the minimal page a sun-and-moon button in the header', () => {
    const shell = createGameShell(document.createElement('div'), 'minimal');
    expect(shell.themeToggle?.closest('.app-header')).not.toBeNull();
    expect(shell.themeToggle?.getAttribute('aria-label')).toBe('Dark theme');
  });
});
