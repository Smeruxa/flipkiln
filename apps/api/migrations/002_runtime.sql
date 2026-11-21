CREATE FUNCTION public.flipkiln_client_snapshot(p_project text, p_environment text, p_token_hash text)
RETURNS TABLE (revision integer, entries jsonb)
LANGUAGE sql
STABLE
SECURITY DEFINER
SET search_path = pg_catalog
AS $$
    SELECT r.revision, r.entries
    FROM public.tokens AS t
    JOIN public.environments AS e ON e.project = t.project AND e.name = t.environment
    JOIN public.releases AS r
        ON r.project = e.project AND r.environment = e.name AND r.revision = e.active_revision
    WHERE t.project = p_project
        AND t.environment = p_environment
        AND t.token_hash = p_token_hash
        AND t.revoked_at IS NULL;
$$;

REVOKE EXECUTE ON FUNCTION public.flipkiln_client_snapshot(text, text, text) FROM PUBLIC;
