\getenv reader_password FLIPKILN_RUNTIME_PASSWORD
BEGIN;
SELECT 'CREATE ROLE flipkiln_reader LOGIN'
WHERE NOT EXISTS (SELECT 1 FROM pg_catalog.pg_roles WHERE rolname = 'flipkiln_reader')
\gexec
ALTER ROLE flipkiln_reader WITH LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION NOBYPASSRLS
    PASSWORD :'reader_password';
ALTER ROLE flipkiln_reader SET default_transaction_read_only = on;
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
REVOKE TEMPORARY ON DATABASE flipkiln FROM PUBLIC;
GRANT CONNECT ON DATABASE flipkiln TO flipkiln_reader;
GRANT USAGE ON SCHEMA public TO flipkiln_reader;
COMMIT;
