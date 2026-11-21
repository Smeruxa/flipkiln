<script setup lang="ts">
    import { CircleAlert, CircleCheck, Info } from '@lucide/vue';
    import type { Issue } from '../lib/api';

    withDefaults(
        defineProps<{ message: string; tone?: 'error' | 'success' | 'info'; issues?: Issue[] }>(),
        {
            tone: 'info',
            issues: () => [],
        },
    );
</script>

<template>
    <div class="notice" :class="`notice--${tone}`" :role="tone === 'error' ? 'alert' : 'status'">
        <CircleAlert v-if="tone === 'error'" :size="18" />
        <CircleCheck v-else-if="tone === 'success'" :size="18" />
        <Info v-else :size="18" />
        <div>
            <div>{{ message }}</div>
            <ul v-if="issues.length">
                <li v-for="(issue, index) in issues" :key="`${issue.path}-${index}`">
                    <code>{{ issue.path || '/' }}</code>
                    · {{ issue.message }}
                </li>
            </ul>
            <slot />
        </div>
    </div>
</template>
