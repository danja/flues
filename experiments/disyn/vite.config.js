import path from 'node:path';
import { defineConfig } from 'vite';

export default defineConfig({
  base: '',
  resolve: {
    alias: {
      '@shared': path.resolve(__dirname, '../shared'),
      '@src': path.resolve(__dirname, 'src')
    }
  },
  server: {
    fs: {
      allow: [path.resolve(__dirname, '..')]
    }
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    sourcemap: true
  }
});

