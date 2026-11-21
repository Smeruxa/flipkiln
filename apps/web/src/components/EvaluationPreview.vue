<script setup lang="ts">
    import { nextTick, ref, useId, watch } from 'vue';
    import { CirclePlay, FlaskConical } from '@lucide/vue';
    import type { EvaluationResult, RequestContext } from '../lib/api';
    import { demoContext } from '../lib/demo';
    import { displayValue } from '../lib/editor';
    import { isJsonScalar, isRecord } from '../lib/snapshot';

    defineProps<{ busy: boolean; result?: EvaluationResult }>();
    const emit = defineEmits<{ evaluate: [context: RequestContext]; reset: [] }>();
    const id = useId();
    const subject = ref('');
    const subjectInput = ref<HTMLInputElement | null>(null);
    const attributes = ref('{}');
    const error = ref('');
    const idError = ref('');
    const reasonNames = {
        disabled: 'Targeting disabled',
        rule_match: 'Rule matched',
        default: 'Default value',
        unknown_key: 'Unknown key',
        type_mismatch: 'Type mismatch',
    };

    watch([subject, attributes], ([idValue, input], [previousId, previousInput]) => {
        if (idValue !== previousId) idError.value = '';
        if (input !== previousInput) error.value = '';
        emit('reset');
    });

    function useExample() {
        subject.value = JSON.stringify(demoContext.id);
        attributes.value = JSON.stringify(demoContext.attributes, null, 2);
    }

    function isValidAttributes(value: unknown): value is NonNullable<RequestContext['attributes']> {
        return isRecord(value) && Object.values(value).every(isJsonScalar);
    }

    function showIdError(message: string) {
        idError.value = message;
        void nextTick(() => subjectInput.value?.focus());
    }

    function submit() {
        error.value = '';
        idError.value = '';
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
        let idValue: unknown;
        try {
            idValue = subject.value === '' ? undefined : JSON.parse(subject.value);
        } catch {
            showIdError('Put the user ID in double quotes.');
            return;
        }
        if (idValue !== undefined && (typeof idValue !== 'string' || idValue === '')) {
            showIdError('Enter a nonempty JSON string or leave the field blank.');
            return;
        }
        emit('evaluate', { id: idValue, attributes: parsed });
    }
</script>

<template>
    <form class="form-stack" @submit.prevent="submit">
        <div class="field">
            <label :for="`${id}-subject`">User ID (optional)</label>
            <input
                :id="`${id}-subject`"
                ref="subjectInput"
                v-model="subject"
                :aria-invalid="idError ? true : undefined"
                :aria-describedby="idError ? `${id}-id-error ${id}-id-help` : `${id}-id-help`"
                placeholder='"customer-42"'
                :disabled="busy"
            />
            <p v-if="idError" :id="`${id}-id-error`" class="field-error" role="alert">{{ idError }}</p>
            <p :id="`${id}-id-help`">
                Use a JSON string such as
                <code>"customer-42"</code>
                . Leave blank to omit the user ID.
            </p>
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
