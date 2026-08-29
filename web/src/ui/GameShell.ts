import type { GameUi } from '../game/GameController.ts';
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
 * beside it is page furniture that arrived with this layout -- the settings
 * panel, the activity tabs, the hint over the stage -- and belongs to the
 * small controllers in this folder rather than to the game.
 */
export type GameShell = {
  readonly root: HTMLElement;
  readonly ui: GameUi;
  readonly settingsTrigger: HTMLButtonElement;
  readonly settingsPanel: HTMLElement;
  readonly settingsBackdrop: HTMLElement;
  readonly settingsClose: HTMLButtonElement;
  readonly themeButtons: readonly HTMLButtonElement[];
  readonly activityTabs: readonly HTMLButtonElement[];
  readonly activityPanels: readonly HTMLElement[];
  readonly interactionHint: HTMLElement;
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

/** The gear, drawn here rather than pulled in as a package for one icon. */
const SETTINGS_ICON = `
<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true" focusable="false">
  <circle cx="12" cy="12" r="3.2" fill="none" stroke="currentColor" stroke-width="1.7"/>
  <path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"
    d="M12 2.6v2.2M12 19.2v2.2M21.4 12h-2.2M4.8 12H2.6M18.6 5.4l-1.6 1.6M7 17l-1.6 1.6M18.6 18.6 17 17M7 7 5.4 5.4"/>
</svg>`;

/**
 * Writes the application markup and ties the typed references to it.
 *
 * One template string rather than a tree of createElement calls: what this
 * file is for is being read as the page's shape, and a shape is easier to read
 * written out than assembled. Every field is named explicitly on the way out
 * -- no cast stands in for the list -- so a control dropped from the markup is
 * a type error rather than an undefined at the first click.
 */
export function createGameShell(host: HTMLElement): GameShell {
  host.innerHTML = `
<main class="game-shell" data-game-state="idle">
  <header class="app-header">
    <div class="app-header__identity">
      <p class="app-header__eyebrow">ThorVG showcase</p>
      <h1 class="app-header__title" id="game-title">Rubik's Cube</h1>
    </div>

    <div class="app-header__tools">
      <output class="timer" id="timer" aria-label="Elapsed time" aria-live="off">00:00.00</output>
      <button class="button button--ghost" type="button" id="share">Share</button>
      <button
        class="button button--icon"
        type="button"
        id="settings-trigger"
        aria-controls="settings-panel"
        aria-expanded="false"
        aria-label="Settings"
      >${SETTINGS_ICON}</button>
    </div>
  </header>

  <section class="game-stage" aria-labelledby="game-title">
    <canvas id="view" aria-label="Interactive Rubik's Cube. Drag a sticker to turn a layer, or drag empty space to orbit the view."></canvas>
    <p class="interaction-hint" id="interaction-hint" aria-hidden="true">Drag a sticker to turn · drag empty space to orbit</p>
  </section>

  <div class="action-dock" role="group" aria-label="Cube actions">
    <button class="button" type="button" id="undo">Undo</button>
    <button class="button" type="button" id="rewind">Rewind</button>
    <button class="button button--primary" type="button" id="scramble">Scramble</button>
    <button class="button button--danger" type="button" id="stop" hidden>Stop</button>
    <button class="button" type="button" id="solve">Solve</button>
    <button class="button" type="button" id="redo">Redo</button>
  </div>

  <p class="solver-note" id="solver-note" hidden>No solver for this cube size yet — Rewind still works.</p>

  <div class="view-bar">
    <div class="segmented" role="group" aria-label="Scene">
      <button type="button" data-view="3d" aria-pressed="false">3D</button>
      <button type="button" data-view="both" aria-pressed="true">Split</button>
      <button type="button" data-view="2d" aria-pressed="false">2D</button>
    </div>

    <div class="segmented" role="group" aria-label="Diagram">
      <button type="button" data-flat="net" aria-pressed="true">Net</button>
      <button type="button" data-flat="rings" aria-pressed="false">Rings</button>
      <button type="button" data-flat="both" aria-pressed="false">Both</button>
    </div>

    <button class="button" type="button" id="ambient" aria-pressed="false">Watch</button>
    <button class="button" type="button" id="paint" aria-pressed="false">Paint</button>
  </div>

  <section class="paint-bar" id="paint-bar" aria-label="Colour your cube" hidden>
    <div class="paint-bar__swatches" role="group" aria-label="Sticker colour">
      <button class="swatch" type="button" data-sticker="2" aria-pressed="true">
        <span class="swatch__chip" data-sticker-chip="2" aria-hidden="true"></span>
        <span class="swatch__name">White</span>
        <span class="swatch__tally" data-sticker-tally="2">0/9</span>
      </button>
      <button class="swatch" type="button" data-sticker="3" aria-pressed="false">
        <span class="swatch__chip" data-sticker-chip="3" aria-hidden="true"></span>
        <span class="swatch__name">Yellow</span>
        <span class="swatch__tally" data-sticker-tally="3">0/9</span>
      </button>
      <button class="swatch" type="button" data-sticker="4" aria-pressed="false">
        <span class="swatch__chip" data-sticker-chip="4" aria-hidden="true"></span>
        <span class="swatch__name">Green</span>
        <span class="swatch__tally" data-sticker-tally="4">0/9</span>
      </button>
      <button class="swatch" type="button" data-sticker="5" aria-pressed="false">
        <span class="swatch__chip" data-sticker-chip="5" aria-hidden="true"></span>
        <span class="swatch__name">Blue</span>
        <span class="swatch__tally" data-sticker-tally="5">0/9</span>
      </button>
      <button class="swatch" type="button" data-sticker="0" aria-pressed="false">
        <span class="swatch__chip" data-sticker-chip="0" aria-hidden="true"></span>
        <span class="swatch__name">Red</span>
        <span class="swatch__tally" data-sticker-tally="0">0/9</span>
      </button>
      <button class="swatch" type="button" data-sticker="1" aria-pressed="false">
        <span class="swatch__chip" data-sticker-chip="1" aria-hidden="true"></span>
        <span class="swatch__name">Orange</span>
        <span class="swatch__tally" data-sticker-tally="1">0/9</span>
      </button>
    </div>

    <div class="paint-bar__actions">
      <button class="button" type="button" id="paint-fill">Fill face</button>
      <button class="button button--primary" type="button" id="paint-apply">Use this cube</button>
      <button class="button" type="button" id="paint-cancel">Cancel</button>
    </div>

    <p class="paint-bar__note" id="paint-note" role="alert"></p>
  </section>

  <p class="status-line" id="status" role="status" aria-live="polite">Loading engine…</p>

  <section class="activity" aria-label="Activity">
    <div class="activity__tabs" role="tablist" aria-label="Activity">
      <button class="activity__tab" type="button" role="tab" id="activity-tab-moves"
        data-activity="moves" aria-controls="activity-panel-moves" aria-selected="true">Moves</button>
      <button class="activity__tab" type="button" role="tab" id="activity-tab-session"
        data-activity="session" aria-controls="activity-panel-session" aria-selected="false" tabindex="-1">Session</button>
    </div>

    <div class="activity__panel" role="tabpanel" id="activity-panel-moves" data-activity="moves"
      aria-labelledby="activity-tab-moves" tabindex="0">
      <ol class="move-log__list" id="move-log" aria-label="Your moves"></ol>
    </div>

    <div class="activity__panel" role="tabpanel" id="activity-panel-session" data-activity="session"
      aria-labelledby="activity-tab-session" tabindex="0" hidden>
      <p class="records__best" id="record-best">No solves yet.</p>
      <ol class="records__list" id="record-list" aria-label="This session"></ol>
    </div>
  </section>

  <details class="advanced">
    <summary>Advanced controls</summary>
    <p class="advanced__hint">Use R, L, U, D, F, or B. Hold Shift for a counter-clockwise turn.</p>
    <div class="turn-depth" role="group" aria-label="Which layers a face turn takes">
      <label class="number-field" for="turn-depth">
        <span>Depth</span>
        <input type="number" id="turn-depth" inputmode="numeric" step="1" min="1" max="${DEFAULT_CUBE_SIZE - 1}" value="1">
      </label>
      <button class="button" type="button" id="turn-wide" aria-pressed="false">Wide</button>
    </div>
    <div class="move-grid" aria-label="Face turns">
      <button class="button" type="button" data-face="r" data-turn="1" aria-label="Turn right face clockwise">R</button>
      <button class="button" type="button" data-face="r" data-turn="-1" aria-label="Turn right face counter-clockwise">R′</button>
      <button class="button" type="button" data-face="l" data-turn="1" aria-label="Turn left face clockwise">L</button>
      <button class="button" type="button" data-face="l" data-turn="-1" aria-label="Turn left face counter-clockwise">L′</button>
      <button class="button" type="button" data-face="u" data-turn="1" aria-label="Turn upper face clockwise">U</button>
      <button class="button" type="button" data-face="u" data-turn="-1" aria-label="Turn upper face counter-clockwise">U′</button>
      <button class="button" type="button" data-face="d" data-turn="1" aria-label="Turn down face clockwise">D</button>
      <button class="button" type="button" data-face="d" data-turn="-1" aria-label="Turn down face counter-clockwise">D′</button>
      <button class="button" type="button" data-face="f" data-turn="1" aria-label="Turn front face clockwise">F</button>
      <button class="button" type="button" data-face="f" data-turn="-1" aria-label="Turn front face counter-clockwise">F′</button>
      <button class="button" type="button" data-face="b" data-turn="1" aria-label="Turn back face clockwise">B</button>
      <button class="button" type="button" data-face="b" data-turn="-1" aria-label="Turn back face counter-clockwise">B′</button>
    </div>
  </details>

  <div class="settings-backdrop" id="settings-backdrop" hidden></div>

  <aside class="settings" id="settings-panel" role="dialog" aria-modal="true"
    aria-labelledby="settings-title" hidden>
    <header class="settings__header">
      <h2 class="settings__title" id="settings-title">Settings</h2>
      <button class="button button--ghost" type="button" id="settings-close">Done</button>
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
      <label class="slider-field" for="speed">
        <span>Animation speed</span>
        <input type="range" id="speed" min="0.25" max="4" step="0.25" value="1">
        <output id="speed-value" for="speed">1.00×</output>
      </label>
      <div class="settings__row">
        <button class="button" type="button" id="mute" aria-pressed="false">Mute turns</button>
        <button class="button" type="button" id="home-view">Home view</button>
      </div>
    </div>

    <div class="settings__group settings__group--danger">
      <p class="settings__label">Danger zone</p>
      <button class="button button--danger" type="button" id="reset">Reset session</button>
    </div>
  </aside>
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
    moveLogList: requireElement<HTMLOListElement>(root, '#move-log'),
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
    themeButtons: requireAll<HTMLButtonElement>(root, '[data-theme-choice]'),
    activityTabs: requireAll<HTMLButtonElement>(root, '[role="tab"]'),
    activityPanels: requireAll<HTMLElement>(root, '[role="tabpanel"]'),
    interactionHint: requireElement<HTMLElement>(root, '#interaction-hint'),
  };
}
