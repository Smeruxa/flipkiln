<script setup lang="ts">
    import { ref } from 'vue';
    import { useMutation, useQuery, useQueryClient } from '@tanstack/vue-query';
    import { CircleUserRound, UserPlus } from '@lucide/vue';
    import { api, errorMessage, roles } from '../lib/api';
    import type { Member, Role } from '../lib/api';
    import AppDialog from './AppDialog.vue';
    import NoticeBox from './NoticeBox.vue';

    const props = defineProps<{ project: string }>();
    const client = useQueryClient();
    const queryKey = ['members', props.project];
    const { data, error, isPending, refetch } = useQuery({
        queryKey,
        queryFn: () => api.members(props.project),
    });
    const editOpen = ref(false);
    const username = ref('');
    const role = ref<Role | ''>('viewer');
    const isExisting = ref(false);
    const descriptions = {
        viewer: 'View configuration and release history.',
        editor: 'Edit drafts and validate entries.',
        publisher: 'Publish and roll back releases.',
        admin: 'Manage read tokens and project access.',
    };
    const {
        mutate: save,
        isPending: saving,
        reset,
        error: saveError,
    } = useMutation({
        mutationFn: () => api.setMember(props.project, username.value.trim(), role.value || null),
        onSuccess: async () => {
            editOpen.value = false;
            await Promise.all([
                client.invalidateQueries({ queryKey }),
                client.invalidateQueries({ queryKey: ['projects'] }),
            ]);
        },
    });
    function select(member?: Member) {
        reset();
        isExisting.value = !!member;
        username.value = member?.username ?? '';
        role.value = member?.role ?? 'viewer';
        editOpen.value = true;
    }
</script>

<template>
    <section class="panel" aria-labelledby="members-heading">
        <header class="panel-heading row-between">
            <div>
                <h2 id="members-heading">Project members</h2>
                <p>Roles apply to both environments and include permissions from lower roles.</p>
            </div>
            <button class="button button--primary" @click="select()">
                <UserPlus :size="17" />
                Add member
            </button>
        </header>
        <div v-if="isPending" class="panel-placeholder" role="status">Loading members…</div>
        <div v-else-if="error" class="panel-pad">
            <NoticeBox :message="errorMessage(error)" tone="error">
                <button class="text-link" @click="refetch()">Try again</button>
            </NoticeBox>
        </div>
        <div v-else-if="!data?.members.length" class="panel-placeholder">No members assigned.</div>
        <ul v-else class="record-list">
            <li v-for="member in data.members" :key="member.username" class="record-row">
                <div class="record-info">
                    <span class="record-icon"><CircleUserRound :size="18" /></span>
                    <div>
                        <strong>{{ member.username }}</strong>
                        <small class="capitalize">{{ member.role }}</small>
                    </div>
                </div>
                <button
                    class="button button--secondary button--compact"
                    :aria-label="`Edit role for ${member.username}`"
                    @click="select(member)"
                >
                    Edit role
                </button>
            </li>
        </ul>
    </section>
    <AppDialog
        v-model:open="editOpen"
        :title="isExisting ? 'Edit project role' : 'Add project member'"
        description="Grant access to an existing Flipkiln account."
        :busy="saving"
    >
        <form id="set-member" class="form-stack" @submit.prevent="save()">
            <div class="field">
                <label for="member-name">Username</label>
                <input
                    id="member-name"
                    v-model="username"
                    required
                    :readonly="isExisting"
                    :disabled="saving"
                />
            </div>
            <div class="field">
                <label for="member-role">Role</label>
                <select id="member-role" v-model="role" :disabled="saving">
                    <option v-for="item in roles" :key="item" :value="item">
                        {{ item[0].toUpperCase() + item.slice(1) }}
                    </option>
                    <option v-if="isExisting" value="">Remove access</option>
                </select>
                <p>{{ role ? descriptions[role] : 'This account will lose access to the project.' }}</p>
            </div>
            <NoticeBox v-if="saveError" :message="errorMessage(saveError)" tone="error" />
        </form>
        <template #actions>
            <button class="button button--secondary" :disabled="saving" @click="editOpen = false">
                Cancel
            </button>
            <button type="submit" form="set-member" class="button button--primary" :disabled="saving">
                Save role
            </button>
        </template>
    </AppDialog>
</template>
