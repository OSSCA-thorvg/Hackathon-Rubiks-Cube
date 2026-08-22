import { expect, test } from '@playwright/test';

// The two ways the page can come up without a cube. Both have to say so and
// leave no control that looks usable behind.

test('a browser without WebAssembly gets the unsupported page', async ({
  page,
}) => {
  // Feature detection runs before the module is fetched, so hiding the
  // global is enough and nothing is downloaded.
  await page.addInitScript(`
    Object.defineProperty(window, 'WebAssembly', { value: undefined });
  `);
  await page.goto('./');

  await expect(page.locator('#app')).toHaveAttribute(
    'data-state',
    'unsupported',
  );
  await expect(page.locator('#status')).toContainText('does not support');
  await expect(page.locator('.game-stage')).toBeHidden();
  await expect(page.locator('.advanced')).toBeHidden();

  for (const id of ['#scramble', '#reset', '#home-view']) {
    await expect(page.locator(id)).toBeDisabled();
  }
});

test('a failed engine load shows the error page', async ({ page }) => {
  await page.route('**/*.wasm', (route) => route.abort());
  await page.goto('./');

  await expect(page.locator('#app')).toHaveAttribute('data-state', 'error');
  await expect(page.locator('#status')).toContainText('Failed to start');
  await expect(page.locator('.game-stage')).toBeHidden();

  // Startup never installed the gameplay controller, so the buttons stay in
  // the disabled state the markup was rendered with.
  for (const id of ['#scramble', '#reset', '#home-view']) {
    await expect(page.locator(id)).toBeDisabled();
  }
});
