import type { components } from '../../../../contracts/generated/api';

export type { components };

export type Entry = components['schemas']['Entry'];
export type EntryType = components['schemas']['EntryType'];
export type Rule = components['schemas']['Rule'];
export type Issue = components['schemas']['Issue'];
export type DraftState = components['schemas']['DraftState'];
export type Snapshot = components['schemas']['Snapshot'];
export type Project = components['schemas']['Project'];
export type Role = components['schemas']['Role'];
export type Environment = components['schemas']['Environment'];
export type RequestContext = components['schemas']['RequestContext'];
export type EvaluationResult = components['schemas']['EvaluationResult'];
export type Release = components['schemas']['Release'];
export type ReleaseDetail = components['schemas']['ReleaseDetail'];
export type ClientToken = components['schemas']['ClientToken'];
export type IssuedToken = components['schemas']['IssuedToken'];
export type Member = components['schemas']['Member'];
export type JsonValue = components['schemas']['JsonValue'];

type Session = { username: string };
type Validation = { valid: boolean; issues: Issue[] };
type Credentials = { username: string; password: string };

export class ApiError extends Error {
    constructor(
        readonly status: number,
        readonly code: string,
        message: string,
        readonly issues: Issue[] = [],
    ) {
        super(message);
        this.name = 'ApiError';
    }
}

type Options = { method?: string; body?: object; headers?: HeadersInit };
async function request<T>(path: string, { method = 'GET', body, headers }: Options = {}): Promise<T> {
    const requestHeaders = new Headers(headers);
    if (body !== undefined) requestHeaders.set('Content-Type', 'application/json');

    let response: Response;
    try {
        response = await fetch(`/api${path}`, {
            method,
            headers: requestHeaders,
            body: body === undefined ? undefined : JSON.stringify(body),
            cache: 'no-store',
            credentials: 'same-origin',
            signal: AbortSignal.timeout(12000),
        });
    } catch {
        throw new ApiError(
            0,
            'connection_failed',
            'Flipkiln is temporarily unreachable. Try again in a moment.',
        );
    }

    if (response.status === 204) return undefined as T;
    const payload = await response.json().catch(() => null);
    if (response.ok && payload !== null) return payload as T;
    if (response.status === 401 && path !== '/auth/login' && path !== '/auth/session') {
        window.dispatchEvent(new Event('flipkiln-session-expired'));
    }
    const error = payload?.error;
    throw new ApiError(
        response.status,
        error?.code ?? 'request_failed',
        error?.message ??
            (response.status >= 500
                ? 'The server is temporarily unavailable. Try again in a moment.'
                : 'This request could not be completed.'),
        error?.issues ?? [],
    );
}

const projectPath = (project: string) => `/manage/projects/${encodeURIComponent(project)}`;
const envPath = (project: string, environment: string) =>
    `${projectPath(project)}/environments/${encodeURIComponent(environment)}`;
const segment = encodeURIComponent;

export const api = {
    session: () => request<Session>('/auth/session'),
    login: (body: Credentials) => request<Session>('/auth/login', { method: 'POST', body }),
    logout: () => request<void>('/auth/logout', { method: 'POST', body: {} }),
    projects: () => request<{ projects: Project[] }>('/manage/projects'),
    createProject: (body: { id: string; name: string }) =>
        request<Project>('/manage/projects', { method: 'POST', body }),
    members: (project: string) => request<{ members: Member[] }>(`${projectPath(project)}/members`),
    setMember: (project: string, username: string, role: Role | null) =>
        request<Member>(`${projectPath(project)}/members/${segment(username)}`, {
            method: 'PUT',
            body: { role },
        }),
    state: (project: string, env: Environment) => request<DraftState>(`${envPath(project, env)}/state`),
    saveDraft: (project: string, env: Environment, draftVersion: number, entries: Entry[]) =>
        request<DraftState>(`${envPath(project, env)}/draft`, {
            method: 'PUT',
            body: { draftVersion, entries },
        }),
    validate: (project: string, env: Environment, entries: Entry[]) =>
        request<Validation>(`${envPath(project, env)}/validate`, { method: 'POST', body: { entries } }),
    preview: (
        project: string,
        env: Environment,
        entries: Entry[],
        keys: string[],
        context: RequestContext,
    ) =>
        request<{ results: EvaluationResult[] }>(`${envPath(project, env)}/preview`, {
            method: 'POST',
            body: { entries, keys, context },
        }),
    releases: (project: string, env: Environment) =>
        request<{ releases: Release[] }>(`${envPath(project, env)}/releases`),
    release: (project: string, env: Environment, revision: number) =>
        request<ReleaseDetail>(`${envPath(project, env)}/releases/${revision}`),
    publish: (project: string, env: Environment, draftVersion: number, note: string, key: string) =>
        request<Snapshot>(`${envPath(project, env)}/publish`, {
            method: 'POST',
            headers: { 'Idempotency-Key': key },
            body: { draftVersion, note },
        }),
    rollback: (
        project: string,
        env: Environment,
        revision: number,
        draftVersion: number,
        note: string,
        key: string,
    ) =>
        request<Snapshot>(`${envPath(project, env)}/rollback`, {
            method: 'POST',
            headers: { 'Idempotency-Key': key },
            body: { revision, draftVersion, note },
        }),
    tokens: (project: string, env: Environment) =>
        request<{ tokens: ClientToken[] }>(`${envPath(project, env)}/tokens`),
    createToken: (project: string, env: Environment, name: string) =>
        request<IssuedToken>(`${envPath(project, env)}/tokens`, { method: 'POST', body: { name } }),
    revokeToken: (project: string, env: Environment, id: ClientToken['id']) =>
        request<void>(`${envPath(project, env)}/tokens/${segment(String(id))}`, { method: 'DELETE' }),
};

export const roleRank: Record<Role, number> = { viewer: 0, editor: 1, publisher: 2, admin: 3 };
export const environmentLabel: Record<Environment, string> = {
    development: 'Development',
    production: 'Production',
};
export const roles = ['viewer', 'editor', 'publisher', 'admin'] as const;
export const environments = ['development', 'production'] as const;
export const typeLabel: Record<EntryType, string> = {
    boolean: 'Boolean',
    string: 'String',
    number: 'Number',
    json: 'JSON',
};

export const authChangeKey = 'flipkiln-auth-change';
export function notifyAuthChange() {
    try {
        localStorage.setItem(authChangeKey, operationKey());
    } catch {
        return;
    }
}

export function errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : 'Something went wrong. Try again.';
}

export function dateLabel(value: string): string {
    return new Intl.DateTimeFormat(undefined, { dateStyle: 'medium', timeStyle: 'short' }).format(
        new Date(value),
    );
}

export function operationKey(): string {
    const bytes = crypto.getRandomValues(new Uint8Array(24));
    return Array.from(bytes, (value) => value.toString(16).padStart(2, '0')).join('');
}
