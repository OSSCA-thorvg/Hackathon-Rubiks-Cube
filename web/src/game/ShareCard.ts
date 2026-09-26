/** The elements the card is made of. */
export type ShareCardUi = {
  /** Carries `data-copied`, which the stylesheet draws the two kinds by. */
  readonly card: HTMLElement;
  readonly title: HTMLElement;
  readonly note: HTMLElement;
  /** The link itself, read-only and selectable. */
  readonly link: HTMLInputElement;
  readonly copyButton: HTMLButtonElement;
  readonly closeButton: HTMLButtonElement;
};

/** A link that was made, and what became of it. */
export type SharedLink = {
  readonly link: string;
  /** Whether it reached the clipboard. */
  readonly copied: boolean;
  /** What opening it gives somebody, in one sentence. */
  readonly opens: string;
};

/** The two timer calls the card uses; injectable so a test can run them. */
export type CardTimers = {
  setTimeout(callback: () => void, delayMs: number): number;
  clearTimeout(handle: number): void;
};

export type ShareCardOptions = {
  /** Puts one string on the clipboard, or rejects when it cannot. */
  readonly copy: (text: string) => Promise<void>;
  /** Where focus goes when the card closes with focus inside it. */
  readonly returnFocus?: HTMLElement;
  /** How long a copied link stays up while nobody is looking at it. */
  readonly lingerMs?: number;
  readonly timers?: CardTimers;
};

/** Long enough to read the link and decide to copy it again; no longer. */
export const SHARE_CARD_LINGER_MS = 7000;

/** How long the copy button says so after it has copied. */
const COPIED_FLASH_MS = 1600;

/**
 * The link Share made, shown by the button that made it.
 *
 * Not a dialog: nothing behind it stops working while it is up, and a copied
 * link needs nothing from anybody, so it goes away on its own -- though not
 * while the pointer is over it or focus is in it, since that is somebody
 * reading it. A link the clipboard refused does need something: it stays,
 * with the link selected and focused, until it is copied by hand or closed.
 *
 * The status line still says what happened, as it does for every command.
 * This is the part a person can act on: the link itself, to read, select or
 * copy again.
 */
export class ShareCard {
  private readonly ui: ShareCardUi;
  private readonly copy: (text: string) => Promise<void>;
  private readonly returnFocus: HTMLElement | null;
  private readonly lingerMs: number;
  private readonly timers: CardTimers;

  private shown: SharedLink | null = null;
  private lingering: number | null = null;
  private flashing: number | null = null;
  /** Whether the pointer is over the card, and whether focus is in it. */
  private pointerInside = false;
  private focusInside = false;
  private active = true;

  constructor(ui: ShareCardUi, options: ShareCardOptions) {
    this.ui = ui;
    this.copy = options.copy;
    this.returnFocus = options.returnFocus ?? null;
    this.lingerMs = options.lingerMs ?? SHARE_CARD_LINGER_MS;
    this.timers = options.timers ?? {
      setTimeout: (callback, delayMs) => window.setTimeout(callback, delayMs),
      clearTimeout: (handle) => window.clearTimeout(handle),
    };

    ui.copyButton.addEventListener('click', this.onCopy);
    ui.closeButton.addEventListener('click', this.onClose);
    ui.link.addEventListener('focus', this.onLinkFocus);
    ui.card.addEventListener('keydown', this.onKeyDown);
    ui.card.addEventListener('pointerenter', this.onPointerEnter);
    ui.card.addEventListener('pointerleave', this.onPointerLeave);
    ui.card.addEventListener('focusin', this.onFocusIn);
    ui.card.addEventListener('focusout', this.onFocusOut);
  }

  /** Whether the card is on screen. */
  isOpen(): boolean {
    return this.shown !== null;
  }

  /** Puts a link up, replacing whatever the card was showing. */
  show(shared: SharedLink): void {
    if (!this.active) return;
    this.shown = shared;

    this.ui.card.dataset.copied = String(shared.copied);
    this.ui.title.textContent = shared.copied ? 'Link copied' : 'Copy this link';
    this.ui.note.textContent = shared.copied
      ? shared.opens
      : `${shared.opens} The browser would not copy it, so select it and copy it yourself.`;
    this.ui.link.value = shared.link;
    this.ui.copyButton.textContent = 'Copy';
    this.ui.card.hidden = false;

    // Refused, it is somebody's next move: the link is where their hands go.
    if (!shared.copied) {
      this.ui.link.focus();
      this.ui.link.select();
    }
    this.linger();
  }

  /** Takes the card down, giving focus back if it was inside. */
  hide(): void {
    if (this.shown === null) return;
    const hadFocus = this.ui.card.contains(document.activeElement);

    this.shown = null;
    this.pointerInside = false;
    this.focusInside = false;
    this.stopLingering();
    this.ui.card.hidden = true;

    if (hadFocus) this.returnFocus?.focus();
  }

  teardown(): void {
    this.hide();
    this.active = false;
    if (this.flashing !== null) this.timers.clearTimeout(this.flashing);
    this.ui.copyButton.removeEventListener('click', this.onCopy);
    this.ui.closeButton.removeEventListener('click', this.onClose);
    this.ui.link.removeEventListener('focus', this.onLinkFocus);
    this.ui.card.removeEventListener('keydown', this.onKeyDown);
    this.ui.card.removeEventListener('pointerenter', this.onPointerEnter);
    this.ui.card.removeEventListener('pointerleave', this.onPointerLeave);
    this.ui.card.removeEventListener('focusin', this.onFocusIn);
    this.ui.card.removeEventListener('focusout', this.onFocusOut);
  }

  /** Somebody is on the card: the pointer over it, or focus in it. */
  private held(): boolean {
    return this.pointerInside || this.focusInside;
  }

  /**
   * Starts the countdown to going away, for a copied link nobody is on.
   *
   * Restarted rather than extended: each time somebody lets go of the card
   * they get the whole time again, which is the time it takes to read it.
   */
  private linger(): void {
    this.stopLingering();
    if (this.shown === null || !this.shown.copied || this.held()) return;
    this.lingering = this.timers.setTimeout(() => {
      this.lingering = null;
      this.hide();
    }, this.lingerMs);
  }

  private stopLingering(): void {
    if (this.lingering === null) return;
    this.timers.clearTimeout(this.lingering);
    this.lingering = null;
  }

  private readonly onCopy = (): void => {
    const shown = this.shown;
    if (shown === null) return;

    void this.copy(shown.link).then(
      (): void => {
        if (!this.active || this.shown !== shown) return;
        const copied = { ...shown, copied: true };
        this.shown = copied;
        this.ui.card.dataset.copied = 'true';
        this.ui.title.textContent = 'Link copied';
        this.ui.note.textContent = shown.opens;
        this.ui.copyButton.textContent = 'Copied';
        if (this.flashing !== null) this.timers.clearTimeout(this.flashing);
        this.flashing = this.timers.setTimeout(() => {
          this.flashing = null;
          this.ui.copyButton.textContent = 'Copy';
        }, COPIED_FLASH_MS);
        this.linger();
      },
      (): void => {
        if (!this.active || this.shown !== shown) return;
        this.ui.note.textContent = `${shown.opens} The browser would not copy it, so select it and copy it yourself.`;
        this.ui.link.focus();
        this.ui.link.select();
      },
    );
  };

  private readonly onClose = (): void => this.hide();

  /** The whole link, ready to copy, the moment it is focused. */
  private readonly onLinkFocus = (): void => this.ui.link.select();

  private readonly onKeyDown = (event: KeyboardEvent): void => {
    if (event.key !== 'Escape') return;
    event.preventDefault();
    this.hide();
  };

  private readonly onPointerEnter = (): void => {
    this.pointerInside = true;
    this.stopLingering();
  };

  private readonly onPointerLeave = (): void => {
    this.pointerInside = false;
    this.linger();
  };

  private readonly onFocusIn = (): void => {
    this.focusInside = true;
    this.stopLingering();
  };

  /** Focus moving between the card's own controls is not leaving it. */
  private readonly onFocusOut = (event: FocusEvent): void => {
    const next = event.relatedTarget;
    if (next instanceof Node && this.ui.card.contains(next)) return;
    this.focusInside = false;
    this.linger();
  };
}
