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

export {};
