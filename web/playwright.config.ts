import { defineConfig } from '@playwright/test';

/**
 * Browser smoke tests run against the production build served by Vite
 * preview, so the e2e pass also validates the deployable artifacts.
 * The WASM artifacts must be synchronized by build_wasm.sh beforehand.
 */
export default defineConfig({
  testDir: './tests/e2e',
  use: {
    baseURL: 'http://127.0.0.1:4173',
  },
  webServer: {
    command:
      'npm run build && npm run preview -- --host 127.0.0.1 --port 4173 --strictPort',
    url: 'http://127.0.0.1:4173',
    reuseExistingServer: false,
    timeout: 120_000,
  },
});
