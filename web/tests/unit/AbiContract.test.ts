import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

/**
 * The C ABI is written out by hand in four places: the functions the bindings
 * define, the export list the link is given, the declarations TypeScript
 * compiles against, and the calls CubeEngine makes. No build holds them to
 * each other. A function left off the export list is simply not exported, a
 * declaration is believed whatever the module holds, and either way the page
 * finds out when it makes the call. So they are held to each other here, by
 * name, before there is a module to load.
 *
 * By name and not by number: how many there are is whatever the four agree on.
 */

// jsdom's URL is not the one node:url knows, so the address goes over as text.
const read = (relative: string): string =>
  readFileSync(fileURLToPath(new URL(relative, import.meta.url).href), 'utf8');

/** Every name a pattern's first group captures, in order of name. */
const names = (source: string, pattern: RegExp): string[] =>
  [...source.matchAll(pattern)].map((match) => match[1]).sort();

// A definition is a line that begins with a return type, which is what tells
// it from a mention of the function in a comment.
const defined = names(
  read('../../../engine/src/bindings/capi.cpp'),
  /^[\w:<> ]*\b(thorvg_rubiks_\w+)\(/gm,
);
const exported = names(
  read('../../../engine/src/meson.build'),
  /'_(thorvg_rubiks_\w+)'/g,
);
const declared = names(
  read('../../src/wasm/generated/thorvg-rubiks.d.ts'),
  /^\s+_(thorvg_rubiks_\w+)\(/gm,
);
// One function is called from several places, and is one name.
const called = [
  ...new Set(
    names(
      read('../../src/wasm/CubeEngine.ts'),
      /module\._(thorvg_rubiks_\w+)\(/g,
    ),
  ),
];

describe('the C ABI', () => {
  it('exports every function the bindings define, and no other', () => {
    expect(defined).not.toHaveLength(0);
    expect(exported).toEqual(defined);
  });

  it('declares every export to TypeScript, and no other', () => {
    expect(declared).toEqual(exported);
  });

  it('is called in full by CubeEngine, which calls nothing undeclared', () => {
    expect(called).toEqual(declared);
  });
});
