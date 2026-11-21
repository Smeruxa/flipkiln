import { QueryClient } from '@tanstack/vue-query';
import { describe, expect, it } from 'vitest';
import type { DraftState } from '../lib/api';
import { newestDraftState } from './useEnvironmentDraft';

const state = (draftVersion: number): DraftState => ({
    draftVersion,
    entries: [],
    active: { revision: 1, entries: [] },
});

describe('draft state cache', () => {
    it('retains a saved version when an older state request finishes later', async () => {
        const client = new QueryClient();
        const queryKey = ['state', 'workshop/development'];
        const latest = state(3);
        let resolveState!: (value: DraftState) => void;
        const reading = new Promise<DraftState>((resolve) => {
            resolveState = resolve;
        });
        const load = client.fetchQuery({
            queryKey,
            queryFn: () => reading,
            structuralSharing: (previous, incoming) =>
                newestDraftState(previous as DraftState | undefined, incoming as DraftState),
        });
        client.setQueryData(queryKey, latest);
        resolveState(state(2));
        await load;
        expect(client.getQueryData(queryKey)).toBe(latest);
    });

    it('can accept a newer state after the previous save', () => {
        const latest = state(3);
        expect(newestDraftState(state(2), latest)).toBe(latest);
    });
});
