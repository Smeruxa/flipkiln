<script setup lang="ts">
    import { Minus, Pencil, Plus } from '@lucide/vue';
    import { displayValue } from '../lib/editor';
    import type { EntryChange } from '../lib/editor';

    defineProps<{ changes: EntryChange[] }>();
    const icons = { Added: Plus, Removed: Minus, Changed: Pencil };
</script>

<template>
    <div v-if="changes.length" class="diff-list">
        <section v-for="change in changes" :key="change.key" class="diff-entry">
            <div class="diff-key">
                <code>{{ change.key }}</code>
                <span class="change-pill" :class="`change-pill--${change.kind.toLowerCase()}`">
                    <component :is="icons[change.kind]" :size="13" />
                    {{ change.kind }}
                </span>
            </div>
            <div v-for="field in change.fields" :key="field.field" class="diff-field">
                <strong>{{ field.label }}</strong>
                <div class="diff-values">
                    <div>
                        <span>Before</span>
                        <pre>{{ displayValue(field.before, true) }}</pre>
                    </div>
                    <div>
                        <span>After</span>
                        <pre>{{ displayValue(field.after, true) }}</pre>
                    </div>
                </div>
            </div>
        </section>
    </div>
    <p v-else class="empty-inline">The two snapshots contain the same values.</p>
</template>
