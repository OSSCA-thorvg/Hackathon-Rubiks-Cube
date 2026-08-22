import { defineConfig } from '@playwright/test';

/**
 * Browser smoke tests run against the production build served by Vite
 * preview under the Pages base path, so the e2e pass also validates the
 * deployable artifacts. The WASM artifacts must be synchronized by
 * build_wasm.sh beforehand.
 *
 * With SMOKE_BASE_URL set, the same suite runs against an already
 * deployed site instead: no local server is started and the URL (the
 * page_url reported by the Pages deployment) becomes the base.
 */
const smokeBaseUrl = process.env.SMOKE_BASE_URL;

const previewUrl = 'http://127.0.0.1:4173/Hackathon-Rubiks-Cube/';

export default defineConfig({
  testDir: './tests/e2e',
  use: {
    // Trailing slash matters: specs navigate with relative paths like
    // './' so the subpath is preserved.
    baseURL: smokeBaseUrl ?? previewUrl,
    // The rendered-scene contract is written against the dark ground, and
    // with no saved preference the page follows the machine. Saying which
    // machine this is here keeps every pixel assertion in the suite reading
    // the same background; theme.spec.ts overrides it to check the other one.
    colorScheme: 'dark',
  },
  webServer: smokeBaseUrl
    ? undefined
    : {
        command:
          'npm run build && npm run preview -- --host 127.0.0.1 --port 4173 --strictPort',
        url: previewUrl,
        reuseExistingServer: false,
        timeout: 120_000,
      },
});
