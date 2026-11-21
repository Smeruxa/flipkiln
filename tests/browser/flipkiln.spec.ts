import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect, test, type Locator, type Page } from '@playwright/test';

const environmentPath = '/projects/workshop/development';
const entriesPath = environmentPath + '/entries';
const historyPath = environmentPath + '/releases';
const managePath = '/api/manage/projects/workshop/environments/development';
const snapshotPath = '/api/client/projects/workshop/environments/development/snapshot';
const demoPassword = 'flipkiln-local-demo';
const demoToken = 'flipkiln-local-demo-read-token';
const toggleKey = 'checkout.fast_lane';

async function signIn(page: Page, username = 'admin') {
    await page.goto('/login');
    await page.getByLabel('Username', { exact: true }).fill(username);
    await page.getByLabel('Password', { exact: true }).fill(demoPassword);
    await page.getByRole('button', { name: 'Sign in', exact: true }).click();
    await expect(page).not.toHaveURL(/\/login(?:[?#]|$)/);
}

async function openEntry(page: Page, name: string) {
    await page.getByRole('button', { name, exact: true }).click();
}

async function shot(page: Page, name: string) {
    if (!process.env.FLIPKILN_SCREENSHOTS) return;
    const directory = resolve(process.env.FLIPKILN_SCREENSHOTS);
    await mkdir(directory, { recursive: true });
    await page.evaluate(() => window.scrollTo({ top: 0, behavior: 'instant' }));
    await page.screenshot({ path: resolve(directory, name), animations: 'disabled' });
}

async function fitViewport(page: Page) {
    const overflow = await page.evaluate(
        () => document.documentElement.scrollWidth - window.innerWidth,
    );
    expect(overflow).toBeLessThanOrEqual(1);
}

async function activateWithKeyboard(locator: Locator) {
    await expect(locator).toBeEnabled();
    await locator.focus();
    await expect(locator).toBeFocused();
    await locator.press('Enter');
}

test('keyboard edit, diff, publish, client reads, and rollback', async ({ page, context }) => {
    await signIn(page);
    await page.goto(entriesPath);
    await openEntry(page, toggleKey);
    await fitViewport(page);
    const baseline = await page.request.get(snapshotPath, {
        headers: { authorization: `Bearer ${demoToken}` },
    });
    expect(baseline.ok()).toBeTruthy();
    const { revision, entries } = await baseline.json();
    const current = entries.find((entry: { key: string }) => entry.key === toggleKey)?.defaultValue;
    expect(typeof current).toBe('boolean');
    const value = page.getByRole('switch', { name: 'Default value', exact: true });
    await expect(value).toBeVisible();
    await shot(page, 'console-1440.png');

    const demo = await context.newPage();
    await demo.goto('/demo');
    await demo.getByLabel('Read token', { exact: true }).fill(demoToken);
    await demo.getByRole('button', { name: 'Connect demo', exact: true }).click();
    await expect(demo.getByText('Published configuration', { exact: true })).toBeVisible();
    const checkout = (express: boolean) =>
        demo.getByText(express ? 'Express checkout' : 'Standard checkout', { exact: true });
    await expect(checkout(current)).toBeVisible();

    if ((await value.getAttribute('aria-checked')) === String(current)) {
        await value.focus();
        await value.press('Space');
        await expect(value).toHaveAttribute('aria-checked', String(!current));
        const saveDraft = page.getByRole('button', { name: 'Save draft', exact: true });
        await activateWithKeyboard(saveDraft);
        await expect(saveDraft).toBeDisabled();
    }
    await expect(checkout(current)).toBeVisible();
    const draftSnapshot = await page.request.get(snapshotPath, {
        headers: { authorization: `Bearer ${demoToken}`, 'If-None-Match': baseline.headers().etag },
    });
    expect(draftSnapshot.status()).toBe(304);

    await activateWithKeyboard(page.getByRole('button', { name: 'Review changes', exact: true }));
    const publish = page.getByRole('dialog');
    await expect(publish).toBeVisible();
    for (let step = 0; step < 6; step++) {
        await page.keyboard.press('Tab');
        await expect(publish.locator(':focus')).toHaveCount(1);
    }
    await expect(publish.getByText(toggleKey, { exact: true })).toBeVisible();
    await expect(publish.getByText(/development/i).first()).toBeVisible();
    await expect(publish.getByText(String(!current), { exact: true }).first()).toBeVisible();
    await shot(page, 'publication-diff-1440.png');
    await activateWithKeyboard(publish.getByRole('button', { name: 'Publish release', exact: true }));
    await expect(publish).toBeHidden();
    await expect(checkout(!current)).toBeVisible({ timeout: 15_000 });
    await shot(demo, 'client-demo-1440.png');

    await page.goto(historyPath);
    await page.getByRole('button', { name: `Revision ${revision}`, exact: true }).click();
    await page.getByRole('button', { name: 'Review rollback', exact: true }).click();
    const rollback = page.getByRole('dialog');
    await expect(rollback.getByText(/replace.*draft|draft.*replace/i)).toBeVisible();
    await activateWithKeyboard(
        rollback.getByRole('button', { name: 'Roll back release', exact: true }),
    );
    await expect(rollback).toBeHidden();
    await expect(checkout(current)).toBeVisible({ timeout: 15_000 });
    const restored = await page.request.get(snapshotPath, {
        headers: { authorization: `Bearer ${demoToken}` },
    });
    expect(restored.ok()).toBeTruthy();
    expect(await restored.json()).toEqual({ revision: revision + 2, entries });
    await demo.close();
});

test('responsive editor and JSON schema errors show the field path', async ({ page }) => {
    await signIn(page);
    await page.setViewportSize({ width: 1024, height: 768 });
    await page.goto(entriesPath);
    await openEntry(page, 'checkout.copy');
    await fitViewport(page);
    const input = page.getByLabel('Default value', { exact: true });
    await input.fill('{invalid json');
    await page.getByRole('button', { name: 'Validate', exact: true }).click();
    await expect(page.getByText(/invalid JSON|valid JSON|JSON syntax/i).first()).toBeVisible();
    await input.fill('{"headline":"Your order","maxItems":0}');
    await page.getByRole('button', { name: 'Validate', exact: true }).click();
    await expect(page.getByText(/\/defaultValue\/maxItems/).first()).toBeVisible();
    await shot(page, 'schema-error-1024.png');

    await page.setViewportSize({ width: 390, height: 844 });
    page.once('dialog', (guard) => guard.accept());
    await page.goto('/projects/workshop/production/entries');
    await expect(page.getByText(/production/i).first()).toBeVisible();
    await fitViewport(page);
    await shot(page, 'production-390.png');
    await page.goto(entriesPath);
    await openEntry(page, 'checkout.copy');
    await expect(page.getByLabel('Default value', { exact: true })).toBeVisible();
    await fitViewport(page);
    await shot(page, 'editor-390.png');
});

test('a stale editor receives a conflict and cannot overwrite a saved draft', async ({
    page,
    context,
    baseURL,
}) => {
    await signIn(page);
    const backup = await (await page.request.get(managePath + '/state')).json();
    const other = await context.newPage();
    await Promise.all([page.goto(entriesPath), other.goto(entriesPath)]);
    await openEntry(page, toggleKey);
    await openEntry(other, toggleKey);
    try {
        const description = (target: Page) => target.getByLabel('Description', { exact: true });
        await description(page).fill('Editor one kept this change.');
        await description(other).fill('Editor two was working on an older version.');
        const save = (target: Page) =>
            target.getByRole('button', { name: 'Save draft', exact: true }).click();
        const first = page.waitForResponse(
            (result) => result.url().endsWith('/draft') && result.request().method() === 'PUT',
        );
        await save(page);
        expect((await first).status()).toBe(200);
        const second = other.waitForResponse(
            (result) => result.url().endsWith('/draft') && result.request().method() === 'PUT',
        );
        await save(other);
        expect((await second).status()).toBe(409);
        await expect(other.getByRole('button', { name: 'Load latest draft' })).toBeVisible();
        await shot(other, 'draft-conflict-1440.png');
        const after = await (await page.request.get(managePath + '/state')).json();
        expect(
            after.entries.find((entry: { key: string }) => entry.key === toggleKey)?.description,
        ).toBe('Editor one kept this change.');
        const restore = await page.request.put(managePath + '/draft', {
            data: { draftVersion: after.draftVersion, entries: backup.entries },
            headers: { origin: baseURL! },
        });
        expect(restore.ok()).toBeTruthy();
    } finally {
        await other.close();
    }
});

test('empty project and read-only access have explicit UI states', async ({
    page,
    browser,
    baseURL,
}) => {
    await signIn(page);
    const projectId = `empty-${Date.now().toString(36)}`;
    const response = await page.request.post('/api/manage/projects', {
        data: { id: projectId, name: 'Empty QA workspace' },
        headers: { origin: baseURL! },
    });
    expect(response.ok()).toBeTruthy();
    await page.goto(`/projects/${projectId}/development/entries`);
    await expect(page.getByText(/no configuration entries/i).first()).toBeVisible();
    await page.getByRole('button', { name: 'New entry', exact: true }).click();
    await page.getByLabel('Key', { exact: true }).fill('pilot.enabled');
    await expect(page.getByRole('switch', { name: 'Default value', exact: true })).toBeVisible();
    await page.getByRole('button', { name: 'Save draft', exact: true }).click();
    await expect(page.getByText('1 saved change', { exact: true })).toBeVisible();
    const draft = await page.request.get(
        `/api/manage/projects/${projectId}/environments/development/state`,
    );
    expect(draft.ok()).toBeTruthy();
    const result = await draft.json();
    expect(result.entries).toMatchObject([
        { key: 'pilot.enabled', type: 'boolean', defaultValue: false },
    ]);
    expect(result.active.entries).toEqual([]);
    const reader = await browser.newContext({ baseURL });
    try {
        const readerPage = await reader.newPage();
        await signIn(readerPage, 'viewer');
        await readerPage.goto(entriesPath);
        await openEntry(readerPage, toggleKey);
        const saveControl = readerPage.getByRole('button', { name: 'Save draft', exact: true });
        if (await saveControl.count()) await expect(saveControl).toBeDisabled();
        const visibleFlag = await readerPage.request.get(managePath + '/state');
        expect(visibleFlag.ok()).toBeTruthy();
        const state = await visibleFlag.json();
        const write = await readerPage.request.put(managePath + '/draft', {
            data: { draftVersion: state.draftVersion, entries: state.entries },
            headers: { origin: baseURL! },
        });
        expect(write.status()).toBe(403);
    } finally {
        await reader.close();
    }
});

test('polling keeps the last valid snapshot when the demo loses connectivity', async ({ page }) => {
    await page.goto('/demo');
    await expect(page.getByText('Using local defaults', { exact: true })).toBeVisible();
    await page.getByLabel('Read token', { exact: true }).fill(demoToken);
    await page.getByRole('button', { name: 'Connect demo', exact: true }).click();
    await expect(page.getByText('Published configuration', { exact: true })).toBeVisible();
    const checkout = page
        .getByText('Standard checkout', { exact: true })
        .or(page.getByText('Express checkout', { exact: true }));
    await expect(checkout).toBeVisible();
    const before = await checkout.textContent();
    await page.route(`**${snapshotPath}`, (route) => route.abort('connectionrefused'));
    await page.waitForEvent('requestfailed', {
        predicate: (request) => request.url().endsWith(snapshotPath),
        timeout: 15_000,
    });
    await expect(checkout).toHaveText(before!);
    await expect(page.getByText('Using last published release', { exact: true })).toBeVisible();
    await page.unroute(`**${snapshotPath}`);
    const recovered = await page.waitForResponse((response) => response.url().endsWith(snapshotPath), {
        timeout: 15_000,
    });
    expect([200, 304]).toContain(recovered.status());
    await expect(page.getByText('Published configuration', { exact: true })).toBeVisible();
    await expect(checkout).toHaveText(before!);
});

test('the management console can retry after an interrupted API request', async ({ page }) => {
    await signIn(page);
    const stateRequest = `**${managePath}/state`;
    await page.route(stateRequest, (route) => route.abort('connectionrefused'));
    await page.goto(entriesPath);
    const retry = page.getByRole('button', { name: 'Try again', exact: true });
    await expect(retry).toBeVisible();
    await page.unroute(stateRequest);
    await retry.click();
    await expect(page.getByRole('button', { name: toggleKey, exact: true })).toBeVisible();
});

test('publication retry retains its request after a successful response loses its body', async ({
    page,
    baseURL,
}) => {
    await signIn(page);
    const project = `retry-${Date.now().toString(36)}`;
    const origin = { origin: baseURL! };
    const created = await page.request.post('/api/manage/projects', {
        headers: origin,
        data: { id: project, name: 'Publication retry QA' },
    });
    expect(created.ok()).toBeTruthy();
    const environment = `/api/manage/projects/${project}/environments/development`;
    const flag = {
        key: 'pilot.enabled',
        description: 'Flag for a retried publication.',
        type: 'boolean',
        defaultValue: false,
        enabled: true,
        rules: [],
    };
    const saved = await page.request.put(`${environment}/draft`, {
        headers: origin,
        data: { draftVersion: 1, entries: [flag] },
    });
    expect(saved.ok()).toBeTruthy();
    const attempts: { key?: string; body: unknown; status: number }[] = [];
    await page.route(`**${environment}/publish`, async (route) => {
        const request = route.request();
        const response = await route.fetch();
        attempts.push({
            key: request.headers()['idempotency-key'],
            body: request.postDataJSON(),
            status: response.status(),
        });
        if (attempts.length === 1 && response.ok()) {
            await route.fulfill({
                status: response.status(),
                contentType: 'application/json',
                body: '{',
            });
        } else {
            await route.fulfill({ response });
        }
    });

    await page.goto(`/projects/${project}/development/entries`);
    await page.getByRole('button', { name: 'Review changes', exact: true }).click();
    const review = page.getByRole('dialog');
    const note = review.getByLabel('Release note (optional)', { exact: true });
    const publish = review.getByRole('button', { name: 'Publish release', exact: true });
    await note.fill('Retain the original release note.');
    await publish.click();
    await expect(review.getByText(/could not confirm the result/i)).toBeVisible();
    await expect(note).toHaveValue('Retain the original release note.');
    await expect(note).not.toBeEditable();
    expect(attempts[0]?.key).toBeTruthy();
    expect(attempts[0]?.status).toBe(200);

    const committed = await page.request.get(`${environment}/state`);
    expect(committed.ok()).toBeTruthy();
    const earlier = await committed.json();
    expect(earlier.active.revision).toBe(2);
    const concurrentDraft = await page.request.put(`${environment}/draft`, {
        headers: origin,
        data: { draftVersion: earlier.draftVersion, entries: [{ ...flag, defaultValue: true }] },
    });
    expect(concurrentDraft.ok()).toBeTruthy();
    const concurrentPublish = await page.request.post(`${environment}/publish`, {
        headers: { ...origin, 'Idempotency-Key': `another-${project}` },
        data: { draftVersion: (await concurrentDraft.json()).draftVersion, note: 'Later release' },
    });
    expect(concurrentPublish.ok()).toBeTruthy();
    expect((await concurrentPublish.json()).revision).toBe(3);

    const stateRequest = `**${environment}/state`;
    await page.route(stateRequest, (route) => route.abort('connectionrefused'));
    await publish.click();
    await expect(review).toBeHidden();
    expect(attempts).toHaveLength(2);
    expect(attempts[1]).toEqual(attempts[0]);
    await expect(page.getByText('Published revision 2 to Development.', { exact: true })).toBeVisible();
    const status = page.locator('[aria-label="Environment status"]');
    await expect(status.getByText('Revision 2', { exact: true })).toHaveCount(0);
    const refresh = page.getByRole('button', { name: 'Try again', exact: true });
    await expect(refresh).toBeVisible();
    await page.unroute(stateRequest);
    await refresh.click();
    await expect(status.getByText('Revision 3', { exact: true })).toBeVisible();

    const history = await page.request.get(`${environment}/releases`);
    expect(history.ok()).toBeTruthy();
    expect((await history.json()).releases.map((item: { revision: number }) => item.revision)).toEqual([
        3, 2, 1,
    ]);
});
