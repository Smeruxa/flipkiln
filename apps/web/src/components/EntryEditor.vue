<script setup lang="ts">
    import { computed, ref, useId, watch } from 'vue';
    import { Archive, Braces, FlaskConical, Radio, Save } from '@lucide/vue';
    import { TabsContent, TabsList, TabsRoot, TabsTrigger } from 'reka-ui';
    import type {
        Entry,
        EntryType,
        Environment,
        EvaluationResult,
        Issue,
        RequestContext,
    } from '../lib/api';
    import { typeLabel } from '../lib/api';
    import { displayValue, initialValue, valueInput } from '../lib/editor';
    import type { WorkingEntry } from '../lib/editor';
    import EnvironmentBadge from './EnvironmentBadge.vue';
    import ValueInput from './ValueInput.vue';
    import TargetingRules from './TargetingRules.vue';
    import EvaluationPreview from './EvaluationPreview.vue';

    const entry = defineModel<WorkingEntry>({ required: true });
    const props = defineProps<{
        environment: Environment;
        live?: Entry;
        revision: number;
        canEdit: boolean;
        busy: boolean;
        dirty: boolean;
        previewResult?: EvaluationResult;
        issues?: Issue[];
    }>();
    const emit = defineEmits<{
        save: [];
        archive: [];
        validate: [];
        preview: [context: RequestContext];
        resetPreview: [];
    }>();
    const id = useId();
    const tab = ref('details');
    const defaultIssues = computed(() =>
        props.issues
            ?.filter(
                (issue) => issue.path === '/defaultValue' || issue.path.startsWith('/defaultValue/'),
            )
            .map((issue) => ({ ...issue, path: issue.path.slice('/defaultValue'.length) })),
    );
    const schemaIssues = computed(
        () =>
            props.issues?.filter(
                (issue) => issue.path === '/schema' || issue.path.startsWith('/schema/'),
            ) ?? [],
    );
    watch(
        () => props.issues,
        () => {
            if (defaultIssues.value?.length || schemaIssues.value.length) tab.value = 'details';
            else if (props.issues?.some((issue) => issue.path.startsWith('/rules')))
                tab.value = 'rules';
        },
    );
    const title = computed(() => entry.value.key || 'New entry');
    watch(
        () => entry.value.uid,
        () => {
            tab.value = 'details';
        },
    );

    function setType(type: EntryType) {
        entry.value.type = type;
        entry.value.valueInput = valueInput(type, initialValue(type));
        for (const rule of entry.value.rules) rule.valueInput = valueInput(type, initialValue(type));
    }
</script>

<template>
    <section class="panel editor-panel" aria-labelledby="editor-heading">
        <header class="panel-heading editor-heading">
            <div>
                <div class="editor-overline">
                    <EnvironmentBadge :environment="environment" />
                    <span class="editor-stage">
                        {{ dirty ? 'Unsaved draft edits' : 'Saved draft' }}
                    </span>
                </div>
                <h2 id="editor-heading" tabindex="-1">{{ title }}</h2>
                <p>
                    {{ typeLabel[entry.type] }}
                    {{ entry.type === 'boolean' ? 'flag' : 'configuration' }}
                </p>
            </div>
        </header>
        <TabsRoot v-model="tab">
            <TabsList aria-label="Entry editor" class="tabs">
                <TabsTrigger class="tab" value="details">Details</TabsTrigger>
                <TabsTrigger class="tab" value="rules">
                    Targeting
                    <span class="tab-count">{{ entry.rules.length }}</span>
                </TabsTrigger>
                <TabsTrigger class="tab" value="preview">Preview</TabsTrigger>
            </TabsList>
            <TabsContent value="details" class="editor-content">
                <div class="published-box">
                    <div class="published-label">
                        <Radio :size="15" />
                        <strong>Published default</strong>
                        <span>r{{ revision }}</span>
                    </div>
                    <pre v-if="live">{{ displayValue(live.defaultValue, true) }}</pre>
                    <div v-else class="muted">This entry has not been published.</div>
                </div>
                <div v-if="entry.identityEditable" class="field-grid">
                    <div class="field">
                        <label :for="`${id}-key`">Key</label>
                        <input
                            :id="`${id}-key`"
                            v-model="entry.key"
                            placeholder="checkout.example"
                            maxlength="64"
                            :readonly="!canEdit || busy"
                        />
                    </div>
                    <div class="field">
                        <label :for="`${id}-type`">Type</label>
                        <select
                            :id="`${id}-type`"
                            :value="entry.type"
                            :disabled="!canEdit || busy"
                            @change="setType(($event.target as HTMLSelectElement).value as EntryType)"
                        >
                            <option v-for="(label, value) in typeLabel" :key="value" :value="value">
                                {{ label }}
                            </option>
                        </select>
                    </div>
                </div>
                <ValueInput
                    v-model="entry.valueInput"
                    :type="entry.type"
                    label="Default value"
                    :disabled="!canEdit || busy"
                    :issues="defaultIssues"
                />
                <div class="field">
                    <label :for="`${id}-description`">Description</label>
                    <textarea
                        :id="`${id}-description`"
                        v-model="entry.description"
                        rows="2"
                        maxlength="1000"
                        :readonly="!canEdit || busy"
                        placeholder="What does this entry control?"
                    />
                </div>
                <details
                    v-if="entry.type === 'json'"
                    class="schema-details"
                    :open="schemaIssues.length > 0 || undefined"
                >
                    <summary>
                        <Braces :size="16" />
                        JSON Schema
                    </summary>
                    <div class="field">
                        <label :for="`${id}-schema`">Schema (Draft 7 subset)</label>
                        <textarea
                            :id="`${id}-schema`"
                            v-model="entry.schemaInput"
                            rows="8"
                            class="code-input"
                            spellcheck="false"
                            :readonly="!canEdit || busy"
                            :aria-invalid="schemaIssues.length ? true : undefined"
                            :aria-describedby="schemaIssues.length ? `${id}-schema-errors` : undefined"
                        />
                        <div
                            v-if="schemaIssues.length"
                            :id="`${id}-schema-errors`"
                            class="inline-errors"
                        >
                            <div v-for="(issue, index) in schemaIssues" :key="index">
                                {{ issue.message }}
                            </div>
                        </div>
                    </div>
                </details>
            </TabsContent>
            <TabsContent value="rules">
                <TargetingRules v-model="entry" :can-edit="canEdit" :busy="busy" />
            </TabsContent>
            <TabsContent value="preview" class="editor-content">
                <EvaluationPreview
                    v-if="canEdit"
                    :busy="busy"
                    :result="previewResult"
                    @evaluate="emit('preview', $event)"
                    @reset="emit('resetPreview')"
                />
                <p v-else class="empty-inline">Evaluating a draft requires the editor role.</p>
            </TabsContent>
        </TabsRoot>
        <footer v-if="canEdit" class="editor-footer">
            <button
                type="button"
                class="button icon-button archive-button"
                aria-label="Remove entry from draft"
                title="Remove entry from draft"
                :disabled="busy"
                @click="emit('archive')"
            >
                <Archive :size="18" />
            </button>
            <div class="stack-actions">
                <button
                    type="button"
                    class="button button--secondary"
                    :disabled="busy"
                    @click="emit('validate')"
                >
                    <FlaskConical :size="16" />
                    Validate
                </button>
                <button
                    type="button"
                    class="button button--primary"
                    :disabled="busy || !dirty"
                    @click="emit('save')"
                >
                    <Save :size="16" />
                    Save draft
                </button>
            </div>
        </footer>
    </section>
</template>
