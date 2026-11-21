<script setup lang="ts">
    import { ref, useId, watch } from 'vue';
    import { CirclePlay, FlaskConical } from '@lucide/vue';
    import type { EvaluationResult, RequestContext } from '../lib/api';
    import { demoContext } from '../lib/demo';
    import { displayValue } from '../lib/editor';

    defineProps<{ busy: boolean; result?: EvaluationResult }>();
    const emit = defineEmits<{ evaluate: [context: RequestContext]; reset: [] }>();
    const id = useId();
    const subject = ref('');
    const attributes = ref('{}');
    const error = ref('');
    const reasonNames = {
        disabled: 'Targeting disabled',
        rule_match: 'Rule matched',
        default: 'Default value',
        unknown_key: 'Unknown key',
        type_mismatch: 'Type mismatch',
    };

    watch([subject, attributes], () => {
        error.value = '';
        emit('reset');
    });

    function useExample() {
        subject.value = demoContext.id ?? '';
        attributes.value = JSON.stringify(demoContext.attributes, null, 2);
    }

    function isValidAttributes(value: unknown): value is NonNullable<RequestContext['attributes']> {
        if (value === null || typeof value !== 'object' || Array.isArray(value)) return false;
        return Object.values(value).every((attribute: unknown) => {
            if (typeof attribute === 'number') {
                return (
                    Number.isFinite(attribute) &&
                    (!Number.isInteger(attribute) || Number.isSafeInteger(attribute))
                );
            }
            return (
                attribute === null || typeof attribute === 'string' || typeof attribute === 'boolean'
            );
        });
    }

    function submit() {
        error.value = '';
        let parsed: unknown;
        try {
            parsed = JSON.parse(attributes.value);
        } catch {
            error.value = 'Enter valid JSON attributes.';
            return;
        }
        if (!isValidAttributes(parsed)) {
            error.value =
                'Use an object with strings, safe numbers, booleans, or null as attribute values.';
            return;
        }
        emit('evaluate', { id: subject.value || undefined, attributes: parsed });
    }
</script>

<template>
    <form class="form-stack" @submit.prevent="submit">
        <div class="field">
            <label :for="`${id}-subject`">User ID (optional)</label>
            <input :id="`${id}-subject`" v-model="subject" placeholder="customer-42" :disabled="busy" />
        </div>
        <div class="field">
            <label :for="`${id}-attributes`">Context attributes</label>
            <textarea
                :id="`${id}-attributes`"
                v-model="attributes"
                class="code-input"
                rows="4"
                spellcheck="false"
                :disabled="busy"
                :aria-invalid="error ? true : undefined"
                :aria-describedby="error ? `${id}-error` : undefined"
            />
            <p v-if="error" :id="`${id}-error`" class="field-error" role="alert">{{ error }}</p>
        </div>
        <div class="stack-actions">
            <button type="submit" class="button button--secondary" :disabled="busy">
                <CirclePlay :size="16" />
                Evaluate context
            </button>
            <button type="button" class="button button--secondary" :disabled="busy" @click="useExample">
                Use example context
            </button>
        </div>
        <div aria-live="polite">
            <div v-if="result" class="published-box">
                <div class="published-label">
                    <FlaskConical :size="15" />
                    <strong>{{ reasonNames[result.reason] }}</strong>
                </div>
                <pre>{{ displayValue(result.value, true) }}</pre>
                <code v-if="result.ruleId">{{ result.ruleId }}</code>
            </div>
        </div>
    </form>
</template>
