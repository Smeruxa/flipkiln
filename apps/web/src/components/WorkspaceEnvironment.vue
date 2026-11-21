<script setup lang="ts">
    import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue';
    import { onBeforeRouteLeave, onBeforeRouteUpdate } from 'vue-router';
    import { useQueryClient } from '@tanstack/vue-query';
    import {
        Archive,
        ArrowLeft,
        ArrowUpRight,
        FilePenLine,
        GitCompareArrows,
        Layers3,
        Radio,
    } from '@lucide/vue';
    import { environmentLabel, errorMessage, roleRank } from '../lib/api';
    import type { Entry, Environment, Project, ReleaseDetail, Snapshot } from '../lib/api';
    import { useEnvironmentDraft } from '../composables/useEnvironmentDraft';
    import ConfigurationEntries from './ConfigurationEntries.vue';
    import AppDialog from './AppDialog.vue';
    import EntryEditor from './EntryEditor.vue';
    import EnvironmentBadge from './EnvironmentBadge.vue';
    import MembersPanel from './MembersPanel.vue';
    import NoticeBox from './NoticeBox.vue';
    import TokensPanel from './TokensPanel.vue';
    import ReleaseHistory from './ReleaseHistory.vue';
    import ReleaseReview from './ReleaseReview.vue';
    import type { ReleaseAttempt, ReviewSnapshot } from './ReleaseReview.vue';

    const props = defineProps<{
        project: Project;
        environment: Environment;
        section: 'entries' | 'releases' | 'tokens' | 'members';
    }>();
    const client = useQueryClient();
    const canEdit = computed(() => roleRank[props.project.role] >= roleRank.editor);
    const canPublish = computed(() => roleRank[props.project.role] >= roleRank.publisher);
    const releaseKey = ['releases', `${props.project.id}/${props.environment}`];
    const draft = useEnvironmentDraft(props.project.id, props.environment);
    const {
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
        saveDraft,
        validateDraft,
        previewEntry,
        acceptLatest,
    } = draft;
    watch(notice, (feedback) => {
        if (feedback?.tone === 'error')
            void nextTick(() => document.getElementById('draft-notice')?.focus());
    });
    const mobileEditor = ref(false);
    const archiveOpen = ref(false);
    const headings = {
        entries: 'Configuration',
        releases: 'Release history',
        tokens: 'Read tokens',
        members: 'Project access',
    };
    const descriptions = {
        entries: 'Manage feature flags and remote values for this environment.',
        releases: 'Inspect the configuration snapshots published to this environment.',
        tokens: 'Connect applications to published configuration.',
        members: 'Control who can view, edit, and publish configuration.',
    };

    function beforeUnload(event: BeforeUnloadEvent) {
        if (dirty.value) {
            event.preventDefault();
            event.returnValue = '';
        }
    }
    onMounted(() => window.addEventListener('beforeunload', beforeUnload));
    onUnmounted(() => window.removeEventListener('beforeunload', beforeUnload));
    function confirmLeave() {
        return !dirty.value || window.confirm('Discard the unsaved edits in this environment?');
    }
    onBeforeRouteLeave((to) => to.name === 'login' || confirmLeave());
    onBeforeRouteUpdate(
        (to, from) =>
            (to.params.project === from.params.project &&
                to.params.environment === from.params.environment) ||
            confirmLeave(),
    );

    function selectEntry(uid: string) {
        draft.selectEntry(uid);
        focusMobileEditor();
    }
    function focusMobileEditor() {
        mobileEditor.value = true;
        if (matchMedia('(max-width: 670px)').matches) {
            void nextTick(() => {
                const heading = document.getElementById('editor-heading');
                heading?.focus({ preventScroll: true });
                heading
                    ?.closest('.editor-wrap')
                    ?.scrollIntoView({ block: 'start', behavior: 'instant' });
            });
        }
    }
    function backToEntries() {
        mobileEditor.value = false;
        void nextTick(() => document.getElementById(`entry-${selectedUid.value}`)?.focus());
    }
    function addEntry() {
        draft.addEntry();
        focusMobileEditor();
    }
    function archiveEntry() {
        draft.archiveEntry();
        archiveOpen.value = false;
    }
    function restoreEntry(entry: Entry) {
        draft.restoreEntry(entry);
        focusMobileEditor();
    }
    const archiveDescription = computed(() => {
        const name = selected.value?.key || 'the entry';
        return (
            `This removes ${name} from the working draft. ` +
            'Save and publish the draft to remove it from application responses.'
        );
    });

    const review = ref<ReviewSnapshot>();
    const reviewOpen = ref(false);
    const reviewCompleted = ref(false);
    const releaseAttempts = new Map<string, ReleaseAttempt>();
    let releaseTrigger: HTMLElement | null = null;
    function reviewKey(item: ReviewSnapshot) {
        return `${item.kind}/${item.version}/${item.source ?? ''}`;
    }
    function rememberAttempt(attempt?: ReleaseAttempt) {
        const item = review.value;
        if (!item) return;
        item.attempt = attempt;
        if (attempt) releaseAttempts.set(reviewKey(item), attempt);
        else releaseAttempts.delete(reviewKey(item));
    }
    function closeReview() {
        reviewOpen.value = false;
    }
    function focusAfterReview() {
        if (reviewCompleted.value) return document.getElementById('draft-notice');
        if (releaseTrigger?.isConnected && !releaseTrigger.matches(':disabled')) return releaseTrigger;
        return document.getElementById('section-title');
    }
    function openReview(target?: ReleaseDetail) {
        if (!base.value || dirty.value || conflict.value || stateError.value) return;
        const snapshot: ReviewSnapshot = {
            kind: target ? 'rollback' : 'publish',
            version: base.value.draftVersion,
            source: target?.revision,
            entries: target?.entries ?? base.value.entries,
            active: base.value.active,
            savedDraft: base.value.entries,
        };
        snapshot.attempt = releaseAttempts.get(reviewKey(snapshot));
        releaseTrigger = document.activeElement instanceof HTMLElement ? document.activeElement : null;
        reviewCompleted.value = false;
        review.value = snapshot;
        reviewOpen.value = true;
    }
    async function afterRelease(snapshot: Snapshot, completed: ReviewSnapshot) {
        conflict.value = false;
        await loadState();
        notice.value = {
            message:
                completed.kind === 'rollback'
                    ? `Restored revision ${completed.source} as revision ${snapshot.revision} ` +
                      `in ${environmentLabel[props.environment]}.`
                    : `Published revision ${snapshot.revision} to ${environmentLabel[props.environment]}.`,
            tone: 'success',
        };
        void client.invalidateQueries({ queryKey: releaseKey });
        reviewCompleted.value = true;
        closeReview();
    }
</script>

<template>
    <div class="workspace-flow">
        <header class="page-intro">
            <div>
                <h1 id="section-title" tabindex="-1">{{ headings[section] }}</h1>
                <p>{{ descriptions[section] }}</p>
            </div>
            <div v-if="section === 'entries' && canPublish" class="review-wrap">
                <button
                    class="button button--primary button--review"
                    type="button"
                    :disabled="busy || dirty || !pendingChanges.length || conflict || !!stateError"
                    @click="openReview()"
                >
                    <GitCompareArrows :size="17" />
                    Review changes
                    <ArrowUpRight :size="16" class="review-arrow" />
                </button>
                <small v-if="dirty">Save your draft to review these edits.</small>
            </div>
        </header>

        <NoticeBox
            v-if="stateError && (section === 'entries' || section === 'releases')"
            :message="errorMessage(stateError)"
            tone="error"
        >
            <button class="text-link" @click="loadState()">Try again</button>
        </NoticeBox>
        <NoticeBox
            id="draft-notice"
            tabindex="-1"
            v-if="notice && (section === 'entries' || section === 'releases')"
            :message="notice.message"
            :tone="notice.tone"
            :issues="notice.issues"
        >
            <button v-if="conflict" class="text-link" :disabled="busy" @click="acceptLatest">
                Load latest draft
            </button>
        </NoticeBox>
        <NoticeBox v-else-if="conflict" message="Another editor changed the draft." tone="error">
            <button class="text-link" :disabled="busy" @click="acceptLatest">Load latest draft</button>
        </NoticeBox>

        <template v-if="section === 'entries'">
            <NoticeBox
                v-if="!canEdit"
                message="Read-only access. A project admin can grant you permission to edit drafts."
            />
            <div v-if="base" class="status-strip" aria-label="Environment status">
                <div>
                    <span class="status-icon status-icon--live"><Radio :size="18" /></span>
                    <div>
                        <span class="status-label">
                            {{ stateError ? 'Last loaded release' : 'Live release' }}
                        </span>
                        <strong>Revision {{ base.active.revision }}</strong>
                    </div>
                </div>
                <div>
                    <span class="status-icon status-icon--draft"><FilePenLine :size="17" /></span>
                    <div>
                        <span class="status-label">Working draft</span>
                        <strong v-if="dirty">Unsaved edits</strong>
                        <strong v-else-if="pendingChanges.length">
                            {{ pendingChanges.length }}
                            {{ pendingChanges.length === 1 ? 'saved change' : 'saved changes' }}
                        </strong>
                        <strong v-else>Matches live</strong>
                    </div>
                </div>
                <div>
                    <span class="status-icon status-icon--keys"><Layers3 :size="18" /></span>
                    <div>
                        <span class="status-label">Published keys</span>
                        <strong>{{ base.active.entries.length }}</strong>
                    </div>
                </div>
            </div>
            <div v-if="stateLoading" class="panel panel-placeholder" role="status">
                Loading configuration…
            </div>
            <div
                v-else-if="base"
                class="split-grid"
                :class="{
                    'split-grid--single': !selected,
                    'split-grid--editing': mobileEditor && selected,
                }"
            >
                <ConfigurationEntries
                    :working="working"
                    :selected-uid="selectedUid"
                    :live-by-key="liveByKey"
                    :staged-keys="stagedKeys"
                    :removed-entries="removedEntries"
                    :can-edit="canEdit"
                    :busy="busy"
                    :dirty="dirty"
                    @create="addEntry"
                    @select="selectEntry"
                    @restore="restoreEntry"
                    @save="saveDraft"
                />
                <div v-if="selected" class="editor-wrap">
                    <button class="button button--secondary mobile-back" @click="backToEntries">
                        <ArrowLeft :size="16" />
                        Back to entries
                    </button>
                    <EntryEditor
                        :key="selected.uid"
                        v-model="selected"
                        :environment="environment"
                        :revision="base.active.revision"
                        :live="selectedLive"
                        :can-edit="canEdit"
                        :busy="busy"
                        :dirty="dirty"
                        :preview-result="result"
                        :issues="selectedIssues"
                        @save="saveDraft"
                        @validate="validateDraft"
                        @archive="archiveOpen = true"
                        @preview="previewEntry"
                        @reset-preview="result = undefined"
                    />
                </div>
            </div>
        </template>

        <ReleaseHistory
            v-else-if="section === 'releases'"
            :project="project.id"
            :environment="environment"
            :active-revision="stateError ? undefined : base?.active.revision"
            :can-publish="canPublish"
            :dirty="dirty"
            :conflict="conflict"
            @rollback="openReview"
        />

        <TokensPanel
            v-else-if="section === 'tokens' && project.role === 'admin'"
            :project="project.id"
            :environment="environment"
        />
        <MembersPanel
            v-else-if="section === 'members' && project.role === 'admin'"
            :project="project.id"
        />
        <NoticeBox v-else message="This section requires the project admin role." />

        <AppDialog
            v-model:open="archiveOpen"
            title="Remove entry from draft"
            :description="archiveDescription"
        >
            <EnvironmentBadge :environment="environment" />
            <template #actions>
                <button class="button button--secondary" @click="archiveOpen = false">Cancel</button>
                <button class="button button--danger" @click="archiveEntry">
                    <Archive :size="16" />
                    Remove entry
                </button>
            </template>
        </AppDialog>

        <ReleaseReview
            v-if="review"
            :project="project.id"
            :environment="environment"
            :review="review"
            :open="reviewOpen"
            :finish="afterRelease"
            :return-focus="focusAfterReview"
            @close="closeReview"
            @conflict="conflict = true"
            @remember="rememberAttempt"
        />
    </div>
</template>
