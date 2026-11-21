<script setup lang="ts">
    import { SwitchRoot, SwitchThumb } from 'reka-ui';
    import { useId } from 'vue';
    import type { EntryType, Issue } from '../lib/api';

    const model = defineModel<string>({ required: true });
    defineProps<{
        type: EntryType;
        label: string;
        disabled?: boolean;
        rows?: number;
        issues?: Issue[];
    }>();
    const id = useId();
</script>

<template>
    <div class="field">
        <label :for="id">{{ label }}</label>
        <div v-if="type === 'boolean'" class="boolean-input">
            <SwitchRoot
                :id="id"
                :model-value="model === 'true'"
                :disabled="disabled"
                class="switch"
                @update:model-value="model = String($event)"
            >
                <SwitchThumb class="switch-thumb" />
            </SwitchRoot>
            <span>{{ model === 'true' ? 'On' : 'Off' }}</span>
        </div>
        <textarea
            v-else-if="type === 'json'"
            :id="id"
            v-model="model"
            class="code-input"
            :aria-invalid="issues?.length ? true : undefined"
            :aria-describedby="issues?.length ? `${id}-error` : undefined"
            :rows="rows ?? 5"
            :readonly="disabled"
            spellcheck="false"
        />
        <input
            v-else
            :id="id"
            :aria-invalid="issues?.length ? true : undefined"
            :aria-describedby="issues?.length ? `${id}-error` : undefined"
            :value="model"
            @input="model = ($event.target as HTMLInputElement).value"
            :type="type === 'number' ? 'number' : 'text'"
            :step="type === 'number' ? 'any' : undefined"
            :readonly="disabled"
        />
        <div v-if="issues?.length" :id="`${id}-error`" class="inline-errors">
            <div v-for="(issue, index) in issues" :key="index">
                <code v-if="issue.path">{{ issue.path }}</code>
                {{ issue.message }}
            </div>
        </div>
    </div>
</template>
