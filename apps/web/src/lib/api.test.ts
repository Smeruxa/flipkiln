import { afterEach, describe, expect, it, vi } from 'vitest';
import { api } from './api';

const request = () => api.publish('workshop', 'development', 2, 'A release note', 'same-operation-key');

afterEach(() => vi.unstubAllGlobals());

describe('release response handling', () => {
    it.each([
        ['unreadable body', () => new Response('{', { status: 200 })],
        ['incomplete snapshot', () => new Response('{}', { status: 200 })],
        ['missing response body', () => new Response(null, { status: 204 })],
        [
            'aborted body',
            () =>
                new Response(
                    new ReadableStream({ start: (controller) => controller.error('aborted') }),
                ),
        ],
    ])('classifies a successful HTTP status with %s as ambiguous', async (_case, response) => {
        vi.stubGlobal('fetch', vi.fn().mockResolvedValue(response()));
        await expect(request()).rejects.toMatchObject({ code: 'invalid_success_response' });
    });

    it('sends the original release payload and key on the retry', async () => {
        const fetch = vi
            .fn()
            .mockResolvedValueOnce(new Response('{', { status: 200 }))
            .mockResolvedValueOnce(Response.json({ revision: 2, entries: [] }));
        vi.stubGlobal('fetch', fetch);
        await expect(request()).rejects.toMatchObject({ code: 'invalid_success_response' });
        await expect(request()).resolves.toEqual({ revision: 2, entries: [] });
        const [first, second] = fetch.mock.calls.map(([, options]) => options as RequestInit);
        expect(second.body).toEqual(first.body);
        expect(new Headers(second.headers).get('Idempotency-Key')).toBe('same-operation-key');
    });
});
