import { defineConfig } from 'vite'

export default defineConfig({
  base: './',
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    rollupOptions: {
      output: {
        manualChunks: undefined,
        // Ensure AudioWorklet file has correct MIME type
        assetFileNames: (assetInfo) => {
          if (assetInfo.name && assetInfo.name.endsWith('worklet.js')) {
            return 'assets/[name]-[hash].js';
          }
          return 'assets/[name]-[hash][extname]';
        }
      }
    },
    watch: {},
  }
})
