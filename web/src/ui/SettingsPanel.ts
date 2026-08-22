/**
 * Everything that can hold focus inside the panel, in document order.
 *
 * Queried on every open rather than kept, because the panel's contents are
 * not fixed: a control that has just been disabled is not a place focus may
 * land, and a list taken once would go on offering it.
 */
/** Minimal keyboard event target, the same shape the game controller takes. */
export type KeyboardTarget = {
  addEventListener(type: 'keydown', listener: (event: KeyboardEvent) => void): void;
  removeEventListener(type: 'keydown', listener: (event: KeyboardEvent) => void): void;
};

const FOCUSABLE =
  'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';

export type SettingsPanelOptions = {
  readonly trigger: HTMLButtonElement;
  readonly panel: HTMLElement;
  readonly backdrop: HTMLElement;
  /** The control inside the panel that closes it. */
  readonly close: HTMLButtonElement;
  /** Where key events are heard; the window in the page. */
  readonly keyboardTarget?: KeyboardTarget;
};

export type SettingsPanel = {
  isOpen(): boolean;
  open(): void;
  /** Closes and, unless told otherwise, gives focus back to the trigger. */
  close(options?: { readonly restoreFocus?: boolean }): void;
  teardown(): void;
};

/**
 * Opens and closes the settings surface and keeps focus where it belongs.
 *
 * One implementation for both shapes it takes: the drawer on a wide screen
 * and the sheet on a narrow one are the same element with the same behavior,
 * and only the stylesheet knows they look different. That is also why the
 * dialog semantics are not conditional -- a panel that traps focus on a
 * phone and leaks it on a laptop would be two behaviors to keep true.
 *
 * Nothing here reaches the engine. Whether a panel is open is a fact about
 * the page and about nothing else.
 */
export function attachSettingsPanel(
  options: SettingsPanelOptions,
): SettingsPanel {
  const { trigger, panel, backdrop, close: closeButton } = options;
  const keyboard: KeyboardTarget = options.keyboardTarget ?? {
    addEventListener: (_type, listener): void => {
      window.addEventListener('keydown', listener);
    },
    removeEventListener: (_type, listener): void => {
      window.removeEventListener('keydown', listener);
    },
  };

  let open = false;

  // Read fresh each time rather than kept: a control that has just been
  // disabled is not a place focus may land, and a list taken once would go
  // on offering it. The selector already excludes disabled controls, and
  // everything inside an open panel is on screen, so nothing else is
  // filtered out here.
  const focusable = (): HTMLElement[] => [
    ...panel.querySelectorAll<HTMLElement>(FOCUSABLE),
  ];

  const closePanel = (settings: { readonly restoreFocus?: boolean } = {}): void => {
    if (!open) return;
    open = false;

    panel.hidden = true;
    backdrop.hidden = true;
    trigger.setAttribute('aria-expanded', 'false');

    // Focus goes back to where it came from unless the page is being torn
    // down, when moving it would be reaching into a document nobody is
    // looking at any more.
    if (settings.restoreFocus !== false) trigger.focus();
  };

  const openPanel = (): void => {
    if (open) return;
    open = true;

    backdrop.hidden = false;
    panel.hidden = false;
    trigger.setAttribute('aria-expanded', 'true');

    // The first thing inside, so a keyboard arrives in the panel rather than
    // continuing through the page behind it.
    const first = focusable()[0] ?? closeButton;
    first.focus();
  };

  /**
   * Keeps Tab inside the panel while it is open.
   *
   * A wrap at each end rather than an inert background: `inert` would be the
   * better tool, and this stays with what every target browser already does
   * the same way.
   */
  const onKeyDown = (event: KeyboardEvent): void => {
    if (!open) return;

    if (event.key === 'Escape') {
      event.preventDefault();
      closePanel();
      return;
    }

    if (event.key !== 'Tab') return;

    const stops = focusable();
    if (stops.length === 0) return;

    const first = stops[0]!;
    const last = stops[stops.length - 1]!;
    const active = document.activeElement;

    if (event.shiftKey && (active === first || !panel.contains(active))) {
      event.preventDefault();
      last.focus();
      return;
    }
    if (!event.shiftKey && active === last) {
      event.preventDefault();
      first.focus();
    }
  };

  const onTriggerClick = (): void => {
    if (open) closePanel();
    else openPanel();
  };

  const onBackdropPointerDown = (): void => closePanel();
  const onCloseClick = (): void => closePanel();

  trigger.addEventListener('click', onTriggerClick);
  backdrop.addEventListener('pointerdown', onBackdropPointerDown);
  closeButton.addEventListener('click', onCloseClick);
  keyboard.addEventListener('keydown', onKeyDown);

  return {
    isOpen: (): boolean => open,
    open: openPanel,
    close: closePanel,
    teardown: (): void => {
      closePanel({ restoreFocus: false });
      trigger.removeEventListener('click', onTriggerClick);
      backdrop.removeEventListener('pointerdown', onBackdropPointerDown);
      closeButton.removeEventListener('click', onCloseClick);
      keyboard.removeEventListener('keydown', onKeyDown);
    },
  };
}
