import type { Entry, EntryType, Issue, JsonValue, Rule } from './api';
import { operationKey } from './api';

export type WorkingRule = {
    id: string;
    kind: Rule['kind'];
    ids: string;
    attribute: string;
    equals: string;
    percentage: string;
    valueInput: string;
};

export type WorkingEntry = {
    uid: string;
    identityEditable: boolean;
    key: string;
    type: EntryType;
    description: string;
    enabled: boolean;
    valueInput: string;
    schemaInput: string;
    rules: WorkingRule[];
};

export function valueInput(type: EntryType, value: JsonValue): string {
    if (type === 'string') return String(value);
    return JSON.stringify(value, null, 2);
}

export function editableEntry(entry?: Entry): WorkingEntry {
    if (entry === undefined) {
        return {
            uid: operationKey(),
            identityEditable: true,
            key: '',
            type: 'boolean',
            description: '',
            enabled: true,
            valueInput: 'false',
            schemaInput: '',
            rules: [],
        };
    }
    return {
        uid: entry.key,
        identityEditable: false,
        key: entry.key,
        type: entry.type,
        description: entry.description,
        enabled: entry.enabled,
        valueInput: valueInput(entry.type, entry.defaultValue),
        schemaInput: entry.schema === undefined ? '' : JSON.stringify(entry.schema, null, 2),
        rules: entry.rules.map((rule) => ({
            ...editableRule(entry.type, rule.kind, rule.id),
            ids: rule.kind === 'ids' ? JSON.stringify(rule.ids, null, 2) : '',
            attribute: rule.kind === 'attribute' ? rule.attribute : '',
            equals: rule.kind === 'attribute' ? JSON.stringify(rule.equals) : '',
            percentage: rule.kind === 'rollout' ? String(rule.percentage) : '10',
            valueInput: valueInput(entry.type, rule.value),
        })),
    };
}

export function initialValue(type: EntryType): JsonValue {
    const values: Record<EntryType, JsonValue> = { boolean: false, string: '', number: 0, json: {} };
    return values[type];
}

export function editableRule(
    type: EntryType,
    kind: Rule['kind'],
    id = `rule-${operationKey().slice(0, 12)}`,
): WorkingRule {
    return {
        kind,
        id,
        ids: '[]',
        attribute: '',
        equals: '""',
        percentage: '10',
        valueInput: valueInput(type, initialValue(type)),
    };
}

export function serializeDraft(workingEntries: WorkingEntry[]): { entries: Entry[]; issues: Issue[] } {
    const issues: Issue[] = [];
    function checkNumbers(value: JsonValue, path: string) {
        if (
            typeof value === 'number' &&
            (!Number.isFinite(value) || (Number.isInteger(value) && !Number.isSafeInteger(value)))
        ) {
            issues.push({
                path,
                message:
                    'Use a finite number. Integers must fit in the safe range (±9,007,199,254,740,991).',
            });
        } else if (value !== null && typeof value === 'object') {
            for (const [key, child] of Object.entries(value))
                checkNumbers(child, `${path}/${key.replaceAll('~', '~0').replaceAll('/', '~1')}`);
        }
    }
    function parse(text: string, path: string): JsonValue {
        try {
            const value = JSON.parse(text) as JsonValue;
            checkNumbers(value, path);
            return value;
        } catch {
            issues.push({ path, message: 'Enter valid JSON.' });
            return null;
        }
    }
    function parseValue(type: EntryType, text: string, path: string): JsonValue {
        return type === 'string' ? text : parse(text, path);
    }

    const entries = workingEntries.map<Entry>((entry, index) => {
        const path = `/entries/${index}`;
        const draftEntry: Entry = {
            key: entry.key.trim(),
            description: entry.description,
            type: entry.type,
            defaultValue: parseValue(entry.type, entry.valueInput, `${path}/defaultValue`),
            enabled: entry.enabled,
            rules: entry.rules.map<Rule>((rule, ruleIndex) => {
                const rulePath = `${path}/rules/${ruleIndex}`;
                const common = {
                    id: rule.id,
                    value: parseValue(entry.type, rule.valueInput, `${rulePath}/value`),
                };
                if (rule.kind === 'attribute') {
                    const equals = parse(rule.equals, `${rulePath}/equals`);
                    if (typeof equals === 'object' && equals !== null) {
                        issues.push({
                            path: `${rulePath}/equals`,
                            message: 'Use a string, number, boolean, or null.',
                        });
                        return { ...common, kind: rule.kind, attribute: rule.attribute, equals: null };
                    }
                    return { ...common, kind: rule.kind, attribute: rule.attribute, equals };
                }
                if (rule.kind === 'rollout') {
                    const percentage = Number(rule.percentage);
                    if (
                        rule.percentage === '' ||
                        !Number.isInteger(percentage) ||
                        percentage < 0 ||
                        percentage > 100
                    ) {
                        issues.push({
                            path: `${rulePath}/percentage`,
                            message: 'Enter a whole percentage from 0 to 100.',
                        });
                    }
                    return { ...common, kind: rule.kind, percentage };
                }
                const ids = parse(rule.ids, `${rulePath}/ids`);
                if (!Array.isArray(ids) || !ids.every((id) => typeof id === 'string' && id !== '')) {
                    issues.push({
                        path: `${rulePath}/ids`,
                        message: 'Enter a JSON array of nonempty user IDs.',
                    });
                    return { ...common, kind: rule.kind, ids: [] };
                }
                return { ...common, kind: rule.kind, ids };
            }),
        };
        if (entry.type === 'json' && entry.schemaInput.trim() !== '') {
            const schema = parse(entry.schemaInput, `${path}/schema`);
            if (schema !== null && typeof schema === 'object' && !Array.isArray(schema)) {
                draftEntry.schema = schema;
            } else if (!issues.some((issue) => issue.path === `${path}/schema`)) {
                issues.push({ path: `${path}/schema`, message: 'Enter a JSON Schema object.' });
            }
        }
        return draftEntry;
    });
    return { entries, issues };
}

function canonical(value: unknown): unknown {
    if (Array.isArray(value)) return value.map(canonical);
    if (typeof value !== 'object' || value === null) return value;
    return Object.fromEntries(
        Object.entries(value)
            .sort(([a], [b]) => a.localeCompare(b))
            .map(([key, item]) => [key, canonical(item)]),
    );
}

export function equalValues(first: unknown, second: unknown): boolean {
    return JSON.stringify(canonical(first)) === JSON.stringify(canonical(second));
}

export type EntryChange = {
    key: string;
    kind: 'Added' | 'Removed' | 'Changed';
    fields: { field: string; label: string; before: unknown; after: unknown }[];
};

const diffFields = {
    type: 'Type',
    description: 'Description',
    enabled: 'Targeting enabled',
    defaultValue: 'Default value',
    rules: 'Rules',
    schema: 'JSON schema',
} satisfies Partial<Record<keyof Entry, string>>;

export function diffEntries(before: Entry[], after: Entry[]): EntryChange[] {
    const previous = new Map(before.map((entry) => [entry.key, entry]));
    const next = new Map(after.map((entry) => [entry.key, entry]));
    const keys = new Set([...previous.keys(), ...next.keys()]);
    const result: EntryChange[] = [];
    for (const key of keys) {
        const oldEntry = previous.get(key);
        const newEntry = next.get(key);
        const fields = (Object.keys(diffFields) as Array<keyof typeof diffFields>)
            .filter((field) => !equalValues(oldEntry?.[field], newEntry?.[field]))
            .map((field) => ({
                field,
                label: diffFields[field],
                before: oldEntry?.[field],
                after: newEntry?.[field],
            }));
        if (fields.length > 0)
            result.push({
                key,
                kind: oldEntry === undefined ? 'Added' : newEntry === undefined ? 'Removed' : 'Changed',
                fields,
            });
    }
    return result;
}

export function displayValue(value: unknown, pretty = false): string {
    return JSON.stringify(value, null, pretty ? 2 : undefined) ?? '—';
}
