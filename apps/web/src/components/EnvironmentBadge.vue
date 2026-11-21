<script setup lang="ts">
    import { FlaskConical, ShieldAlert } from '@lucide/vue';
    import { environmentLabel } from '../lib/api';
    import type { Environment } from '../lib/api';

    defineProps<{ environment: Environment }>();
</script>

<template>
    <span class="env-badge" :class="`env-badge--${environment}`">
        <ShieldAlert v-if="environment === 'production'" :size="14" />
        <FlaskConical v-else :size="14" />
        {{ environmentLabel[environment] }}
    </span>
</template>
