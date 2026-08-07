import { defineConfig } from 'vitest/config';

/**
 * Unit tests cover the TypeScript boundary with a fake Emscripten module;
 * the Playwright e2e suite under tests/e2e owns the real WASM connection.
 */
export default defineConfig({
  test: {
    environment: 'jsdom',
    include: ['tests/unit/**/*.test.ts'],
    setupFiles: ['tests/unit/setup.ts'],
  },
});
