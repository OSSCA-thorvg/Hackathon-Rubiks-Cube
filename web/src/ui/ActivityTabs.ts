export type ActivityTabsOptions = {
  readonly tabs: readonly HTMLButtonElement[];
  readonly panels: readonly HTMLElement[];
};

export type ActivityTabs = {
  /** Which panel is on screen, by its `data-activity` name. */
  selected(): string;
  select(name: string): void;
  teardown(): void;
};

/**
 * Two boards of the same width, one at a time.
 *
 * The move log and the session records used to sit under each other and push
 * the stage up the page between them. They hold the same kind of thing -- what
 * has happened so far -- so they share one frame and take turns in it.
 *
 * Both panels stay in the DOM and the one not chosen carries `hidden`. That
 * keeps the records readable to anything that reads text rather than pixels,
 * and it means the session board does not have to be rebuilt every time
 * somebody looks at it.
 */
export function attachActivityTabs(options: ActivityTabsOptions): ActivityTabs {
  const { tabs, panels } = options;

  const nameOf = (element: HTMLElement): string => element.dataset.activity ?? '';

  let current = nameOf(tabs[0] as HTMLElement);

  const apply = (): void => {
    for (const tab of tabs) {
      const chosen = nameOf(tab) === current;
      tab.setAttribute('aria-selected', String(chosen));
      // Only the chosen tab is a tab stop; the arrow keys move between them,
      // which is what a tab list is supposed to do.
      tab.tabIndex = chosen ? 0 : -1;
    }
    for (const panel of panels) panel.hidden = nameOf(panel) !== current;
  };

  const select = (name: string): void => {
    if (!tabs.some((tab) => nameOf(tab) === name)) return;
    current = name;
    apply();
  };

  const onClick = (event: Event): void => {
    const tab = event.currentTarget as HTMLButtonElement;
    select(nameOf(tab));
  };

  /** Left and right walk the list and take focus with them. */
  const onKeyDown = (event: KeyboardEvent): void => {
    const step =
      event.key === 'ArrowRight' ? 1 : event.key === 'ArrowLeft' ? -1 : 0;
    if (step === 0) return;

    event.preventDefault();
    const index = tabs.findIndex((tab) => nameOf(tab) === current);
    const next = tabs[(index + step + tabs.length) % tabs.length]!;
    select(nameOf(next));
    next.focus();
  };

  for (const tab of tabs) {
    tab.addEventListener('click', onClick);
    tab.addEventListener('keydown', onKeyDown);
  }
  apply();

  return {
    selected: (): string => current,
    select,
    teardown: (): void => {
      for (const tab of tabs) {
        tab.removeEventListener('click', onClick);
        tab.removeEventListener('keydown', onKeyDown);
      }
    },
  };
}
