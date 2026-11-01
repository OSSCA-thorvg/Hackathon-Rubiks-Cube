/**
 * Which page this is: the minimal ThorVG demo, or the full app.
 *
 * Decided once, from the address, before anything is built. The minimal page
 * is the cube, Scramble, Reset and Solve, and a light and dark switch;
 * everything else -- the net, the menu, the panels, the settings, the keys --
 * is the full app's, which the `?extended` parameter opens. Nothing on the
 * minimal page links there.
 */
export type AppMode = 'minimal' | 'extended';

/** The query parameter that opens the full app. */
export const EXTENDED_PARAM = 'extended';

/** The mode an address's query string asks for; the default is minimal. */
export function appModeOf(search: string): AppMode {
  return new URLSearchParams(search).has(EXTENDED_PARAM)
    ? 'extended'
    : 'minimal';
}
