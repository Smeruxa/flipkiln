<script setup lang="ts">
    import { computed, ref } from 'vue';
    import {
        Braces,
        ChevronRight,
        Layers3,
        Plus,
        Save,
        Search,
        ToggleRight,
        Type,
        Variable,
    } from '@lucide/vue';
    import { typeLabel } from '../lib/api';
    import type { Entry } from '../lib/api';
    import { displayValue } from '../lib/editor';
    import type { WorkingEntry } from '../lib/editor';

    const props = defineProps<{
        working: WorkingEntry[];
        selectedUid: string;
        liveByKey: Map<string, Entry>;
        stagedKeys: Set<string>;
        removedEntries: Entry[];
        canEdit: boolean;
        busy: boolean;
        dirty: boolean;
    }>();
    const emit = defineEmits<{
        create: [];
        select: [uid: string];
        restore: [entry: Entry];
        save: [];
    }>();
    const search = ref('');
    const typeIcons = { boolean: ToggleRight, json: Braces, string: Type, number: Variable };
    const visibleEntries = computed(() => {
        const query = search.value.trim().toLowerCase();
        return props.working.filter((entry) =>
            `${entry.key} ${entry.description} ${entry.type}`.toLowerCase().includes(query),
        );
    });

    function createEntry() {
        search.value = '';
        emit('create');
    }
</script>

<template>
    <section class="panel entries-panel" aria-labelledby="entries-heading">
        <header class="panel-heading row-between">
            <div>
                <h2 id="entries-heading">Flags &amp; config</h2>
                <p>
                    {{ working.length }} {{ working.length === 1 ? 'entry' : 'entries' }} in the draft
                </p>
            </div>
            <button
                v-if="canEdit"
                type="button"
                class="button button--secondary"
                :disabled="busy"
                @click="createEntry"
            >
                <Plus :size="17" />
                New entry
            </button>
        </header>
        <div v-if="working.length" class="filter-bar">
            <div class="search-field">
                <Search :size="18" />
                <input v-model="search" aria-label="Search entries" placeholder="Search keys…" />
            </div>
        </div>
        <div v-if="!working.length" class="panel-placeholder">
            <Layers3 :size="26" />
            <strong>No configuration entries</strong>
            <p>
                {{
                    canEdit
                        ? 'Add a flag or a remote value to this draft.'
                        : 'An editor can add entries to this environment.'
                }}
            </p>
        </div>
        <p v-else-if="!visibleEntries.length" class="panel-placeholder">No matching entries.</p>
        <div v-else class="entry-list" aria-label="Configuration entries">
            <div class="entry-list-head" aria-hidden="true">
                <span>Key</span>
                <span>Type</span>
                <span>Live default</span>
                <span></span>
            </div>
            <button
                v-for="entry in visibleEntries"
                :key="entry.uid"
                :id="`entry-${entry.uid}`"
                type="button"
                class="entry-row"
                :class="{ 'entry-row--active': selectedUid === entry.uid }"
                :aria-current="selectedUid === entry.uid ? 'true' : undefined"
                :aria-label="entry.key || 'New entry'"
                @click="emit('select', entry.uid)"
            >
                <span class="entry-key">
                    <span class="entry-icon" :class="`entry-icon--${entry.type}`">
                        <component :is="typeIcons[entry.type]" :size="17" />
                    </span>
                    <span>
                        <code>{{ entry.key || 'New entry' }}</code>
                        <small>
                            {{
                                !liveByKey.has(entry.key)
                                    ? 'New in draft'
                                    : stagedKeys.has(entry.key)
                                      ? 'Draft differs'
                                      : entry.description || 'In sync'
                            }}
                        </small>
                    </span>
                </span>
                <span class="entry-type">{{ typeLabel[entry.type] }}</span>
                <span
                    class="entry-live"
                    :class="{
                        'entry-live--boolean': entry.type === 'boolean' && liveByKey.has(entry.key),
                    }"
                    :title="displayValue(liveByKey.get(entry.key)?.defaultValue)"
                >
                    {{
                        entry.type === 'json' && liveByKey.has(entry.key)
                            ? '{ … }'
                            : displayValue(liveByKey.get(entry.key)?.defaultValue)
                    }}
                </span>
                <ChevronRight :size="16" class="entry-chevron" />
            </button>
        </div>
        <div v-if="removedEntries.length" class="removed-list">
            <strong>Removed from draft</strong>
            <div v-for="entry in removedEntries" :key="entry.key">
                <code>{{ entry.key }}</code>
                <button
                    v-if="canEdit"
                    type="button"
                    class="text-link"
                    :disabled="busy"
                    @click="emit('restore', entry)"
                >
                    Restore
                </button>
            </div>
        </div>
        <footer v-if="!working.length && dirty && canEdit" class="editor-footer history-footer">
            <button class="button button--primary" :disabled="busy" @click="emit('save')">
                <Save :size="16" />
                Save draft
            </button>
        </footer>
    </section>
</template>
