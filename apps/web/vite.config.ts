import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

const control = process.env.FLIPKILN_API_UPSTREAM ?? 'http://127.0.0.1:8080';
const runtime = process.env.FLIPKILN_RUNTIME_UPSTREAM ?? control;

export default defineConfig({
    plugins: [vue()],
    server: {
        proxy: { '/api/client': runtime, '/api': control },
    },
});
