<script setup lang="ts">
    import { computed, ref, watch } from 'vue';
    import { useQuery } from '@tanstack/vue-query';
    import { ChevronRight, CircleDot, History, RotateCcw } from '@lucide/vue';
    import { api, dateLabel, errorMessage } from '../lib/api';
    import type { Environment, ReleaseDetail } from '../lib/api';
    import { displayValue } from '../lib/editor';
    import EnvironmentBadge from './EnvironmentBadge.vue';
    import NoticeBox from './NoticeBox.vue';

    const props = defineProps<{
        project: string;
        environment: Environment;
        activeRevision?: number;
        canPublish: boolean;
        dirty: boolean;
        conflict: boolean;
    }>();
    const emit = defineEmits<{ rollback: [revision: ReleaseDetail] }>();
    const scope = `${props.project}/${props.environment}`;
    const {
        data: releaseData,
        error: releaseError,
        isPending: releasesLoading,
        refetch: loadReleases,
    } = useQuery({
        queryKey: ['releases', scope],
        queryFn: () => api.releases(props.project, props.environment),
    });
    const revision = ref<number>();
    const {
        data: releaseDetail,
        error: detailError,
        isFetching: detailLoading,
        refetch: loadDetail,
    } = useQuery({
        queryKey: computed(() => ['release', scope, revision.value]),
        queryFn: () => api.release(props.project, props.environment, revision.value!),
        enabled: computed(() => revision.value !== undefined),
    });
    const kinds = { initial: 'Initial release', publish: 'Publication', rollback: 'Rollback' };
    watch(
        releaseData,
        (data) => {
            revision.value ??= data?.releases[0]?.revision;
        },
        { immediate: true },
    );
    watch(
        () => props.activeRevision,
        (updated, old) => {
            if (old !== undefined) revision.value = updated;
        },
    );
</script>

<template>
    <div class="split-grid split-grid--history">
        <section class="panel" aria-labelledby="history-heading">
            <header class="panel-heading">
                <h2 id="history-heading">Revisions</h2>
                <p>Publication and rollback activity.</p>
            </header>
            <div v-if="releasesLoading" class="panel-placeholder" role="status">Loading releases…</div>
            <div v-else-if="releaseError" class="panel-pad">
                <NoticeBox :message="errorMessage(releaseError)" tone="error">
                    <button class="text-link" @click="loadReleases()">Try again</button>
                </NoticeBox>
            </div>
            <ul v-else class="record-list release-list">
                <li v-for="item in releaseData?.releases ?? []" :key="item.revision">
                    <button
                        class="record-row release-row"
                        type="button"
                        :class="{ 'entry-row--active': revision === item.revision }"
                        :aria-current="revision === item.revision ? 'true' : undefined"
                        :aria-label="`Revision ${item.revision}`"
                        @click="revision = item.revision"
                    >
                        <span class="record-info">
                            <span class="record-icon">
                                <RotateCcw v-if="item.kind === 'rollback'" :size="18" />
                                <History v-else :size="18" />
                            </span>
                            <span>
                                <strong>Revision {{ item.revision }}</strong>
                                <small>{{ kinds[item.kind] }} · {{ dateLabel(item.createdAt) }}</small>
                            </span>
                        </span>
                        <span v-if="item.revision === activeRevision" class="live-label">
                            <CircleDot :size="13" />
                            Live
                        </span>
                        <ChevronRight v-else :size="16" />
                    </button>
                </li>
            </ul>
        </section>
        <section class="panel" aria-label="Release snapshot">
            <div v-if="detailLoading" class="panel-placeholder" role="status">Loading revision…</div>
            <div v-else-if="detailError" class="panel-pad">
                <NoticeBox :message="errorMessage(detailError)" tone="error">
                    <button class="text-link" @click="loadDetail()">Try again</button>
                </NoticeBox>
            </div>
            <template v-else-if="releaseDetail">
                <header class="panel-heading row-between">
                    <div>
                        <h2>Revision {{ releaseDetail.revision }}</h2>
                        <p>{{ kinds[releaseDetail.kind] }} by {{ releaseDetail.actor }}</p>
                    </div>
                    <EnvironmentBadge :environment="environment" />
                </header>
                <div class="panel-pad history-body">
                    <p v-if="releaseDetail.note">{{ releaseDetail.note }}</p>
                    <p v-if="releaseDetail.sourceRevision">
                        Restored from revision {{ releaseDetail.sourceRevision }}.
                    </p>
                    <div class="snapshot-meta">
                        <h3>Snapshot</h3>
                        <span>{{ releaseDetail.entries.length }} keys</span>
                    </div>
                    <div v-for="entry in releaseDetail.entries" :key="entry.key" class="snapshot-row">
                        <code>{{ entry.key }}</code>
                        <pre>{{ displayValue(entry.defaultValue, true) }}</pre>
                    </div>
                    <p v-if="!releaseDetail.entries.length" class="empty-inline">
                        This revision contains no entries.
                    </p>
                </div>
                <footer
                    v-if="canPublish && activeRevision && releaseDetail.revision !== activeRevision"
                    class="editor-footer history-footer"
                >
                    <button
                        class="button button--secondary"
                        type="button"
                        :disabled="dirty || conflict"
                        @click="emit('rollback', releaseDetail)"
                    >
                        <RotateCcw :size="16" />
                        Review rollback
                    </button>
                    <span v-if="dirty" class="muted">Save your edits first.</span>
                </footer>
            </template>
            <div v-else class="panel-placeholder">Select a revision to inspect its snapshot.</div>
        </section>
    </div>
</template>
