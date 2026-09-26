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
