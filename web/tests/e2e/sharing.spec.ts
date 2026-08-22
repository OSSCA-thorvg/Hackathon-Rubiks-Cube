import { expect, test, type Page } from '@playwright/test';

import { probeCanvas } from './sceneContract.ts';
import { fillInSettings } from './shell.ts';

/** A viewport large enough to keep the desktop HUD beside the canvas. */
const DESKTOP = { width: 1200, height: 1200 };

/** Turns one face from the keyboard and waits for it to settle. */
async function turn(page: Page, key: string): Promise<void> {
  const moveButtons = page.locator('[data-face]');
  await page.keyboard.press(key);
  await expect(moveButtons.first()).toBeDisabled();
  await expect(moveButtons.first()).toBeEnabled();
}

/** Reads the link the Share button put on the clipboard. */
async function share(page: Page): Promise<string> {
  await page.locator('#share').click();
  await expect(page.locator('#status')).toHaveText(
    'Link copied. It opens this cube.',
  );
  return page.evaluate(() => navigator.clipboard.readText());
}

test.beforeEach(async ({ page, context }) => {
  // The clipboard is what the Share button writes to, and reading it back is
  // how a test sees the link. Both permissions are needed: writing is what the
  // page does, reading is what the test does afterwards.
  await context.grantPermissions(['clipboard-read', 'clipboard-write']);
  await page.setViewportSize(DESKTOP);
  await page.goto('./');
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
});

test('a link opens the same cube somewhere else', async ({ page, context }) => {
  const shell = page.locator('.game-shell');

  // Nothing has happened, so there is nothing to send.
  await expect(page.locator('#share')).toBeDisabled();

  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');

  await turn(page, 'r');
  await turn(page, 'u');
  await expect(page.locator('#move-log li')).toHaveCount(2);

  const sent = await probeCanvas(page);
  const link = await share(page);

  // The sender's own address is untouched: the state went onto a copy of it.
  expect(new URL(page.url()).hash).toBe('');
  expect(new URL(link).hash).toMatch(/^#s=[A-Za-z0-9_-]+$/);

  // And the copy is of wherever the page actually is, so the deployment's
  // subpath comes along with it rather than being assumed away.
  expect(new URL(link).pathname).toBe(new URL(page.url()).pathname);
  expect(new URL(link).pathname).toContain('/Hackathon-Rubiks-Cube/');

  // A page that has never seen this one, opened on the link alone.
  const other = await context.newPage();
  await other.goto(link);
  await expect(other.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(other.locator('#status')).toHaveText(
    'Ready. This cube came from a shared link.',
  );

  // The same cube, down to the pixel of the flat view.
  const received = await probeCanvas(other);
  expect(received.net).toEqual(sent.net);

  // The address is clean but still points at the same page, so the reload at
  // the end of this test is a fresh start rather than a 404.
  expect(new URL(other.url()).hash).toBe('');
  expect(new URL(other.url()).pathname).toBe(new URL(link).pathname);

  // Both of the sender's moves came across, and undo walks back through them.
  await expect(other.locator('#move-log li')).toHaveCount(2);
  await expect(other.locator('#undo')).toBeEnabled();
  await other.locator('#undo').click();
  await expect(other.locator('#rewind')).toBeEnabled();
  await expect(
    other.locator('#move-log li[data-state="pending"]'),
  ).toHaveCount(1);

  // And the scramble came across too, so a solve reaches all the way down and
  // a redo puts it back.
  await other.locator('#rewind').click();
  await expect(other.locator('#stop')).toBeHidden();
  await expect(other.locator('.game-shell')).toHaveAttribute(
    'data-game-state',
    'idle',
  );
  await expect(other.locator('#redo')).toBeEnabled();

  // Opened rather than played, so finishing it is not a record: the session
  // never started, because starting one is a scramble.
  await expect(other.locator('#record-best')).toHaveText('No solves yet.');

  await other.reload();
  await expect(other.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(other.locator('#status')).toHaveText(
    'Ready. Scramble the cube to begin.',
  );
  await expect(other.locator('#move-log li')).toHaveCount(0);
});

test('a damaged link opens a fresh cube and does not repeat itself', async ({
  context,
}) => {
  // Its own page, because a fragment is the one part of an address a browser
  // will change without loading anything: navigating the already-open page
  // would put the payload on it and never run the application again.
  const opened = await context.newPage();
  await opened.goto('./#s=this-is-not-a-payload');

  await expect(opened.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(opened.locator('#status')).toHaveText(
    'That shared link could not be read. Ready with a fresh cube.',
  );
  await expect(opened.locator('#move-log li')).toHaveCount(0);

  // The fragment is gone, so the refusal is not replayed on every reload.
  expect(new URL(opened.url()).hash).toBe('');

  await opened.reload();
  await expect(opened.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(opened.locator('#status')).toHaveText(
    'Ready. Scramble the cube to begin.',
  );
});

/**
 * Solves a one-move scramble by hand, without knowing which move it was.
 *
 * A scramble move is a quarter or half turn of one outer face, so turning some
 * face repeatedly reaches the solved cube within three of them -- and four
 * turns of the wrong face put the cube back exactly where it was, which is
 * what makes trying the next one a clean start rather than a deeper mess.
 */
async function solveByHand(page: Page): Promise<void> {
  const shell = page.locator('.game-shell');

  for (const key of ['r', 'l', 'u', 'd', 'f', 'b']) {
    for (let attempt = 0; attempt < 4; attempt += 1) {
      await turn(page, key);
      if ((await shell.getAttribute('data-game-state')) === 'completed') return;
    }
  }

  throw new Error('A one-move scramble was not solved by any single face.');
}

test('a session keeps its own solves and nothing else', async ({ page }) => {
  const shell = page.locator('.game-shell');

  await expect(page.locator('#record-best')).toHaveText('No solves yet.');

  // A rewind to the solved cube is a completed session that nobody solved.
  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');
  await turn(page, 'r');
  await page.locator('#rewind').click();
  await expect(shell).toHaveAttribute('data-game-state', 'completed');
  await expect(page.locator('#status')).toContainText('Not a solve of your own');
  await expect(page.locator('#record-best')).toHaveText('No solves yet.');
  await expect(page.locator('#record-list li')).toHaveCount(0);

  // A cube taken apart by one move and put back by hand is a solve of your
  // own, however many wrong turns went into it.
  await fillInSettings(page, '#scramble-moves', '1');
  await page.locator('#scramble').click();
  await expect(shell).toHaveAttribute('data-game-state', 'ready');

  await solveByHand(page);

  await expect(page.locator('#status')).toContainText('A new best');
  await expect(page.locator('#record-best')).toContainText('Best ');
  await expect(page.locator('#record-list li')).toHaveCount(1);
  await expect(page.locator('#record-list li').first()).toContainText(
    '1-move scramble',
  );

  // Nothing survives a reload, which is what "this session" means.
  await page.reload();
  await expect(page.locator('#app')).toHaveAttribute('data-state', 'ready');
  await expect(page.locator('#record-best')).toHaveText('No solves yet.');
});
