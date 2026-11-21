import { describe, expect, it } from 'vitest';
import type { Entry } from './api';
import { diffEntries, editableEntry, serializeDraft } from './editor';

const entry: Entry = {
    key: 'checkout.fast_lane',
    description: 'Express checkout',
    type: 'boolean',
    enabled: true,
    defaultValue: false,
    rules: [{ id: 'shoppers', kind: 'ids', ids: [' customer ', 'two\nlines'], value: true }],
};

describe('working draft serialization', () => {
    it('round trips IDs without altering their whitespace', () => {
        expect(serializeDraft([editableEntry(entry)])).toEqual({ entries: [entry], issues: [] });
    });

    it('keeps invalid input in the editor and reports a JSON Pointer', () => {
        const draft = editableEntry({ ...entry, type: 'json', defaultValue: null, rules: [] });
        draft.valueInput = '{';
        expect(serializeDraft([draft]).issues).toEqual([
            { path: '/entries/0/defaultValue', message: 'Enter valid JSON.' },
        ]);
        expect(draft.valueInput).toBe('{');
    });

    it('rejects unsafe integers with escaped nested keys', () => {
        const draft = editableEntry({ ...entry, type: 'json', defaultValue: null, rules: [] });
        draft.valueInput = '{"a/b":[9007199254740993]}';
        expect(serializeDraft([draft]).issues[0]?.path).toBe('/entries/0/defaultValue/a~1b/0');
    });
});

describe('release diff', () => {
    it('shows typed field values for an entry that changes', () => {
        expect(diffEntries([entry], [{ ...entry, defaultValue: true }])).toEqual([
            {
                key: entry.key,
                kind: 'Changed',
                fields: [{ field: 'defaultValue', label: 'Default value', before: false, after: true }],
            },
        ]);
    });

    it('recognizes removal and preserves the order of rules', () => {
        const variant: Entry = {
            ...entry,
            rules: [...entry.rules, { id: 'slow', kind: 'rollout', percentage: 0, value: true }],
        };
        expect(diffEntries([entry], [])[0]?.kind).toBe('Removed');
        expect(
            diffEntries([variant], [{ ...variant, rules: [...variant.rules].reverse() }])[0]?.fields[0]
                ?.field,
        ).toBe('rules');
    });

    it('ignores property order in JSON', () => {
        const value = { ...entry, type: 'json' as const };
        expect(
            diffEntries(
                [{ ...value, defaultValue: { a: 1, b: 2 } }],
                [{ ...value, defaultValue: { b: 2, a: 1 } }],
            ),
        ).toEqual([]);
    });
});
