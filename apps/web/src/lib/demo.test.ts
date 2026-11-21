import { describe, expect, it } from 'vitest';
import type { Entry } from './api';
import { checkoutSettings, decodeSnapshot, demoDefaults, localValue, rolloutBucket } from './demo';

const entry: Entry = {
    key: 'checkout.fast_lane',
    description: 'Express checkout',
    type: 'boolean',
    enabled: true,
    defaultValue: false,
    rules: [{ id: 'rollout', kind: 'rollout', percentage: 100, value: true }],
};

describe('demo snapshot', () => {
    it('uses local defaults until a checked snapshot arrives', () => {
        expect(checkoutSettings(null, 'workshop', 'development')).toEqual(demoDefaults);
        expect(
            checkoutSettings(
                decodeSnapshot({ revision: 2, entries: [entry] }),
                'workshop',
                'development',
            ).express,
        ).toBe(true);
    });

    it('rejects a malformed or duplicated snapshot instead of caching it', () => {
        expect(() => decodeSnapshot({ revision: 0, entries: [] })).toThrow();
        expect(() => decodeSnapshot({ revision: 1, entries: [entry, entry] })).toThrow();
        expect(() =>
            decodeSnapshot({ revision: 1, entries: [{ ...entry, defaultValue: 'false' }] }),
        ).toThrow();
    });

    it('applies the kill switch and skips rollout for requests without an ID', () => {
        expect(localValue(entry, 'workshop', 'development', {})).toBe(false);
        expect(localValue({ ...entry, enabled: false }, 'workshop', 'development', { id: 'one' })).toBe(
            false,
        );
    });

    it('keeps the same FNV-1a bucket across releases and namespaces the cohort by environment', () => {
        const bucket = rolloutBucket('workshop', 'development', entry.key, 'rollout', 'customer-42');
        expect(bucket).toBe(
            rolloutBucket('workshop', 'development', entry.key, 'rollout', 'customer-42'),
        );
        expect(bucket).not.toBe(
            rolloutBucket('workshop', 'production', entry.key, 'rollout', 'customer-42'),
        );
    });
});
