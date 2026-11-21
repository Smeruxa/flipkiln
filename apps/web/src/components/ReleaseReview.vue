<script setup lang="ts">
    import { computed, ref, watch } from 'vue';
    import { ApiError, api, environmentLabel, errorMessage, operationKey } from '../lib/api';
    import type { Entry, Environment, Issue, Snapshot } from '../lib/api';
    import { diffEntries } from '../lib/editor';
    import AppDialog from './AppDialog.vue';
    import ChangeDiff from './ChangeDiff.vue';
    import EnvironmentBadge from './EnvironmentBadge.vue';
    import NoticeBox from './NoticeBox.vue';

    export type ReleaseAttempt = { key: string; note: string };
    export type ReviewSnapshot = {
        kind: 'publish' | 'rollback';
        version: number;
        source?: number;
        entries: Entry[];
        active: Snapshot;
        attempt?: ReleaseAttempt;
    };
    const props = defineProps<{ project: string; environment: Environment; review: ReviewSnapshot }>();
    const emit = defineEmits<{
        close: [];
        conflict: [];
        remember: [attempt?: ReleaseAttempt];
        released: [snapshot: Snapshot, review: ReviewSnapshot];
    }>();
    const reviewing = ref(false);
    const releasing = ref(false);
    const note = ref('');
    const validationPassed = ref(false);
    const notice = ref<{ message: string; tone: 'error' | 'success' | 'info'; issues?: Issue[] }>();
    const changed = ref(false);
    const changes = computed(() => diffEntries(props.review.active.entries, props.review.entries));
    const isRollback = computed(() => props.review.kind === 'rollback');
    const environmentName = computed(() => environmentLabel[props.environment]);
    const isProduction = computed(() => props.environment === 'production');
    const title = computed(() =>
        isRollback.value ? `Review rollback to revision ${props.review.source}` : 'Review release',
    );
    const description = computed(() =>
        isRollback.value
            ? `Applications in ${environmentName.value} will read revision ${props.review.source}. ` +
              'Flipkiln will create a new release from that snapshot.'
            : `Applications connected to ${environmentName.value} will read the saved values below.`,
    );
    const confirmLabel = computed(() => {
        if (releasing.value) return isRollback.value ? 'Restoring…' : 'Publishing…';
        if (isProduction.value)
            return isRollback.value
                ? 'Roll back to this revision in Production'
                : 'Publish to Production';
        return isRollback.value ? 'Roll back release' : 'Publish release';
    });

    async function validate() {
        const { entries } = props.review;
        reviewing.value = true;
        validationPassed.value = false;
        notice.value = undefined;
        try {
            const validation = await api.validate(props.project, props.environment, entries);
            validationPassed.value = validation.valid;
            notice.value = validation.valid
                ? { message: 'The saved draft passed validation.', tone: 'success' }
                : { message: 'Draft validation failed.', tone: 'error', issues: validation.issues };
        } catch (error) {
            notice.value = { message: errorMessage(error), tone: 'error' };
        } finally {
            reviewing.value = false;
        }
    }
    watch(
        () => props.review,
        (review) => {
            note.value = review.attempt?.note ?? '';
            notice.value = review.attempt
                ? { message: 'Retry to check the original release request.', tone: 'info' }
                : undefined;
            changed.value = false;
            validationPassed.value = review.kind === 'rollback' || !!review.attempt;
            if (!validationPassed.value) void validate();
        },
        { immediate: true },
    );

    async function release() {
        if (!validationPassed.value || changed.value || releasing.value || reviewing.value) return;
        const review = props.review;
        const { kind, version, source } = review;
        const attempt = review.attempt ?? { key: operationKey(), note: note.value.trim() };
        emit('remember', attempt);
        releasing.value = true;
        notice.value = undefined;
        try {
            const snapshot =
                kind === 'rollback' && source !== undefined
                    ? await api.rollback(
                          props.project,
                          props.environment,
                          source,
                          version,
                          attempt.note,
                          attempt.key,
                      )
                    : await api.publish(
                          props.project,
                          props.environment,
                          version,
                          attempt.note,
                          attempt.key,
                      );
            emit('remember');
            emit('released', snapshot, review);
        } catch (error) {
            const ambiguous = error instanceof ApiError && (error.status === 0 || error.status >= 500);
            if (!ambiguous) emit('remember');
            if (error instanceof ApiError && error.code === 'draft_conflict') {
                changed.value = true;
                emit('conflict');
            }
            notice.value = {
                message: ambiguous
                    ? 'Flipkiln could not confirm the result. Retry to check the original request.'
                    : errorMessage(error),
                tone: 'error',
                issues: error instanceof ApiError ? error.issues : [],
            };
        } finally {
            releasing.value = false;
        }
    }
</script>

<template>
    <AppDialog
        :open="true"
        :title="title"
        :description="description"
        :busy="releasing || reviewing"
        wide
        @update:open="!$event && emit('close')"
    >
        <div class="release-review">
            <div class="row-between">
                <EnvironmentBadge :environment="environment" />
                <span class="muted review-revisions">
                    Published r{{ review.active.revision }} /
                    {{ isRollback ? `Revision ${review.source}` : 'Saved draft' }}
                </span>
            </div>
            <NoticeBox
                v-if="isRollback"
                message="Rollback replaces the saved draft and creates a new revision."
            />
            <p v-if="reviewing" class="empty-inline" role="status">Validating the saved draft…</p>
            <NoticeBox
                v-if="notice"
                :message="notice.message"
                :tone="notice.tone"
                :issues="notice.issues"
            >
                <button v-if="!isRollback && !validationPassed" class="text-link" @click="validate">
                    Retry validation
                </button>
            </NoticeBox>
            <div>
                <h3>
                    {{ changes.length }} {{ changes.length === 1 ? 'entry change' : 'entry changes' }}
                </h3>
                <ChangeDiff :changes="changes" />
            </div>
            <form id="release-note-form" @submit.prevent="release">
                <div class="field">
                    <label for="release-note">Release note (optional)</label>
                    <textarea
                        id="release-note"
                        v-model="note"
                        rows="2"
                        placeholder="Describe the change for your team"
                        maxlength="1000"
                        :disabled="releasing"
                        :readonly="!!review.attempt"
                    />
                </div>
            </form>
        </div>
        <template #actions>
            <button
                type="button"
                class="button button--secondary"
                :disabled="reviewing || releasing"
                @click="emit('close')"
            >
                {{ changed ? 'Close' : 'Cancel' }}
            </button>
            <button
                type="submit"
                form="release-note-form"
                :class="isRollback ? 'button button--danger' : 'button button--primary'"
                :disabled="reviewing || releasing || !validationPassed || changed"
            >
                {{ confirmLabel }}
            </button>
        </template>
    </AppDialog>
</template>
