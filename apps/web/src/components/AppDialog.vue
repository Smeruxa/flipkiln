<script setup lang="ts">
    import { X } from '@lucide/vue';
    import { nextTick, ref, watch } from 'vue';
    import {
        DialogClose,
        DialogContent,
        DialogDescription,
        DialogOverlay,
        DialogPortal,
        DialogRoot,
        DialogTitle,
    } from 'reka-ui';

    const open = defineModel<boolean>('open', { required: true });
    const props = defineProps<{
        title: string;
        description: string;
        busy?: boolean;
        wide?: boolean;
        protect?: boolean;
        returnFocus?: () => HTMLElement | null;
    }>();

    const heading = ref<HTMLElement>();
    let opener: HTMLElement | null = null;
    watch(
        open,
        (visible) => {
            if (visible)
                opener = document.activeElement instanceof HTMLElement ? document.activeElement : null;
        },
        { flush: 'sync', immediate: true },
    );
    function closeGuard(event: Event) {
        if (props.busy || props.protect) event.preventDefault();
    }
    function focusOnOpen(event: Event) {
        if (!props.wide) return;
        event.preventDefault();
        void nextTick(() => heading.value?.focus());
    }
    function focusOnClose(event: Event) {
        const target = props.returnFocus ? props.returnFocus() : opener;
        if (!target?.isConnected || target.matches(':disabled')) return;
        event.preventDefault();
        void nextTick(() => target.focus());
    }
</script>

<template>
    <DialogRoot v-model:open="open">
        <DialogPortal>
            <DialogOverlay class="dialog-overlay" />
            <DialogContent
                class="dialog-panel"
                :class="{ 'dialog-panel--wide': wide }"
                @escape-key-down="closeGuard"
                @interact-outside="closeGuard"
                @open-auto-focus="focusOnOpen"
                @close-auto-focus="focusOnClose"
            >
                <div class="dialog-head">
                    <div>
                        <DialogTitle class="dialog-title" as-child>
                            <h2 ref="heading" tabindex="-1">{{ title }}</h2>
                        </DialogTitle>
                        <DialogDescription class="dialog-description">
                            {{ description }}
                        </DialogDescription>
                    </div>
                    <DialogClose as-child>
                        <button
                            type="button"
                            class="button icon-button"
                            aria-label="Close dialog"
                            :disabled="busy"
                        >
                            <X :size="19" />
                        </button>
                    </DialogClose>
                </div>
                <div class="dialog-body"><slot /></div>
                <div v-if="$slots.actions" class="dialog-actions"><slot name="actions" /></div>
            </DialogContent>
        </DialogPortal>
    </DialogRoot>
</template>
