import { defineConfig } from '@playwright/test';

export default defineConfig({
    testDir: './browser',
    outputDir: '../test-results/browser-results',
    workers: 1,
    timeout: 45_000,
    expect: { timeout: 8_000 },
    reporter: [
        ['list'],
        [
            'html',
            {
                open: 'never',
                outputFolder:
                    process.env.PLAYWRIGHT_HTML_OUTPUT_DIR || '../test-results/playwright-report',
            },
        ],
    ],
    use: {
        baseURL: process.env.FLIPKILN_BASE_URL || 'http://127.0.0.1:8080',
        viewport: { width: 1440, height: 900 },
        trace: 'retain-on-failure',
        screenshot: 'only-on-failure',
        launchOptions: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE
            ? { executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE }
            : {},
    },
});
