import { mkdir, readFile, writeFile } from 'node:fs/promises';
import openapiTS, { astToString } from 'openapi-typescript';

const schema = new URL('./openapi.yaml', import.meta.url);
const outputDirectory = new URL('./generated/', import.meta.url);
const output = new URL('api.d.ts', outputDirectory);
const declarations = await openapiTS(schema);
const contents = astToString(declarations, { formatOptions: { removeComments: true } });

if (process.argv.includes('--check')) {
    const existing = await readFile(output, 'utf8').catch((error) => {
        if (error.code !== 'ENOENT') {
            throw error;
        }
    });
    if (existing !== contents) {
        console.error('Generated API types are stale. Run npm run generate --prefix contracts.');
        process.exitCode = 1;
    }
} else {
    await mkdir(outputDirectory, { recursive: true });
    await writeFile(output, contents, 'utf8');
}
