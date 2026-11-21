import type { Entry, EntryType, JsonValue, Rule, Snapshot } from './api';

export function isRecord(value: unknown): value is Record<string, unknown> {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
}
export function isJsonScalar(value: unknown): value is string | number | boolean | null {
    return (
        value === null ||
        typeof value === 'string' ||
        typeof value === 'boolean' ||
        (typeof value === 'number' &&
            Number.isFinite(value) &&
            (!Number.isInteger(value) || Number.isSafeInteger(value)))
    );
}
function isJson(value: unknown): value is JsonValue {
    if (isJsonScalar(value)) return true;
    if (Array.isArray(value)) return value.every(isJson);
    return isRecord(value) && Object.values(value).every(isJson);
}
function matchesType(type: EntryType, value: unknown): boolean {
    return type === 'json' ? isJson(value) : typeof value === type && isJsonScalar(value);
}
function validRule(rule: unknown, type: EntryType): rule is Rule {
    if (!isRecord(rule) || typeof rule.id !== 'string' || !matchesType(type, rule.value)) return false;
    if (rule.kind === 'ids')
        return Array.isArray(rule.ids) && rule.ids.every((value: unknown) => typeof value === 'string');
    if (rule.kind === 'attribute')
        return typeof rule.attribute === 'string' && isJsonScalar(rule.equals);
    return (
        rule.kind === 'rollout' &&
        typeof rule.percentage === 'number' &&
        Number.isInteger(rule.percentage) &&
        rule.percentage >= 0 &&
        rule.percentage <= 100
    );
}
function validEntry(entry: unknown): entry is Entry {
    if (
        !isRecord(entry) ||
        typeof entry.key !== 'string' ||
        typeof entry.description !== 'string' ||
        typeof entry.enabled !== 'boolean'
    )
        return false;
    const type = entry.type;
    return (
        (type === 'boolean' || type === 'string' || type === 'number' || type === 'json') &&
        matchesType(type, entry.defaultValue) &&
        Array.isArray(entry.rules) &&
        entry.rules.every((rule) => validRule(rule, type))
    );
}

export function decodeSnapshot(payload: unknown): Snapshot {
    if (
        !isRecord(payload) ||
        typeof payload.revision !== 'number' ||
        !Number.isSafeInteger(payload.revision) ||
        payload.revision < 1 ||
        !Array.isArray(payload.entries) ||
        !payload.entries.every(validEntry)
    ) {
        throw new Error('The server returned an invalid configuration snapshot.');
    }
    const keys = new Set(payload.entries.map((entry) => entry.key));
    if (keys.size !== payload.entries.length)
        throw new Error('The snapshot contains duplicate configuration keys.');
    return { revision: payload.revision, entries: payload.entries };
}
