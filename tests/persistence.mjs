import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';

const url = process.env.FLIPKILN_BASE_URL || 'http://127.0.0.1:8080';
const password = process.env.FLIPKILN_ADMIN_PASSWORD || 'flipkiln-local-demo';
const token = process.env.FLIPKILN_DEMO_TOKEN || 'flipkiln-local-demo-read-token';
const manage = '/api/manage/projects/workshop/environments/development';
const client = '/api/client/projects/workshop/environments/development';
const [command, file] = process.argv.slice(2);

async function request(path, options) {
    const response = await fetch(url + path, { ...options, signal: AbortSignal.timeout(10_000) });
    assert(response.ok, `${path} returned HTTP ${response.status}`);
    return response;
}

async function authenticate() {
    const response = await request('/api/auth/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', origin: url },
        body: JSON.stringify({ username: 'admin', password }),
    });
    const cookie = response.headers.getSetCookie()[0]?.split(';')[0];
    assert(cookie?.startsWith('flipkiln_session='), 'Missing session cookie');
    return { cookie };
}

async function inspect(headers) {
    const [state, history, snapshot] = await Promise.all([
        request(manage + '/state', { headers }),
        request(manage + '/releases', { headers }),
        request(client + '/snapshot', { headers: { authorization: `Bearer ${token}` } }),
    ]);
    return {
        state: await state.json(),
        history: await history.json(),
        snapshot: await snapshot.json(),
        etag: snapshot.headers.get('etag'),
    };
}

async function prepare(headers) {
    const { state, history } = await inspect(headers);
    assert(
        history.releases.length >= 3,
        'Run the browser publication and rollback scenario before upgrading',
    );
    const entry = state.entries.find(({ key }) => key === 'checkout.fast_lane');
    assert(entry, 'Missing demo flag');
    entry.description += ' Unpublished edit retained through the upgrade.';
    await request(manage + '/draft', {
        method: 'PUT',
        headers: { ...headers, 'Content-Type': 'application/json', origin: url },
        body: JSON.stringify({ draftVersion: state.draftVersion, entries: state.entries }),
    });
    const before = await inspect(headers);
    assert.notDeepEqual(
        before.state.entries,
        before.snapshot.entries,
        'Fixture needs an unpublished draft',
    );
    assert.equal(before.state.active.revision, before.snapshot.revision);
    assert.deepEqual(before.state.active.entries, before.snapshot.entries);
    assert.match(before.etag, /^".+"$/);
    await writeFile(file, JSON.stringify(before, null, 2) + '\n');
    console.log(
        `Recorded revision ${before.snapshot.revision}, draft and ${before.history.releases.length} records.`,
    );
}

async function compare(headers) {
    const before = JSON.parse(await readFile(file, 'utf8'));
    const after = await inspect(headers);
    assert.deepEqual(
        after,
        before,
        'Active snapshot, ETag, draft and history must survive Kubernetes changes',
    );
    const response = await fetch(url + client + '/snapshot', {
        headers: { authorization: `Bearer ${token}`, 'If-None-Match': before.etag },
        signal: AbortSignal.timeout(10_000),
    });
    assert.equal(response.status, 304);
    console.log(
        `Revision ${before.snapshot.revision}, ETag, draft and history are intact (304 verified).`,
    );
}

if (!file || !['prepare', 'compare'].includes(command)) {
    console.error('Usage: node tests/persistence.mjs <prepare|compare> <state-file>');
    process.exit(2);
}
const headers = await authenticate();
if (command === 'prepare') await prepare(headers);
else await compare(headers);
