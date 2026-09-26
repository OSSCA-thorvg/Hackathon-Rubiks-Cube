/** Tells a listener when the answer to `oneAtATime` may have changed. */
export type WidthWatch = (listener: () => void) => () => void;

export type DetailPanelsOptions = {
  /**
   * The buttons that open and close the panels. Each names its panel by
   * `aria-controls`, and itself by `data-panel`.
   */
  readonly toggles: readonly HTMLButtonElement[];
  /**
   * Carries `data-panels="open"` while any panel is, which is what the wide
   * layout reads to make room for the column beside the stage.
   */
  readonly root: HTMLElement;
  /**
   * Whether only one panel may be open at a time. Asked at every press
   * rather than once, because it is a question about the window.
   */
  readonly oneAtATime: () => boolean;
  /** Where to hear that the window changed; absent in a test that does not. */
  readonly watchWidth?: WidthWatch;
};

export type DetailPanels = {
  /** Whether the panel of that name is on screen. */
  isOpen(name: string): boolean;
  toggle(name: string): void;
  teardown(): void;
};

/**
 * The Moves, Session and Turn panels, each opened and closed by its own button.
 *
 * Not popups. A panel stays where it was put until the same button is pressed
 * again: a press elsewhere, Escape, and a command that has nothing to do with
 * it all leave it open, because it is somewhere a person keeps an eye on while
 * they play rather than a question the page is waiting on an answer to.
 *
 * On a wide screen they are independent, and any of them can be open beside
 * the others in a column next to the stage. A narrow one has room for one
 * under the stage, so opening a second puts the first away -- and a window
 * narrowed while several were open keeps the one opened last.
 *
 * Nothing is remembered past the page, for the same reason the records are
 * not: a reload is a fresh start by rule.
 */
export function attachDetailPanels(options: DetailPanelsOptions): DetailPanels {
  const { toggles, root, oneAtATime } = options;

  const nameOf = (toggle: HTMLButtonElement): string => toggle.dataset.panel ?? '';

  const panelOf = (toggle: HTMLButtonElement): HTMLElement | null => {
    const id = toggle.getAttribute('aria-controls');
    return id === null ? null : root.querySelector<HTMLElement>(`#${id}`);
  };

  const toggleNamed = (name: string): HTMLButtonElement | undefined =>
    toggles.find((toggle) => nameOf(toggle) === name);

  /** The open panels, the one opened last at the end. */
  const opened: string[] = [];

  const apply = (): void => {
    for (const toggle of toggles) {
      const open = opened.includes(nameOf(toggle));
      toggle.setAttribute('aria-expanded', String(open));
      const panel = panelOf(toggle);
      if (panel !== null) panel.hidden = !open;
    }
    root.dataset.panels = opened.length > 0 ? 'open' : 'closed';
  };

  const close = (name: string): void => {
    const at = opened.indexOf(name);
    if (at !== -1) opened.splice(at, 1);
  };

  const toggle = (name: string): void => {
    if (toggleNamed(name) === undefined) return;

    if (opened.includes(name)) {
      close(name);
    } else {
      if (oneAtATime()) opened.length = 0;
      opened.push(name);
    }
    apply();
  };

  /** Put away all but the last one, when the window stops having room. */
  const onWidthChange = (): void => {
    if (!oneAtATime() || opened.length <= 1) return;
    opened.splice(0, opened.length - 1);
    apply();
  };

  const onClick = (event: Event): void => {
    toggle(nameOf(event.currentTarget as HTMLButtonElement));
  };

  for (const button of toggles) button.addEventListener('click', onClick);
  const unwatch = options.watchWidth?.(onWidthChange) ?? null;
  apply();

  return {
    isOpen: (name: string): boolean => opened.includes(name),
    toggle,
    teardown: (): void => {
      for (const button of toggles) button.removeEventListener('click', onClick);
      unwatch?.();
    },
  };
}
