import { computed, ref, watch } from 'vue';
import { useQuery, useQueryClient } from '@tanstack/vue-query';
import { ApiError, api, errorMessage } from '../lib/api';
import type {
    DraftState,
    Entry,
    Environment,
    EvaluationResult,
    Issue,
    RequestContext,
} from '../lib/api';
import { diffEntries, editableEntry, serializeDraft } from '../lib/editor';
import type { WorkingEntry } from '../lib/editor';

export function newestDraftState(current: DraftState | undefined, candidate: DraftState): DraftState {
    return current && current.draftVersion >= candidate.draftVersion ? current : candidate;
}

type DraftNotice = { message: string; tone: 'error' | 'success' | 'info'; issues?: Issue[] };

export function useEnvironmentDraft(project: string, environment: Environment) {
    const client = useQueryClient();
    const scope = `${project}/${environment}`;
    const stateKey = ['state', scope];
    const {
        data: stateData,
        error: stateError,
        isPending: stateLoading,
        refetch: loadState,
    } = useQuery({
        queryKey: stateKey,
        queryFn: ({ signal }) => api.state(project, environment, signal),
        structuralSharing: (current, candidate) =>
            newestDraftState(current as DraftState | undefined, candidate as DraftState),
    });
    const working = ref<WorkingEntry[]>([]);
    const base = ref<DraftState>();
    const selectedUid = ref('');
    const busy = ref(false);
    const conflict = ref(false);
    const result = ref<EvaluationResult>();
    const notice = ref<DraftNotice>();
    const workingEntries = computed(() => serializeDraft(working.value));
    const dirty = computed(
        () =>
            !!base.value &&
            (workingEntries.value.issues.length > 0 ||
                diffEntries(base.value.entries, workingEntries.value.entries).length > 0),
    );
    const pendingChanges = computed(() =>
        base.value ? diffEntries(base.value.active.entries, base.value.entries) : [],
    );
    const selectedIndex = computed(() =>
        working.value.findIndex((entry) => entry.uid === selectedUid.value),
    );
    const selected = computed({
        get: () => working.value[selectedIndex.value],
        set: (entry) => {
            working.value[selectedIndex.value] = entry;
        },
    });
    const liveByKey = computed(
        () => new Map(base.value?.active.entries.map((entry) => [entry.key, entry])),
    );
    const selectedIssues = computed(() => {
        const prefix = `/entries/${selectedIndex.value}`;
        return notice.value?.issues
            ?.filter((issue) => issue.path.startsWith(`${prefix}/`))
            .map((issue) => ({ ...issue, path: issue.path.slice(prefix.length) }));
    });
    const selectedLive = computed(() => liveByKey.value.get(selected.value?.key ?? ''));
    const stagedKeys = computed(() => new Set(pendingChanges.value.map((change) => change.key)));
    const removedEntries = computed(
        () =>
            base.value?.active.entries.filter(
                (item) => !working.value.some((entry) => entry.key === item.key),
            ) ?? [],
    );
    function receiveState(state: DraftState) {
        if (base.value && state.draftVersion < base.value.draftVersion) return;
        const preferred = selected.value?.key;
        base.value = state;
        working.value = state.entries.map((entry) => editableEntry(entry));
        selectedUid.value =
            working.value.find((entry) => entry.key === preferred)?.uid ??
            working.value.find((entry) => entry.key === 'checkout.fast_lane')?.uid ??
            working.value[0]?.uid ??
            '';
    }
    watch(
        stateData,
        (state) => {
            if (!state) return;
            if (!dirty.value) receiveState(state);
            else if (base.value && state.draftVersion > base.value.draftVersion) conflict.value = true;
        },
        { immediate: true },
    );
    watch(workingEntries, () => {
        result.value = undefined;
    });

    function selectEntry(uid: string) {
        selectedUid.value = uid;
        result.value = undefined;
    }

    function addEntry() {
        const entry = editableEntry();
        working.value.push(entry);
        selectEntry(entry.uid);
    }

    function archiveEntry() {
        const index = selectedIndex.value;
        if (index < 0) return;
        working.value.splice(index, 1);
        selectEntry(working.value[Math.min(index, working.value.length - 1)]?.uid ?? '');
    }

    function restoreEntry(entry: Entry) {
        working.value.push(editableEntry(entry));
        selectEntry(entry.key);
    }

    function setError(error: unknown) {
        conflict.value = error instanceof ApiError && error.code === 'draft_conflict';
        notice.value = {
            message: conflict.value
                ? 'Another editor saved this draft. Your local edits are still here.'
                : errorMessage(error),
            tone: 'error',
            issues: error instanceof ApiError ? error.issues : [],
        };
    }
    function validInput() {
        const { issues } = workingEntries.value;
        if (issues.length) {
            notice.value = { message: 'Fix the draft fields below.', tone: 'error', issues };
            return false;
        }
        return true;
    }
    async function performDraftAction(action: () => Promise<void>) {
        if (busy.value) return;
        notice.value = undefined;
        if (!validInput()) return;
        busy.value = true;
        try {
            await action();
        } catch (error) {
            setError(error);
        } finally {
            busy.value = false;
        }
    }
    async function saveDraft() {
        if (!base.value || !dirty.value) return;
        const draftVersion = base.value.draftVersion;
        await performDraftAction(async () => {
            const state = await api.saveDraft(
                project,
                environment,
                draftVersion,
                workingEntries.value.entries,
            );
            await client.cancelQueries({ queryKey: stateKey, exact: true });
            const latest = client.setQueryData<DraftState>(stateKey, state);
            receiveState(latest ?? state);
            conflict.value = false;
            notice.value = {
                message: `Draft saved. Applications continue using revision ${state.active.revision}.`,
                tone: 'success',
            };
        });
    }
    async function validateDraft() {
        await performDraftAction(async () => {
            const validation = await api.validate(project, environment, workingEntries.value.entries);
            notice.value = validation.valid
                ? { message: 'The draft passed validation.', tone: 'success' }
                : { message: 'Draft validation failed.', tone: 'error', issues: validation.issues };
        });
    }
    async function previewEntry(context: RequestContext) {
        if (!selected.value) return;
        const key = selected.value.key.trim();
        result.value = undefined;
        await performDraftAction(async () => {
            const evaluated = await api.preview(
                project,
                environment,
                workingEntries.value.entries,
                [key],
                context,
            );
            if (selected.value?.key.trim() === key) result.value = evaluated.results[0];
        });
    }
    async function acceptLatest() {
        if (busy.value) return;
        if (dirty.value && !window.confirm('Discard the unsaved edits in this environment?')) return;
        busy.value = true;
        try {
            const { data, error } = await loadState();
            if (error) throw error;
            if (data) receiveState(data);
            conflict.value = false;
            notice.value = { message: 'Loaded the latest saved draft.', tone: 'success' };
        } catch (error) {
            setError(error);
        } finally {
            busy.value = false;
        }
    }

    return {
        stateError,
        stateLoading,
        loadState,
        working,
        base,
        selectedUid,
        busy,
        conflict,
        result,
        notice,
        dirty,
        pendingChanges,
        selected,
        liveByKey,
        selectedIssues,
        selectedLive,
        stagedKeys,
        removedEntries,
        selectEntry,
        addEntry,
        archiveEntry,
        restoreEntry,
        saveDraft,
        validateDraft,
        previewEntry,
        acceptLatest,
    };
}
