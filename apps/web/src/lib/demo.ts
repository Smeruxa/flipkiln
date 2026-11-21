import type { Entry, Environment, JsonValue, RequestContext, Snapshot } from './api';
import { decodeSnapshot, isRecord } from './snapshot';

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
            isRecord(copy) && typeof copy.headline === 'string' && copy.headline.length > 0
                ? copy.headline
                : demoDefaults.headline,
        maxItems:
            isRecord(copy) && typeof copy.maxItems === 'number' && copy.maxItems >= 1
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
