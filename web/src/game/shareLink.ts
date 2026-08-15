/**
 * Where a shared state sits in a URL, and how it gets in and out of one.
 *
 * The fragment, rather than a query parameter: a fragment is never sent to the
 * server, and the Pages deployment routes on the path alone, so a link works
 * from a static host without a rule of its own. What is in it is the cube and
 * nothing else -- no seed, no identity, nothing about the person who shared it.
 *
 * Pure string work, so that the one module that touches the address bar is the
 * lifecycle and everything here can be read without a browser.
 */

/** The name the encoded state is carried under. */
export const SHARE_FRAGMENT_KEY = 's';

/** The part of `location` this module reads; injectable for tests. */
export type LocationLike = {
  readonly href: string;
  readonly hash: string;
};

/** The part of `history` it writes through; injectable for tests. */
export type HistoryLike = {
  replaceState(data: unknown, unused: string, url: string): void;
};

/**
 * The encoded state a fragment carries, or null when it carries none.
 *
 * Read through URLSearchParams so that `#s=...` beside anything else still
 * works. base64url shares no character with the escaping that parser undoes,
 * so nothing it does can alter a payload that was well formed.
 */
export function readShareFragment(hash: string): string | null {
  const text = hash.startsWith('#') ? hash.slice(1) : hash;
  if (text === '') return null;

  const value = new URLSearchParams(text).get(SHARE_FRAGMENT_KEY);
  return value === null || value === '' ? null : value;
}

/**
 * Takes the fragment off the address bar, without a history entry.
 *
 * Called whether the state in it was read or refused. A fragment left behind
 * would replay the same refusal on every reload, and would make a reload of a
 * shared link something other than the fresh start every other reload is.
 */
export function clearShareFragment(
  location: LocationLike,
  history: HistoryLike,
): void {
  const url = new URL(location.href);
  if (url.hash === '') return;

  url.hash = '';
  history.replaceState(null, '', `${url.pathname}${url.search}`);
}

/**
 * The link to this page carrying one encoded state.
 *
 * Built from a copy of the address rather than by assigning `location.hash`:
 * writing the address bar adds a history entry, scrolls the page to whatever
 * the fragment names, and would leave the sender's own address holding a state
 * -- which is exactly the thing every reload here is supposed not to have.
 */
export function shareUrl(href: string, encoded: string): string {
  const url = new URL(href);
  url.hash = `${SHARE_FRAGMENT_KEY}=${encoded}`;
  return url.toString();
}
