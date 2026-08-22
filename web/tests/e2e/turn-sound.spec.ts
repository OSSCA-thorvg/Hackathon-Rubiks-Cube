import { expect, test, type Page } from '@playwright/test';
import { fillInSettings, pressInSettings } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** How many moves the scrambles here are; short enough to finish quickly. */
const SCRAMBLE_MOVES = 6;

/**
 * Counts the voices the page builds, by standing in front of AudioContext.
 *
 * There is no way to hear a browser from a test, so what is checked is the one
 * observable that means a sound was made: an oscillator started on a real
 * context. Installed before any script of ours runs, so the constructor the
 * sound reaches for is this one.
 */
async function countVoices(page: Page): Promise<void> {
  await page.addInitScript(() => {
    const state = { contexts: 0, voices: 0 };
    (window as unknown as { __audio: typeof state }).__audio = state;

    const Real = window.AudioContext;
    if (!Real) return;

    window.AudioContext = function PatchedAudioContext() {
      state.contexts += 1;
      const context = new Real();
      const create = context.createOscillator.bind(context);
      context.createOscillator = (): OscillatorNode => {
        state.voices += 1;
        return create();
      };
      return context;
    } as unknown as typeof AudioContext;
  });
}

/** Reads the counter back out of the page. */
async function voices(page: Page): Promise<{ contexts: number; voices: number }> {
  return page.evaluate(
    () => (window as unknown as { __audio: { contexts: number; voices: number } }).__audio,
  );
}

/** Runs one scramble and waits for the sequence to finish playing. */
async function scramble(page: Page): Promise<void> {
  await fillInSettings(page, '#scramble-moves', String(SCRAMBLE_MOVES));
  await page.locator('#scramble').click();
  await expect(page.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'ready',
    { timeout: 20_000 },
  );
}

test.use({ viewport: DESKTOP });

test('a turn that lands makes a sound, and mute stops it', async ({ page }) => {
  await countVoices(page);
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  // Nothing has been touched yet, so no context has been opened -- the policy
  // that requires a gesture is the reason it is opened in one.
  expect((await voices(page)).contexts).toBe(0);

  await scramble(page);

  const afterScramble = await voices(page);
  expect(afterScramble.contexts).toBe(1);

  // One sound per turn that lands, at most: the pressing of Scramble is the
  // gesture that opened the context, and every move of the sequence that
  // followed committed while it played.
  expect(afterScramble.voices).toBeGreaterThan(0);
  expect(afterScramble.voices).toBeLessThanOrEqual(SCRAMBLE_MOVES);

  await pressInSettings(page, '#mute');
  await expect(page.locator('#mute')).toHaveAttribute('aria-pressed', 'true');

  await pressInSettings(page, '#reset');
  await scramble(page);

  // The same sequence again with nothing added to the count, and no second
  // context: muting is the sound refusing, not the page tearing anything down.
  const afterMute = await voices(page);
  expect(afterMute.voices).toBe(afterScramble.voices);
  expect(afterMute.contexts).toBe(1);
});

test('watching turns the cube in silence', async ({ page }) => {
  await countVoices(page);
  await page.goto('/');
  await expect(page.locator('.game-shell')).toBeVisible();

  // A gesture first, so the context exists and silence afterwards means the
  // sound was not asked for rather than that it could not be made.
  await pressInSettings(page, '#home-view');
  expect((await voices(page)).contexts).toBe(1);

  await page.locator('#ambient').click();
  await expect(page.locator('#ambient')).toHaveAttribute('aria-pressed', 'true');

  // Long enough for several turns of the pattern to have landed.
  await page.waitForTimeout(2000);

  // Watching never touches the record, so the observation the sound rides on
  // never changes and nothing here had to ask whether a pattern was running.
  expect((await voices(page)).voices).toBe(0);

  await page.locator('#ambient').click();
});
