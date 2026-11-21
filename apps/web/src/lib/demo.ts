import type { Entry, EntryType, Environment, JsonValue, RequestContext, Rule, Snapshot } from './api';

export const demoContext: RequestContext = {
    id: 'customer-42',
    attributes: { plan: 'pro', country: 'DE' },
};
export const demoDefaults = {
    express: false,
    headline: 'Your order',
    maxItems: 4,
    button: 'Continue to payment',
    expressLimit: 6,
};

function object(value: unknown): value is Record<string, unknown> {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
}
function isScalar(value: unknown): value is string | number | boolean | null {
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
    if (isScalar(value)) return true;
    if (Array.isArray(value)) return value.every(isJson);
    return object(value) && Object.values(value).every(isJson);
}
function matchesType(type: EntryType, value: unknown): boolean {
    return type === 'json' ? isJson(value) : typeof value === type && isScalar(value);
}
function validRule(rule: unknown, type: EntryType): rule is Rule {
    if (!object(rule) || typeof rule.id !== 'string' || !matchesType(type, rule.value)) return false;
    if (rule.kind === 'ids')
        return Array.isArray(rule.ids) && rule.ids.every((value: unknown) => typeof value === 'string');
    if (rule.kind === 'attribute') return typeof rule.attribute === 'string' && isScalar(rule.equals);
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
        !object(entry) ||
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
        !object(payload) ||
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

export function rolloutBucket(
    project: string,
    environment: Environment,
    key: string,
    rule: string,
    id: string,
): number {
    let hash = 14695981039346656037n;
    for (const byte of new TextEncoder().encode([project, environment, key, rule, id].join('\n'))) {
        hash = BigInt.asUintN(64, (hash ^ BigInt(byte)) * 1099511628211n);
    }
    return Number(hash % 10000n);
}

export function localValue(
    entry: Entry | undefined,
    project: string,
    environment: Environment,
    context: RequestContext,
): JsonValue | undefined {
    if (!entry?.enabled) return entry?.defaultValue;
    for (const rule of entry.rules) {
        if (rule.kind === 'ids' && context.id && rule.ids.includes(context.id)) return rule.value;
        if (
            rule.kind === 'attribute' &&
            context.attributes &&
            Object.hasOwn(context.attributes, rule.attribute) &&
            context.attributes[rule.attribute] === rule.equals
        )
            return rule.value;
        if (
            rule.kind === 'rollout' &&
            context.id &&
            rolloutBucket(project, environment, entry.key, rule.id, context.id) < rule.percentage * 100
        )
            return rule.value;
    }
    return entry.defaultValue;
}

export function checkoutSettings(snapshot: Snapshot | null, project: string, environment: Environment) {
    const value = (key: string) =>
        localValue(
            snapshot?.entries.find((entry) => entry.key === key),
            project,
            environment,
            demoContext,
        );
    const express = value('checkout.fast_lane');
    const copy = value('checkout.copy');
    const button = value('checkout.button_label');
    const expressLimit = value('checkout.item_limit');
    return {
        express: typeof express === 'boolean' ? express : demoDefaults.express,
        headline:
            object(copy) && typeof copy.headline === 'string' && copy.headline.length > 0
                ? copy.headline
                : demoDefaults.headline,
        maxItems:
            object(copy) && typeof copy.maxItems === 'number' && copy.maxItems >= 1
                ? copy.maxItems
                : demoDefaults.maxItems,
        button: typeof button === 'string' && button.length > 0 ? button : demoDefaults.button,
        expressLimit:
            typeof expressLimit === 'number' && expressLimit >= 0
                ? expressLimit
                : demoDefaults.expressLimit,
    };
}

export async function readSnapshot(
    scope: { project: string; environment: Environment; token: string },
    etag: string | null,
    signal: AbortSignal,
) {
    const headers = new Headers({ Authorization: `Bearer ${scope.token}` });
    if (etag) headers.set('If-None-Match', etag);
    const project = encodeURIComponent(scope.project);
    const path = `/api/client/projects/${project}/environments/${scope.environment}/snapshot`;
    const response = await fetch(path, {
        headers,
        credentials: 'omit',
        cache: 'no-store',
        signal,
    });
    if (response.status === 304) return null;
    if (response.status === 401 || response.status === 403 || response.status === 404)
        throw new Error('Check the read token and its project and environment.');
    if (!response.ok) throw new Error('Flipkiln could not return the current release.');
    return { snapshot: decodeSnapshot(await response.json()), etag: response.headers.get('ETag') };
}
