import type { GameUi } from '../game/GameController.ts';
import type { LightingUi } from './LightingControls.ts';
import {
  DEFAULT_CUBE_SIZE,
  DEFAULT_SCRAMBLE_MOVES,
  MAX_CUBE_SIZE,
  MAX_SCRAMBLE_MOVES,
  MIN_CUBE_SIZE,
} from '../wasm/CubeEngine.ts';

/**
 * The generated shell, and the elements nothing else can find for itself.
 *
 * `ui` is what the gameplay controller is handed, and it has not changed
 * meaning: the same ids, the same attributes, the same fields. Everything
 * beside it is page furniture -- the settings drawer, the detail panels and
 * their rail, the command menu, the hint over the stage -- and belongs to the
 * small controllers in this folder rather than to the game.
 */
export type GameShell = {
  readonly root: HTMLElement;
  readonly ui: GameUi;
  readonly settingsTrigger: HTMLButtonElement;
  readonly settingsPanel: HTMLElement;
  readonly settingsBackdrop: HTMLElement;
  readonly settingsClose: HTMLButtonElement;
  /** The lighting sliders, which the lifecycle wires once an engine is up. */
  readonly lightingUi: LightingUi;
  readonly themeButtons: readonly HTMLButtonElement[];
  /** The rail buttons that open and close the three detail panels. */
  readonly panelToggles: readonly HTMLButtonElement[];
  readonly interactionHint: HTMLElement;
  /** The size beside the title, which opens Settings at the size field. */
  readonly cubeSizeChip: HTMLButtonElement;
  /** The two buttons either side of the depth field. */
  readonly depthSteppers: readonly HTMLButtonElement[];
  readonly command: CommandShell;
};

/** The command menu's elements. */
export type CommandShell = {
  readonly trigger: HTMLButtonElement;
  readonly shortcut: HTMLElement;
  readonly dialog: HTMLElement;
  readonly backdrop: HTMLElement;
  readonly input: HTMLInputElement;
  readonly list: HTMLElement;
  readonly message: HTMLElement;
};

/**
 * Reads one required element, treating a missing one as a broken template.
 *
 * Neither nullable nor optional: every selector below names something this
 * module wrote a few lines earlier, so a miss is a typo in the markup rather
 * than a page that has to carry on without a button.
 */
function requireElement<ElementType extends Element>(
  host: ParentNode,
  selector: string,
): ElementType {
  const element = host.querySelector<ElementType>(selector);
  if (element === null) {
    throw new Error(`Game shell is missing ${selector}.`);
  }
  return element;
}

/** Reads a group of elements, refusing an empty one for the same reason. */
function requireAll<ElementType extends Element>(
  host: ParentNode,
  selector: string,
): ElementType[] {
  const elements = [...host.querySelectorAll<ElementType>(selector)];
  if (elements.length === 0) {
    throw new Error(`Game shell is missing every ${selector}.`);
  }
  return elements;
}

/**
 * One stroke icon, drawn here rather than pulled in as a package.
 *
 * Written without whitespace inside, so an icon adds nothing to the text of
 * the button it sits in: `Split` reads as `Split`, to a test and to a screen
 * reader alike.
 */
function icon(paths: string, size = 20): string {
  return `<svg viewBox="0 0 24 24" width="${size}" height="${size}" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">${paths}</svg>`;
}

/** The icon set, by what each one stands for. */
export const ICONS = {
  cube: '<path d="M12 3 20 7.5v9L12 21l-8-4.5v-9z"/><path d="M4 7.5 12 12l8-4.5"/><path d="M12 12v9"/>',
  split: '<rect x="4" y="3" width="16" height="18" rx="2.5"/><path d="M4 12h16"/>',
  net: '<path d="M9 3h6v6h6v6h-6v6H9v-6H3V9h6z"/>',
  watch: '<path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="3"/>',
  paint: '<path d="M20.5 3.5a2.1 2.1 0 0 0-3 0L9 12l3 3 8.5-8.5a2.1 2.1 0 0 0 0-3z"/><path d="M9 12c-2.5 0-4.5 1.8-4.5 4.3 0 1.2-.6 2.4-1.5 3.2 3.7.7 7.9-.4 9-4.5"/>',
  moves: '<path d="M9 6h11"/><path d="M9 12h11"/><path d="M9 18h11"/><path d="M4.5 6h.01"/><path d="M4.5 12h.01"/><path d="M4.5 18h.01"/>',
  session: '<circle cx="12" cy="13.5" r="7.5"/><path d="M12 9.5v4l2.5 2"/><path d="M9.5 2.5h5"/>',
  turn: '<rect x="3" y="6" width="5" height="5" rx="1"/><rect x="9.5" y="6" width="5" height="5" rx="1"/><rect x="16" y="6" width="5" height="5" rx="1"/><rect x="3" y="13" width="5" height="5" rx="1"/><rect x="9.5" y="13" width="5" height="5" rx="1"/><rect x="16" y="13" width="5" height="5" rx="1"/>',
  rewind: '<path d="M6 5v14"/><path d="M18 6 9.5 12 18 18z"/>',
  undo: '<path d="M9 14 4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 0 11H11"/>',
  redo: '<path d="m15 14 5-5-5-5"/><path d="M20 9H9.5a5.5 5.5 0 0 0 0 11H13"/>',
  scramble: '<path d="M16 4h4v4"/><path d="M4 20 20 4"/><path d="M20 16v4h-4"/><path d="m14 14 6 6"/><path d="M4 4l6 6"/>',
  stop: '<rect x="6.5" y="6.5" width="11" height="11" rx="2"/>',
  solve: '<path d="m5 19 9-9"/><path d="M16 3v4"/><path d="M14 5h4"/><path d="M19 10v3"/><path d="M17.5 11.5h3"/>',
  search: '<circle cx="11" cy="11" r="6.5"/><path d="m20 20-4.2-4.2"/>',
  share: '<path d="M12 15V3"/><path d="m7 8 5-5 5 5"/><path d="M5 12v7a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-7"/>',
  settings: '<path d="M4 7h9"/><path d="M17 7h3"/><circle cx="15" cy="7" r="2"/><path d="M4 17h3"/><path d="M11 17h9"/><circle cx="9" cy="17" r="2"/>',
  chevron: '<path d="m6 9 6 6 6-6"/>',
  pointer: '<rect x="6.5" y="3" width="11" height="18" rx="5.5"/><path d="M12 7v3"/>',
  play: '<path d="M8 5.5v13l10-6.5z"/>',
  minus: '<path d="M5 12h14"/>',
  plus: '<path d="M12 5v14"/><path d="M5 12h14"/>',
  home: '<path d="M4 11.5 12 5l8 6.5"/><path d="M6.5 10v9h11v-9"/>',
  sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
  sound: '<path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z"/><path d="M15.5 9a4 4 0 0 1 0 6"/>',
  reset: '<path d="M4 12a8 8 0 1 0 2.4-5.7"/><path d="M4 4v4.5h4.5"/>',
} as const;

/** One face-turn key: the letter, which way, and what a reader is told. */
const FACE_KEYS: readonly {
  readonly face: string;
  readonly turn: 1 | -1;
  readonly name: string;
}[] = [
  { face: 'r', turn: 1, name: 'right' },
  { face: 'r', turn: -1, name: 'right' },
  { face: 'l', turn: 1, name: 'left' },
  { face: 'l', turn: -1, name: 'left' },
  { face: 'u', turn: 1, name: 'upper' },
  { face: 'u', turn: -1, name: 'upper' },
  { face: 'd', turn: 1, name: 'down' },
  { face: 'd', turn: -1, name: 'down' },
  { face: 'f', turn: 1, name: 'front' },
  { face: 'f', turn: -1, name: 'front' },
  { face: 'b', turn: 1, name: 'back' },
  { face: 'b', turn: -1, name: 'back' },
];

/** The twelve face-turn keys, each with its face's colour beside the letter. */
function faceKeys(): string {
  return FACE_KEYS.map(({ face, turn, name }) => {
    const letter = face.toUpperCase();
    const written = turn === 1 ? letter : `${letter}′`;
    const direction = turn === 1 ? 'clockwise' : 'counter-clockwise';
    return `<button class="key-button" type="button" data-face="${face}" data-turn="${turn}" aria-label="Turn ${name} face ${direction}"><span class="move-dot" data-dot="${face}" aria-hidden="true"></span>${written}</button>`;
  }).join('');
}

/** The six sticker colours to paint with, in the order a cube is usually held. */
const SWATCHES: readonly { readonly value: number; readonly name: string }[] = [
  { value: 2, name: 'White' },
  { value: 3, name: 'Yellow' },
  { value: 4, name: 'Green' },
  { value: 5, name: 'Blue' },
  { value: 0, name: 'Red' },
  { value: 1, name: 'Orange' },
];

function swatches(): string {
  return SWATCHES.map(
    ({ value, name }, index) =>
      `<button class="swatch" type="button" data-sticker="${value}" aria-pressed="${index === 0}"><span class="swatch__chip" data-sticker-chip="${value}" aria-hidden="true"></span><span class="swatch__name">${name}</span><span class="swatch__tally" data-sticker-tally="${value}">0/9</span></button>`,
  ).join('');
}

/**
 * Writes the application markup and ties the typed references to it.
 *
 * One template string rather than a tree of createElement calls: what this
 * file is for is being read as the page's shape, and a shape is easier to read
 * written out than assembled. Every field is named explicitly on the way out
 * -- no cast stands in for the list -- so a control dropped from the markup is
 * a type error rather than an undefined at the first click.
 *
 * The order is the order a narrow screen reads it in, top to bottom, which is
 * also the order it is tabbed through. A wide screen lifts the two rails and
 * the panels out to the sides; nothing moves in the document to do it.
 */
export function createGameShell(host: HTMLElement): GameShell {
  host.innerHTML = `
<main class="game-shell" data-game-state="idle" data-sticker-palette="classic" data-panels="closed">
  <header class="app-header">
    <div class="brand">
      <span class="brand__mark" aria-hidden="true"><span></span><span></span><span></span><span></span></span>
      <div class="brand__text">
        <h1 class="brand__title" id="game-title">Rubik's Cube</h1>
        <p class="brand__byline">drawn with ThorVG</p>
      </div>
      <button class="chip-button size-chip" type="button" id="cube-size-chip"><span class="visually-hidden">Cube size </span><span id="cube-size-label">${DEFAULT_CUBE_SIZE}×${DEFAULT_CUBE_SIZE}×${DEFAULT_CUBE_SIZE}</span>${icon(ICONS.chevron, 16)}</button>
    </div>

    <div class="app-header__tools">
      <button class="pill-button command-trigger" type="button" id="command-trigger"
        aria-haspopup="dialog" aria-controls="command-palette" aria-expanded="false"
        aria-keyshortcuts="Meta+K Control+K" aria-label="Search commands or type moves">${icon(ICONS.search, 18)}<span class="command-trigger__label" aria-hidden="true">Search or type a move</span><kbd class="command-trigger__key" id="command-shortcut" aria-hidden="true">⌘K</kbd></button>
      <button class="pill-button" type="button" id="share" aria-label="Share">${icon(ICONS.share, 18)}<span class="pill-button__label" aria-hidden="true">Share</span></button>
      <button class="pill-button pill-button--icon" type="button" id="settings-trigger"
        aria-controls="settings-panel" aria-expanded="false" aria-label="Settings">${icon(ICONS.settings, 18)}</button>
    </div>
  </header>

  <div class="clock">
    <output class="timer" id="timer" aria-label="Elapsed time" aria-live="off">00:00.00</output>
    <p class="status-line" id="status" role="status" aria-live="polite">Loading engine…</p>
  </div>

  <section class="game-stage" aria-labelledby="game-title">
    <div class="stage-frame">
      <canvas id="view" aria-label="Interactive Rubik's Cube. Drag a sticker to turn a layer, or drag empty space to orbit the view."></canvas>
      <p class="interaction-hint" id="interaction-hint" aria-hidden="true">${icon(ICONS.pointer, 14)}<span>Drag a sticker to turn · drag empty space to orbit</span></p>
    </div>
  </section>

  <div class="view-rail" role="group" aria-label="View">
    <div class="rail-group" role="group" aria-label="Scene">
      <button class="rail-button" type="button" data-view="3d" aria-pressed="false">${icon(ICONS.cube)}<span>3D</span></button>
      <button class="rail-button" type="button" data-view="both" aria-pressed="true">${icon(ICONS.split)}<span>Split</span></button>
      <button class="rail-button" type="button" data-view="2d" aria-pressed="false">${icon(ICONS.net)}<span>2D</span></button>
    </div>
    <div class="rail-group rail-group--diagram" role="group" aria-label="Diagram">
      <button class="rail-button rail-button--text" type="button" data-flat="net" aria-pressed="true">Net</button>
      <button class="rail-button rail-button--text" type="button" data-flat="rings" aria-pressed="false">Rings</button>
      <button class="rail-button rail-button--text" type="button" data-flat="both" aria-pressed="false">Both</button>
    </div>
    <div class="rail-group rail-group--modes" role="group" aria-label="Modes">
      <button class="rail-button" type="button" id="ambient" aria-pressed="false">${icon(ICONS.watch)}<span>Watch</span></button>
      <button class="rail-button" type="button" id="paint" aria-pressed="false">${icon(ICONS.paint)}<span>Paint</span></button>
    </div>
  </div>

  <div class="detail-panels">
    <section class="panel" id="panel-moves" aria-labelledby="panel-moves-title" hidden>
      <header class="panel__header">
        <h2 class="panel__title" id="panel-moves-title">Moves</h2>
        <span class="panel__meta" id="moves-progress">0 / 0</span>
      </header>
      <div class="panel__section">
        <p class="panel__label">Scramble</p>
        <p class="scramble-text" id="scramble-text"></p>
      </div>
      <div class="panel__section">
        <p class="panel__label">Your moves</p>
        <ol class="move-log__list" id="move-log" aria-label="Your moves"></ol>
      </div>
    </section>

    <section class="panel" id="panel-session" aria-labelledby="panel-session-title" hidden>
      <header class="panel__header">
        <h2 class="panel__title" id="panel-session-title">Session</h2>
        <span class="panel__meta" id="record-tally">No solves</span>
      </header>
      <p class="records__best" id="record-best">No solves yet.</p>
      <ol class="records__list" id="record-list" aria-label="This session"></ol>
    </section>

    <section class="panel" id="panel-turn" aria-labelledby="panel-turn-title" hidden>
      <header class="panel__header">
        <h2 class="panel__title" id="panel-turn-title">Turn</h2>
        <span class="panel__meta">Face turns</span>
      </header>
      <p class="turn-keys">Keys <kbd>R</kbd><kbd>L</kbd><kbd>U</kbd><kbd>D</kbd><kbd>F</kbd><kbd>B</kbd> · hold <kbd>Shift</kbd> to reverse</p>
      <div class="turn-depth" role="group" aria-label="Which layers a face turn takes">
        <label class="turn-depth__label" for="turn-depth">Depth</label>
        <div class="stepper">
          <button class="stepper__button" type="button" data-step="-1" aria-controls="turn-depth" aria-label="Shallower turn">${icon(ICONS.minus, 16)}</button>
          <input class="stepper__input" type="number" id="turn-depth" inputmode="numeric" step="1" min="1" max="${DEFAULT_CUBE_SIZE - 1}" value="1">
          <button class="stepper__button" type="button" data-step="1" aria-controls="turn-depth" aria-label="Deeper turn">${icon(ICONS.plus, 16)}</button>
        </div>
        <button class="soft-button" type="button" id="turn-wide" aria-pressed="false">Wide</button>
      </div>
      <div class="move-grid" role="group" aria-label="Face turns">${faceKeys()}</div>
    </section>
  </div>

  <section class="paint-bar" id="paint-bar" aria-label="Colour your cube" hidden>
    <div class="paint-bar__swatches" role="group" aria-label="Sticker colour">${swatches()}</div>
    <div class="paint-bar__actions">
      <button class="soft-button" type="button" id="paint-fill">Fill face</button>
      <button class="soft-button soft-button--primary" type="button" id="paint-apply">Use this cube</button>
      <button class="soft-button" type="button" id="paint-cancel">Cancel</button>
    </div>
    <p class="paint-bar__note" id="paint-note" role="alert"></p>
  </section>

  <div class="timeline" role="group" aria-label="Move timeline">
    <ol class="timeline__moves" id="timeline-moves" aria-label="Your moves, around where the cube is"></ol>
    <div class="timeline__bar">
      <span class="timeline__label" id="timeline-scramble">No scramble</span>
      <div class="timeline__track" id="timeline-track" aria-hidden="true"><span class="timeline__scramble"></span><span class="timeline__done"></span><span class="timeline__head"></span></div>
      <span class="timeline__label timeline__label--end" id="timeline-progress">0 / 0</span>
    </div>
  </div>

  <p class="solver-note" id="solver-note" hidden>No solver for this cube size yet — Rewind still works.</p>

  <div class="action-dock" role="group" aria-label="Cube actions">
    <button class="dock-button dock-button--side" type="button" id="rewind">${icon(ICONS.rewind, 18)}<span class="dock-button__label">Rewind</span></button>
    <button class="dock-button dock-button--round" type="button" id="undo" aria-label="Undo">${icon(ICONS.undo)}</button>
    <button class="dock-button dock-button--primary" type="button" id="scramble">${icon(ICONS.scramble, 18)}<span>Scramble</span></button>
    <button class="dock-button dock-button--primary dock-button--stop" type="button" id="stop" hidden>${icon(ICONS.stop, 18)}<span>Stop</span></button>
    <button class="dock-button dock-button--round" type="button" id="redo" aria-label="Redo">${icon(ICONS.redo)}</button>
    <button class="dock-button dock-button--side" type="button" id="solve">${icon(ICONS.solve, 18)}<span class="dock-button__label">Solve</span></button>
  </div>

  <div class="details-rail" role="group" aria-label="Details">
    <button class="rail-button" type="button" id="details-moves" data-panel="moves" aria-controls="panel-moves" aria-expanded="false">${icon(ICONS.moves)}<span>Moves</span></button>
    <button class="rail-button" type="button" id="details-session" data-panel="session" aria-controls="panel-session" aria-expanded="false">${icon(ICONS.session)}<span>Session</span></button>
    <button class="rail-button" type="button" id="details-turn" data-panel="turn" aria-controls="panel-turn" aria-expanded="false">${icon(ICONS.turn)}<span>Turn</span></button>
  </div>

  <div class="settings-backdrop" id="settings-backdrop" hidden></div>

  <aside class="settings" id="settings-panel" role="dialog" aria-modal="true"
    aria-labelledby="settings-title" hidden>
    <header class="settings__header">
      <h2 class="settings__title" id="settings-title">Settings</h2>
      <button class="soft-button" type="button" id="settings-close">Done</button>
    </header>

    <div class="settings__group" role="group" aria-label="Appearance">
      <p class="settings__label">Appearance</p>
      <div class="segmented">
        <button type="button" data-theme-choice="system" aria-pressed="true">System</button>
        <button type="button" data-theme-choice="light" aria-pressed="false">Light</button>
        <button type="button" data-theme-choice="dark" aria-pressed="false">Dark</button>
      </div>
    </div>

    <div class="settings__group" role="group" aria-label="Sticker colors">
      <p class="settings__label">Sticker colors</p>
      <div class="segmented">
        <button type="button" data-palette="classic" aria-pressed="true">Classic</button>
        <button type="button" data-palette="high-contrast" aria-pressed="false">High contrast</button>
      </div>
    </div>

    <div class="settings__group">
      <p class="settings__label">Cube</p>
      <label class="number-field" for="cube-size">
        <span>Cube size</span>
        <input type="number" id="cube-size" inputmode="numeric" step="1" min="${MIN_CUBE_SIZE}" max="${MAX_CUBE_SIZE}" value="${DEFAULT_CUBE_SIZE}">
      </label>
      <label class="number-field" for="scramble-moves">
        <span>Scramble moves</span>
        <input type="number" id="scramble-moves" inputmode="numeric" step="1" min="1" max="${MAX_SCRAMBLE_MOVES}" value="${DEFAULT_SCRAMBLE_MOVES}">
      </label>
    </div>

    <div class="settings__group">
      <p class="settings__label">Motion and sound</p>
      <label class="slider-field" for="speed">
        <span>Animation speed</span>
        <input type="range" id="speed" min="0.25" max="4" step="0.25" value="1">
        <output id="speed-value" for="speed">1.00×</output>
      </label>
      <div class="settings__row">
        <button class="soft-button" type="button" id="mute" aria-pressed="false">Mute turns</button>
        <button class="soft-button" type="button" id="home-view">Home view</button>
      </div>
    </div>

    <div class="settings__group" role="group" aria-label="Lighting">
      <p class="settings__label">Lighting</p>
      <label class="slider-field" for="lighting-ambient">
        <span>Ambient</span>
        <input type="range" id="lighting-ambient" data-lighting="ambient" min="0" max="1.5" step="0.01">
        <output for="lighting-ambient" data-lighting-value="ambient"></output>
      </label>
      <label class="slider-field" for="lighting-attenuation">
        <span>Falloff</span>
        <input type="range" id="lighting-attenuation" data-lighting="attenuation" min="0" max="3" step="0.05">
        <output for="lighting-attenuation" data-lighting-value="attenuation"></output>
      </label>
      <label class="slider-field" for="lighting-saturation">
        <span>Saturation</span>
        <input type="range" id="lighting-saturation" data-lighting="saturation" min="0" max="2" step="0.01">
        <output for="lighting-saturation" data-lighting-value="saturation"></output>
      </label>
      <p class="settings__sublabel">Key light</p>
      <label class="slider-field" for="lighting-x">
        <span>X</span>
        <input type="range" id="lighting-x" data-lighting="x" min="-10" max="10" step="0.1">
        <output for="lighting-x" data-lighting-value="x"></output>
      </label>
      <label class="slider-field" for="lighting-y">
        <span>Y</span>
        <input type="range" id="lighting-y" data-lighting="y" min="-10" max="10" step="0.1">
        <output for="lighting-y" data-lighting-value="y"></output>
      </label>
      <label class="slider-field" for="lighting-z">
        <span>Z</span>
        <input type="range" id="lighting-z" data-lighting="z" min="-10" max="10" step="0.1">
        <output for="lighting-z" data-lighting-value="z"></output>
      </label>
      <label class="slider-field" for="lighting-diffuse">
        <span>Diffuse</span>
        <input type="range" id="lighting-diffuse" data-lighting="diffuse" min="0" max="1.5" step="0.01">
        <output for="lighting-diffuse" data-lighting-value="diffuse"></output>
      </label>
      <label class="slider-field" for="lighting-specular">
        <span>Specular</span>
        <input type="range" id="lighting-specular" data-lighting="specular" min="0" max="1.5" step="0.01">
        <output for="lighting-specular" data-lighting-value="specular"></output>
      </label>
      <label class="slider-field" for="lighting-shininess">
        <span>Shininess</span>
        <input type="range" id="lighting-shininess" data-lighting="shininess" min="1" max="128" step="1">
        <output for="lighting-shininess" data-lighting-value="shininess"></output>
      </label>
      <div class="settings__row">
        <button class="soft-button" type="button" id="lighting-reset">Reset lights</button>
        <button class="soft-button" type="button" id="lighting-copy">Copy values</button>
      </div>
      <output class="lighting-values" id="lighting-values" aria-label="Lighting values"></output>
    </div>

    <div class="settings__group settings__group--danger">
      <p class="settings__label">Danger zone</p>
      <button class="soft-button soft-button--danger" type="button" id="reset">Reset session</button>
    </div>
  </aside>

  <div class="command-backdrop" id="command-backdrop" hidden></div>

  <div class="command-palette" id="command-palette" role="dialog" aria-modal="true"
    aria-labelledby="command-title" hidden>
    <h2 class="visually-hidden" id="command-title">Command menu</h2>
    <div class="command-palette__field">
      ${icon(ICONS.search, 18)}
      <input class="command-palette__input" id="command-input" type="text"
        role="combobox" aria-expanded="true" aria-controls="command-list" aria-autocomplete="list"
        aria-label="Search commands or type moves" placeholder="Search commands or type moves"
        autocomplete="off" autocapitalize="off" spellcheck="false">
      <kbd class="command-palette__esc" aria-hidden="true">esc</kbd>
    </div>
    <p class="command-palette__message" id="command-message" aria-live="polite"></p>
    <div class="command-palette__list" id="command-list" role="listbox" aria-label="Commands"></div>
    <p class="command-palette__footer" aria-hidden="true"><span>↑↓ navigate</span><span>↵ run</span><span>esc close</span></p>
  </div>
</main>
`;

  const root = requireElement<HTMLElement>(host, '.game-shell');
  const canvas = requireElement<HTMLCanvasElement>(root, '#view');

  const ui: GameUi = {
    root,
    canvas,
    timer: requireElement<HTMLOutputElement>(root, '#timer'),
    status: requireElement<HTMLParagraphElement>(root, '#status'),
    scrambleButton: requireElement<HTMLButtonElement>(root, '#scramble'),
    scrambleMovesInput: requireElement<HTMLInputElement>(root, '#scramble-moves'),
    resetButton: requireElement<HTMLButtonElement>(root, '#reset'),
    cubeSizeInput: requireElement<HTMLInputElement>(root, '#cube-size'),
    cubeSizeLabel: requireElement<HTMLElement>(root, '#cube-size-label'),
    turnDepthInput: requireElement<HTMLInputElement>(root, '#turn-depth'),
    turnWideButton: requireElement<HTMLButtonElement>(root, '#turn-wide'),
    undoButton: requireElement<HTMLButtonElement>(root, '#undo'),
    redoButton: requireElement<HTMLButtonElement>(root, '#redo'),
    rewindButton: requireElement<HTMLButtonElement>(root, '#rewind'),
    solveButton: requireElement<HTMLButtonElement>(root, '#solve'),
    solverNote: requireElement<HTMLParagraphElement>(root, '#solver-note'),
    stopButton: requireElement<HTMLButtonElement>(root, '#stop'),
    shareButton: requireElement<HTMLButtonElement>(root, '#share'),
    recordBest: requireElement<HTMLParagraphElement>(root, '#record-best'),
    recordList: requireElement<HTMLOListElement>(root, '#record-list'),
    recordTally: requireElement<HTMLElement>(root, '#record-tally'),
    moveLogList: requireElement<HTMLOListElement>(root, '#move-log'),
    timeline: {
      strip: requireElement<HTMLOListElement>(root, '#timeline-moves'),
      track: requireElement<HTMLElement>(root, '#timeline-track'),
      scrambleLabel: requireElement<HTMLElement>(root, '#timeline-scramble'),
      progressLabel: requireElement<HTMLElement>(root, '#timeline-progress'),
      scrambleText: requireElement<HTMLElement>(root, '#scramble-text'),
      panelProgress: requireElement<HTMLElement>(root, '#moves-progress'),
    },
    ambientButton: requireElement<HTMLButtonElement>(root, '#ambient'),
    homeViewButton: requireElement<HTMLButtonElement>(root, '#home-view'),
    viewButtons: requireAll<HTMLButtonElement>(root, '[data-view]'),
    paintButton: requireElement<HTMLButtonElement>(root, '#paint'),
    paintBar: requireElement<HTMLElement>(root, '#paint-bar'),
    paintSwatches: requireAll<HTMLButtonElement>(root, '[data-sticker]'),
    paintFillButton: requireElement<HTMLButtonElement>(root, '#paint-fill'),
    paintApplyButton: requireElement<HTMLButtonElement>(root, '#paint-apply'),
    paintCancelButton: requireElement<HTMLButtonElement>(root, '#paint-cancel'),
    paintNote: requireElement<HTMLElement>(root, '#paint-note'),
    flatButtons: requireAll<HTMLButtonElement>(root, '[data-flat]'),
    paletteButtons: requireAll<HTMLButtonElement>(root, '[data-palette]'),
    muteButton: requireElement<HTMLButtonElement>(root, '#mute'),
    speedInput: requireElement<HTMLInputElement>(root, '#speed'),
    speedValue: requireElement<HTMLOutputElement>(root, '#speed-value'),
    moveButtons: requireAll<HTMLButtonElement>(root, '[data-face]'),
  };

  return {
    root,
    ui,
    settingsTrigger: requireElement<HTMLButtonElement>(root, '#settings-trigger'),
    settingsPanel: requireElement<HTMLElement>(root, '#settings-panel'),
    settingsBackdrop: requireElement<HTMLElement>(root, '#settings-backdrop'),
    settingsClose: requireElement<HTMLButtonElement>(root, '#settings-close'),
    lightingUi: {
      inputs: requireAll<HTMLInputElement>(root, '[data-lighting]'),
      readouts: requireAll<HTMLOutputElement>(root, '[data-lighting-value]'),
      resetButton: requireElement<HTMLButtonElement>(root, '#lighting-reset'),
      copyButton: requireElement<HTMLButtonElement>(root, '#lighting-copy'),
      values: requireElement<HTMLOutputElement>(root, '#lighting-values'),
    },
    themeButtons: requireAll<HTMLButtonElement>(root, '[data-theme-choice]'),
    panelToggles: requireAll<HTMLButtonElement>(root, '[data-panel]'),
    interactionHint: requireElement<HTMLElement>(root, '#interaction-hint'),
    cubeSizeChip: requireElement<HTMLButtonElement>(root, '#cube-size-chip'),
    depthSteppers: requireAll<HTMLButtonElement>(root, '[data-step]'),
    command: {
      trigger: requireElement<HTMLButtonElement>(root, '#command-trigger'),
      shortcut: requireElement<HTMLElement>(root, '#command-shortcut'),
      dialog: requireElement<HTMLElement>(root, '#command-palette'),
      backdrop: requireElement<HTMLElement>(root, '#command-backdrop'),
      input: requireElement<HTMLInputElement>(root, '#command-input'),
      list: requireElement<HTMLElement>(root, '#command-list'),
      message: requireElement<HTMLElement>(root, '#command-message'),
    },
  };
}
