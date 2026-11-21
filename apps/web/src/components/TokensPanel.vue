<script setup lang="ts">
    import { computed, nextTick, ref } from 'vue';
    import { useMutation, useQuery, useQueryClient } from '@tanstack/vue-query';
    import { Clipboard, KeyRound, Plus } from '@lucide/vue';
    import { api, dateLabel, errorMessage } from '../lib/api';
    import type { ClientToken, Environment, IssuedToken } from '../lib/api';
    import AppDialog from './AppDialog.vue';
    import EnvironmentBadge from './EnvironmentBadge.vue';
    import NoticeBox from './NoticeBox.vue';

    const props = defineProps<{ project: string; environment: Environment }>();
    const client = useQueryClient();
    const queryKey = ['tokens', props.project, props.environment];
    const { data, error, isPending, refetch } = useQuery({
        queryKey,
        queryFn: () => api.tokens(props.project, props.environment),
    });
    const createOpen = ref(false);
    const revokeOpen = ref(false);
    const name = ref('');
    const issued = ref<IssuedToken>();
    const selected = ref<ClientToken>();
    const revokeDescription = computed(() => {
        const name = selected.value?.name ?? 'this token';
        return `Applications using ${name} will lose read access to this environment.`;
    });
    const copied = ref(false);
    const manualCopy = ref(false);
    const tokenField = ref<HTMLInputElement>();
    const {
        mutate: create,
        isPending: creating,
        error: createError,
        reset: resetCreate,
    } = useMutation({
        mutationFn: async (tokenName: string) => {
            issued.value = await api.createToken(props.project, props.environment, tokenName);
            void client.invalidateQueries({ queryKey });
            await nextTick();
            tokenField.value?.focus();
        },
    });
    const {
        mutate: revoke,
        isPending: revoking,
        error: revokeError,
        reset: resetRevoke,
    } = useMutation({
        mutationFn: (id: ClientToken['id']) => api.revokeToken(props.project, props.environment, id),
        onSuccess: () => {
            revokeOpen.value = false;
            void client.invalidateQueries({ queryKey });
        },
    });
    function startCreate() {
        resetCreate();
        issued.value = undefined;
        copied.value = false;
        manualCopy.value = false;
        name.value = '';
        createOpen.value = true;
    }
    function startRevoke(token: ClientToken) {
        resetRevoke();
        selected.value = token;
        revokeOpen.value = true;
    }
    async function copyToken(token: string) {
        try {
            await navigator.clipboard.writeText(token);
            copied.value = true;
        } catch {
            tokenField.value?.focus();
            tokenField.value?.select();
            manualCopy.value = true;
        }
    }
</script>

<template>
    <section class="panel" aria-labelledby="tokens-heading">
        <header class="panel-heading row-between">
            <div>
                <h2 id="tokens-heading">Client read tokens</h2>
                <p>Grant applications read access to this environment.</p>
            </div>
            <button class="button button--primary" type="button" @click="startCreate">
                <Plus :size="17" />
                Create token
            </button>
        </header>
        <div v-if="isPending" class="panel-placeholder" role="status">Loading tokens…</div>
        <div v-else-if="error" class="panel-pad">
            <NoticeBox :message="errorMessage(error)" tone="error">
                <button class="text-link" @click="refetch()">Try again</button>
            </NoticeBox>
        </div>
        <div v-else-if="!data?.tokens.length" class="panel-placeholder">
            <KeyRound :size="24" />
            <strong>No read tokens yet</strong>
            <p>Create one to connect an application.</p>
        </div>
        <ul v-else class="record-list">
            <li v-for="token in data.tokens" :key="token.id" class="record-row">
                <div class="record-info">
                    <span class="record-icon"><KeyRound :size="17" /></span>
                    <div>
                        <strong>{{ token.name }}</strong>
                        <small>Created {{ dateLabel(token.createdAt) }}</small>
                        <small v-if="token.revokedAt">Revoked {{ dateLabel(token.revokedAt) }}</small>
                    </div>
                </div>
                <span v-if="token.revokedAt" class="neutral-label">Revoked</span>
                <button
                    v-else
                    type="button"
                    class="button button--secondary button--compact"
                    :aria-label="`Revoke ${token.name}`"
                    @click="startRevoke(token)"
                >
                    Revoke
                </button>
            </li>
        </ul>
    </section>
    <AppDialog
        v-model:open="createOpen"
        :title="issued ? 'Copy your read token' : 'Create read token'"
        :description="
            issued
                ? 'This is the only time Flipkiln will show the secret.'
                : 'This token can read published configuration in this environment.'
        "
        :busy="creating"
        :protect="!!issued"
        @update:open="!$event && (issued = undefined)"
    >
        <div v-if="issued" class="form-stack">
            <div class="token-copy">
                <div class="field">
                    <label for="issued-token">Read token</label>
                    <input
                        id="issued-token"
                        ref="tokenField"
                        :value="issued.token"
                        class="code-input"
                        readonly
                        @focus="($event.target as HTMLInputElement).select()"
                    />
                </div>
                <button class="button button--secondary" @click="copyToken(issued.token)">
                    <Clipboard :size="17" />
                    {{ copied ? 'Copied' : 'Copy' }}
                </button>
            </div>
            <p v-if="manualCopy" role="status" class="muted">
                The token is selected. Copy it with your keyboard or browser menu.
            </p>
        </div>
        <form v-else id="create-token" class="form-stack" @submit.prevent="create(name.trim())">
            <div><EnvironmentBadge :environment="environment" /></div>
            <div class="field">
                <label for="token-name">Token name</label>
                <input
                    id="token-name"
                    v-model="name"
                    required
                    maxlength="100"
                    placeholder="Checkout service"
                    :disabled="creating"
                />
            </div>
            <NoticeBox v-if="createError" :message="errorMessage(createError)" tone="error" />
        </form>
        <template #actions>
            <button
                v-if="issued"
                type="button"
                class="button button--primary"
                @click="
                    issued = undefined;
                    createOpen = false;
                "
            >
                Done
            </button>
            <template v-else>
                <button
                    class="button button--secondary"
                    :disabled="creating"
                    @click="createOpen = false"
                >
                    Cancel
                </button>
                <button
                    class="button button--primary"
                    type="submit"
                    form="create-token"
                    :disabled="creating || !name.trim()"
                >
                    Create token
                </button>
            </template>
        </template>
    </AppDialog>
    <AppDialog
        v-model:open="revokeOpen"
        title="Revoke read token"
        :description="revokeDescription"
        :busy="revoking"
    >
        <EnvironmentBadge :environment="environment" />
        <NoticeBox v-if="revokeError" :message="errorMessage(revokeError)" tone="error" />
        <template #actions>
            <button class="button button--secondary" :disabled="revoking" @click="revokeOpen = false">
                Cancel
            </button>
            <button
                class="button button--danger"
                :disabled="revoking"
                @click="selected && revoke(selected.id)"
            >
                Revoke token
            </button>
        </template>
    </AppDialog>
</template>
