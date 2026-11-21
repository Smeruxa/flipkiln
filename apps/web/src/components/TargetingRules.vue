<script setup lang="ts">
    import { useId } from 'vue';
    import { ArrowDown, ArrowUp, Plus, Trash2 } from '@lucide/vue';
    import { SwitchRoot, SwitchThumb } from 'reka-ui';
    import { editableRule } from '../lib/editor';
    import type { WorkingEntry } from '../lib/editor';
    import ValueInput from './ValueInput.vue';

    const entry = defineModel<WorkingEntry>({ required: true });
    defineProps<{ canEdit: boolean; busy: boolean }>();
    const id = useId();
    const ruleNames = { ids: 'ID list', attribute: 'Attribute match', rollout: 'Percentage rollout' };

    function moveRule(index: number, offset: number) {
        const other = index + offset;
        const rules = entry.value.rules;
        if (other >= 0 && other < rules.length)
            [rules[index], rules[other]] = [rules[other], rules[index]];
    }
</script>

<template>
    <div class="editor-content">
        <div class="setting-row">
            <div>
                <label :for="`${id}-targeting`">Targeting enabled</label>
                <p>
                    Checks rules from top to bottom and uses the first match. Disabled targeting uses
                    the default value.
                </p>
            </div>
            <SwitchRoot
                :id="`${id}-targeting`"
                v-model="entry.enabled"
                :disabled="!canEdit || busy"
                class="switch"
            >
                <SwitchThumb class="switch-thumb" />
            </SwitchRoot>
        </div>
        <p v-if="!entry.rules.length" class="empty-inline">
            Add a rule to return a different value for specific app contexts.
        </p>
        <section v-for="(rule, index) in entry.rules" :key="rule.id" class="rule-card">
            <div class="rule-card-head">
                <div>
                    <strong>{{ ruleNames[rule.kind] }}</strong>
                    <span class="rule-id">{{ rule.id }}</span>
                </div>
                <div v-if="canEdit" class="stack-actions">
                    <button
                        type="button"
                        class="button icon-button"
                        :aria-label="`Move rule ${index + 1} up`"
                        :disabled="busy || index === 0"
                        @click="moveRule(index, -1)"
                    >
                        <ArrowUp :size="16" />
                    </button>
                    <button
                        type="button"
                        class="button icon-button"
                        :aria-label="`Move rule ${index + 1} down`"
                        :disabled="busy || index === entry.rules.length - 1"
                        @click="moveRule(index, 1)"
                    >
                        <ArrowDown :size="16" />
                    </button>
                    <button
                        type="button"
                        class="button icon-button"
                        :aria-label="`Remove rule ${index + 1}`"
                        :disabled="busy"
                        @click="entry.rules.splice(index, 1)"
                    >
                        <Trash2 :size="16" />
                    </button>
                </div>
            </div>
            <div v-if="rule.kind === 'ids'" class="field">
                <label :for="`${id}-${rule.id}-ids`">User IDs (JSON array)</label>
                <textarea
                    :id="`${id}-${rule.id}-ids`"
                    v-model="rule.ids"
                    :readonly="!canEdit || busy"
                    rows="3"
                    class="code-input"
                    spellcheck="false"
                    placeholder=' ["customer-42"] '
                />
            </div>
            <template v-else-if="rule.kind === 'attribute'">
                <div class="field">
                    <label :for="`${id}-${rule.id}-attribute`">Attribute name</label>
                    <input
                        :id="`${id}-${rule.id}-attribute`"
                        v-model="rule.attribute"
                        :readonly="!canEdit || busy"
                        placeholder="plan"
                    />
                </div>
                <div class="field">
                    <label :for="`${id}-${rule.id}-equals`">Equals (JSON scalar)</label>
                    <input
                        :id="`${id}-${rule.id}-equals`"
                        v-model="rule.equals"
                        :readonly="!canEdit || busy"
                        placeholder='"pro"'
                    />
                </div>
            </template>
            <div v-else class="field">
                <label :for="`${id}-${rule.id}-percent`">Rollout percentage</label>
                <input
                    :id="`${id}-${rule.id}-percent`"
                    :value="rule.percentage"
                    @input="rule.percentage = ($event.target as HTMLInputElement).value"
                    :readonly="!canEdit || busy"
                    type="number"
                    min="0"
                    max="100"
                    step="1"
                />
            </div>
            <ValueInput
                v-model="rule.valueInput"
                :type="entry.type"
                :label="`Rule ${index + 1} value`"
                :disabled="!canEdit || busy"
                :rows="3"
            />
        </section>
        <div v-if="canEdit" class="rule-actions" aria-label="Add targeting rule">
            <button
                v-for="(label, kind) in ruleNames"
                :key="kind"
                type="button"
                class="button button--secondary button--compact"
                :disabled="busy"
                @click="entry.rules.push(editableRule(entry.type, kind))"
            >
                <Plus :size="14" />
                {{ label }}
            </button>
        </div>
    </div>
</template>
