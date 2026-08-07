import { defineConfig, type UserConfig } from 'vite';

/**
 * The production site lives under the repository Pages subpath. `vite
 * preview` resolves this config with command 'serve', so branching on the
 * command alone would serve the preview at '/' while the built HTML
 * references subpath assets, breaking the preview-based e2e suite. Preview
 * verifies production output, so it shares the production base.
 */
export default defineConfig(({ command, isPreview }): UserConfig => ({
  base: command === 'build' || isPreview ? '/Hackathon-Rubiks-Cube/' : '/',
}));
