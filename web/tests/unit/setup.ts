/**
 * jsdom exposes ImageData only when a native canvas package is installed.
 * The unit tests need just the (data, width, height) constructor shape, so
 * provide a minimal spec-shaped stand-in when the global is missing.
 */
if (typeof globalThis.ImageData === 'undefined') {
  class FakeImageData {
    readonly data: Uint8ClampedArray;
    readonly width: number;
    readonly height: number;

    constructor(data: Uint8ClampedArray, width: number, height: number) {
      if (data.length !== width * height * 4) {
        throw new Error('ImageData length does not match its dimensions.');
      }
      this.data = data;
      this.width = width;
      this.height = height;
    }
  }

  globalThis.ImageData = FakeImageData as unknown as typeof ImageData;
}

/**
 * jsdom has no media queries. Every query answers no -- the narrow layout, no
 * preference for motion or colour -- and never changes, which is the page a
 * test builds. Stood in for here rather than asked about in the code, so the
 * page does not carry a branch no browser takes.
 */
if (typeof window.matchMedia !== 'function') {
  window.matchMedia = (query: string): MediaQueryList =>
    ({
      matches: false,
      media: query,
      onchange: null,
      addEventListener: (): void => {},
      removeEventListener: (): void => {},
      addListener: (): void => {},
      removeListener: (): void => {},
      dispatchEvent: (): boolean => false,
    }) as MediaQueryList;
}

/** jsdom lays nothing out, so there is nothing for an element to scroll to. */
if (typeof Element.prototype.scrollIntoView !== 'function') {
  Element.prototype.scrollIntoView = (): void => {};
}

export {};
