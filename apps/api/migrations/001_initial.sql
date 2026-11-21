CREATE TABLE users (
    username text PRIMARY KEY,
    password_hash text NOT NULL,
    global_admin boolean NOT NULL DEFAULT false
);
CREATE TABLE sessions (
    token_hash text PRIMARY KEY,
    username text NOT NULL REFERENCES users (username) ON DELETE CASCADE,
    expires_at timestamptz NOT NULL DEFAULT now() + interval '12 hours'
);
CREATE INDEX sessions_expiry ON sessions (expires_at);
CREATE TABLE projects (
    id text PRIMARY KEY,
    name text NOT NULL
);
CREATE TABLE memberships (
    project text NOT NULL REFERENCES projects (id) ON DELETE CASCADE,
    username text NOT NULL REFERENCES users (username) ON DELETE CASCADE,
    role smallint NOT NULL CHECK (role BETWEEN 0 AND 3),
    PRIMARY KEY (project, username)
);
CREATE TABLE environments (
    project text NOT NULL REFERENCES projects (id) ON DELETE CASCADE,
    name text NOT NULL CHECK (name IN ('development', 'production')),
    draft_version int NOT NULL DEFAULT 1 CHECK (draft_version > 0),
    draft_entries jsonb NOT NULL CHECK (jsonb_typeof(draft_entries) = 'array'),
    active_revision int NOT NULL CHECK (active_revision > 0),
    PRIMARY KEY (project, name)
);
CREATE TABLE releases (
    project text NOT NULL,
    environment text NOT NULL,
    revision int NOT NULL CHECK (revision > 0),
    kind text NOT NULL CHECK (kind IN ('initial','publish','rollback')),
    source_revision int,
    entries jsonb NOT NULL CHECK (jsonb_typeof(entries) = 'array'),
    actor text NOT NULL REFERENCES users (username),
    note text NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (project, environment, revision),
    FOREIGN KEY (project, environment) REFERENCES environments (project, name),
    FOREIGN KEY (project, environment, source_revision)
        REFERENCES releases (project, environment, revision),
    CHECK ((kind = 'rollback') = (source_revision IS NOT NULL))
);
ALTER TABLE environments ADD FOREIGN KEY (project, name, active_revision)
    REFERENCES releases (project, environment, revision)
    DEFERRABLE INITIALLY DEFERRED;
CREATE TABLE entry_types (
    project text NOT NULL,
    environment text NOT NULL,
    key text NOT NULL,
    type text NOT NULL CHECK (type IN ('boolean','string','number','json')),
    PRIMARY KEY (project, environment, key),
    FOREIGN KEY (project, environment) REFERENCES environments (project, name)
);
CREATE TABLE operation_keys (
    project text NOT NULL,
    environment text NOT NULL,
    actor text NOT NULL REFERENCES users (username),
    key text NOT NULL,
    parameters jsonb NOT NULL,
    revision int NOT NULL,
    PRIMARY KEY (project, environment, actor, key),
    FOREIGN KEY (project, environment, revision)
        REFERENCES releases (project, environment, revision)
);
CREATE TABLE tokens (
    id text PRIMARY KEY,
    project text NOT NULL,
    environment text NOT NULL,
    name text NOT NULL,
    token_hash text UNIQUE NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    revoked_at timestamptz,
    FOREIGN KEY (project, environment) REFERENCES environments (project, name)
);
CREATE INDEX tokens_environment ON tokens (project, environment);
CREATE TABLE flipkiln_installation (
    singleton boolean PRIMARY KEY CHECK (singleton),
    mode text NOT NULL CHECK (mode IN ('demo', 'standard'))
);
