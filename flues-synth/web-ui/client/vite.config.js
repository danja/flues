import { defineConfig } from 'vite';

export default defineConfig({
    root: '.',
    base: './',  // Relative paths for deployment
    build: {
        outDir: 'dist',
        emptyOutDir: true,
        rollupOptions: {
            output: {
                manualChunks: undefined,  // Single bundle
            }
        }
    },
    server: {
        port: 3000,
        proxy: {
            '/ws': {
                target: 'ws://localhost:8081',
                ws: true
            }
        }
    }
});
