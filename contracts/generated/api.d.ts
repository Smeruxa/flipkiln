export interface paths {
    "/health/live": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get: operations["liveness"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/health/ready": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get: operations["readiness"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/auth/login": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["login"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/auth/session": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get: operations["session"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/auth/logout": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["logout"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects": {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        get: operations["listProjects"];
        put?: never;
        post: operations["createProject"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/members": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
            };
            cookie?: never;
        };
        get: operations["listMembers"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/members/{username}": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                username: string;
            };
            cookie?: never;
        };
        get?: never;
        put: operations["updateMember"];
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/state": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get: operations["getState"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/draft": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put: operations["saveDraft"];
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/validate": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["validateDraft"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/preview": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["previewDraft"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/releases": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get: operations["listReleases"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/releases/{revision}": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
                revision: components["schemas"]["Version"];
            };
            cookie?: never;
        };
        get: operations["getRelease"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/publish": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["publishDraft"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/rollback": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["rollbackRelease"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/tokens": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get: operations["listTokens"];
        put?: never;
        post: operations["createToken"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/manage/projects/{project}/environments/{environment}/tokens/{id}": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
                id: string;
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post?: never;
        delete: operations["revokeToken"];
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/client/projects/{project}/environments/{environment}/snapshot": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get: operations["readSnapshot"];
        put?: never;
        post?: never;
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
    "/api/client/projects/{project}/environments/{environment}/evaluate": {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        get?: never;
        put?: never;
        post: operations["evaluateFlags"];
        delete?: never;
        options?: never;
        head?: never;
        patch?: never;
        trace?: never;
    };
}
export type webhooks = Record<string, never>;
export interface components {
    schemas: {
        Session: {
            username: string;
        };
        Environment: "development" | "production";
        Role: "viewer" | "editor" | "publisher" | "admin";
        NullableRole: components["schemas"]["Role"] | null;
        Project: {
            id: string;
            name: string;
            role: components["schemas"]["Role"];
            environments: components["schemas"]["Environment"][];
        };
        Member: {
            username: string;
            role: components["schemas"]["NullableRole"];
        };
        JsonValue: boolean | string | number | null | components["schemas"]["JsonValue"][] | {
            [key: string]: components["schemas"]["JsonValue"];
        };
        AttributeValue: string | number | boolean | null;
        EntryType: "boolean" | "string" | "number" | "json";
        Rule: components["schemas"]["IdsRule"] | components["schemas"]["AttributeRule"] | components["schemas"]["RolloutRule"];
        IdsRule: {
            id: string;
            kind: "ids";
            ids: string[];
            value: components["schemas"]["JsonValue"];
        };
        AttributeRule: {
            id: string;
            kind: "attribute";
            attribute: string;
            equals: components["schemas"]["AttributeValue"];
            value: components["schemas"]["JsonValue"];
        };
        RolloutRule: {
            id: string;
            kind: "rollout";
            percentage: number;
            value: components["schemas"]["JsonValue"];
        };
        Entry: {
            key: string;
            description: string;
            type: components["schemas"]["EntryType"];
            enabled: boolean;
            defaultValue: components["schemas"]["JsonValue"];
            rules: components["schemas"]["Rule"][];
            schema?: {
                [key: string]: components["schemas"]["JsonValue"];
            };
        };
        EntryList: {
            entries: components["schemas"]["Entry"][];
        };
        Version: number;
        Snapshot: components["schemas"]["EntryList"] & {
            revision: components["schemas"]["Version"];
        };
        DraftUpdate: {
            draftVersion: components["schemas"]["Version"];
        };
        DraftState: components["schemas"]["EntryList"] & components["schemas"]["DraftUpdate"] & {
            active: components["schemas"]["Snapshot"];
        };
        PublishRequest: components["schemas"]["DraftUpdate"] & {
            note: string;
        };
        Release: {
            revision: components["schemas"]["Version"];
            kind: "initial" | "publish" | "rollback";
            sourceRevision: components["schemas"]["Version"] | null;
            createdAt: string;
            actor: string;
            note: string;
        };
        ReleaseDetail: components["schemas"]["EntryList"] & components["schemas"]["Release"];
        Issue: {
            path: string;
            message: string;
        };
        Validation: {
            valid: boolean;
            issues: components["schemas"]["Issue"][];
        };
        ErrorResponse: {
            error: {
                code: string;
                message: string;
                issues?: components["schemas"]["Issue"][];
            };
        };
        RequestContext: {
            id?: string;
            attributes?: {
                [key: string]: components["schemas"]["AttributeValue"];
            };
        };
        EvaluationRequest: {
            keys: string[];
            context: components["schemas"]["RequestContext"];
            expectedTypes?: {
                [key: string]: components["schemas"]["EntryType"];
            };
        };
        EvaluationResult: {
            key: string;
            reason: "disabled" | "rule_match" | "default" | "unknown_key" | "type_mismatch";
            value?: components["schemas"]["JsonValue"];
            ruleId?: string;
        };
        EvaluationResults: {
            results: components["schemas"]["EvaluationResult"][];
        };
        ClientToken: {
            id: string;
            name: string;
            createdAt: string;
            revokedAt: string | null;
        };
        IssuedToken: components["schemas"]["ClientToken"] & {
            token: string;
        };
    };
    responses: {
        Error: {
            headers: {
                [name: string]: unknown;
            };
            content: {
                "application/json": components["schemas"]["ErrorResponse"];
            };
        };
        Snapshot: {
            headers: {
                [name: string]: unknown;
            };
            content: {
                "application/json": components["schemas"]["Snapshot"];
            };
        };
        State: {
            headers: {
                [name: string]: unknown;
            };
            content: {
                "application/json": components["schemas"]["DraftState"];
            };
        };
    };
    parameters: {
        Project: string;
        Environment: components["schemas"]["Environment"];
        IdempotencyKey: string;
    };
    requestBodies: never;
    headers: {
        SnapshotETag: string;
    };
    pathItems: never;
}
export type $defs = Record<string, never>;
export interface operations {
    liveness: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content?: never;
            };
        };
    };
    readiness: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content?: never;
            };
            503: components["responses"]["Error"];
        };
    };
    login: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": {
                    username: string;
                    password: string;
                };
            };
        };
        responses: {
            200: {
                headers: {
                    "Set-Cookie"?: string;
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Session"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    session: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Session"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    logout: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": Record<string, never>;
            };
        };
        responses: {
            204: {
                headers: {
                    [name: string]: unknown;
                };
                content?: never;
            };
            default: components["responses"]["Error"];
        };
    };
    listProjects: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": {
                        projects: components["schemas"]["Project"][];
                    };
                };
            };
            default: components["responses"]["Error"];
        };
    };
    createProject: {
        parameters: {
            query?: never;
            header?: never;
            path?: never;
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": {
                    id: string;
                    name: string;
                };
            };
        };
        responses: {
            201: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Project"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    listMembers: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": {
                        members: components["schemas"]["Member"][];
                    };
                };
            };
            default: components["responses"]["Error"];
        };
    };
    updateMember: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                username: string;
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": {
                    role: components["schemas"]["NullableRole"];
                };
            };
        };
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Member"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    getState: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: components["responses"]["State"];
            default: components["responses"]["Error"];
        };
    };
    saveDraft: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["DraftUpdate"] & components["schemas"]["EntryList"];
            };
        };
        responses: {
            200: components["responses"]["State"];
            default: components["responses"]["Error"];
        };
    };
    validateDraft: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["EntryList"];
            };
        };
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Validation"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    previewDraft: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["EntryList"] & components["schemas"]["EvaluationRequest"];
            };
        };
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["EvaluationResults"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    listReleases: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": {
                        releases: components["schemas"]["Release"][];
                    };
                };
            };
            default: components["responses"]["Error"];
        };
    };
    getRelease: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
                revision: components["schemas"]["Version"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["ReleaseDetail"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    publishDraft: {
        parameters: {
            query?: never;
            header: {
                "Idempotency-Key": components["parameters"]["IdempotencyKey"];
            };
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["PublishRequest"];
            };
        };
        responses: {
            200: components["responses"]["Snapshot"];
            default: components["responses"]["Error"];
        };
    };
    rollbackRelease: {
        parameters: {
            query?: never;
            header: {
                "Idempotency-Key": components["parameters"]["IdempotencyKey"];
            };
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["PublishRequest"] & {
                    revision: components["schemas"]["Version"];
                };
            };
        };
        responses: {
            200: components["responses"]["Snapshot"];
            default: components["responses"]["Error"];
        };
    };
    listTokens: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": {
                        tokens: components["schemas"]["ClientToken"][];
                    };
                };
            };
            default: components["responses"]["Error"];
        };
    };
    createToken: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": {
                    name: string;
                };
            };
        };
        responses: {
            201: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["IssuedToken"];
                };
            };
            default: components["responses"]["Error"];
        };
    };
    revokeToken: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
                id: string;
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            204: {
                headers: {
                    [name: string]: unknown;
                };
                content?: never;
            };
            default: components["responses"]["Error"];
        };
    };
    readSnapshot: {
        parameters: {
            query?: never;
            header?: {
                "If-None-Match"?: string;
            };
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody?: never;
        responses: {
            200: {
                headers: {
                    ETag: components["headers"]["SnapshotETag"];
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["Snapshot"];
                };
            };
            304: {
                headers: {
                    ETag: components["headers"]["SnapshotETag"];
                    [name: string]: unknown;
                };
                content?: never;
            };
            default: components["responses"]["Error"];
        };
    };
    evaluateFlags: {
        parameters: {
            query?: never;
            header?: never;
            path: {
                project: components["parameters"]["Project"];
                environment: components["parameters"]["Environment"];
            };
            cookie?: never;
        };
        requestBody: {
            content: {
                "application/json": components["schemas"]["EvaluationRequest"];
            };
        };
        responses: {
            200: {
                headers: {
                    [name: string]: unknown;
                };
                content: {
                    "application/json": components["schemas"]["EvaluationResults"] & {
                        revision: components["schemas"]["Version"];
                    };
                };
            };
            default: components["responses"]["Error"];
        };
    };
}
