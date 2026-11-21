import js from '@eslint/js';
import { defineConfig, globalIgnores } from 'eslint/config';
import vue from 'eslint-plugin-vue';
import tseslint from 'typescript-eslint';

export default defineConfig(
    globalIgnores(['dist/**']),
    js.configs.recommended,
    tseslint.configs.recommended,
    vue.configs['flat/essential'],
    {
        files: ['**/*.{mjs,ts}'],
        rules: { 'max-len': ['error', { code: 110, tabWidth: 4, ignoreUrls: true }] },
    },
    {
        files: ['**/*.vue'],
        languageOptions: { parserOptions: { parser: tseslint.parser } },
        rules: {
            'no-undef': 'off',
            'vue/max-len': ['error', { code: 110, template: 110, tabWidth: 4 }],
        },
    },
);
