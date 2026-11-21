<script setup lang="ts">
    import { computed, ref, watchEffect } from 'vue';
    import { useMutation, useQuery, useQueryClient } from '@tanstack/vue-query';
    import { useRoute, useRouter } from 'vue-router';
    import {
        AppWindow,
        ChevronDown,
        ChevronRight,
        CircleUserRound,
        FolderPlus,
        GitBranch,
        History,
        KeyRound,
        LogOut,
        SlidersHorizontal,
        UsersRound,
    } from '@lucide/vue';
    import { ApiError, api, environmentLabel, errorMessage, notifyAuthChange } from '../lib/api';
    import type { Environment } from '../lib/api';
    import AppDialog from '../components/AppDialog.vue';
    import BrandMark from '../components/BrandMark.vue';
    import EnvironmentBadge from '../components/EnvironmentBadge.vue';
    import NoticeBox from '../components/NoticeBox.vue';
    import WorkspaceEnvironment from '../components/WorkspaceEnvironment.vue';

    const route = useRoute();
    const router = useRouter();
    const client = useQueryClient();
    const {
        data: session,
        isPending: sessionLoading,
        error: sessionError,
        refetch: reloadSession,
    } = useQuery({ queryKey: ['session'], queryFn: api.session });
    const {
        data: projectData,
        isPending: projectsLoading,
        error: projectsError,
        refetch: reloadProjects,
    } = useQuery({
        queryKey: ['projects'],
        queryFn: api.projects,
        enabled: computed(() => !!session.value),
    });
    const projects = computed(() => projectData.value?.projects ?? []);
    const project = computed(() => projects.value.find((item) => item.id === route.params.project));
    const environment = computed<Environment>(() =>
        route.params.environment === 'production' ? 'production' : 'development',
    );
    const knownEnvironment = computed(() =>
        project.value?.environments.some((value) => value === route.params.environment),
    );
    const tabs = [
        { id: 'entries', label: 'Configuration', short: 'Config', icon: SlidersHorizontal },
        { id: 'releases', label: 'Releases', short: 'Releases', icon: History },
        { id: 'tokens', label: 'Read tokens', short: 'Tokens', icon: KeyRound },
        { id: 'members', label: 'Access', short: 'Access', icon: UsersRound },
    ] as const;
    const section = computed(() => tabs.find((item) => item.id === route.params.tab) ?? tabs[0]);
    const allowedTabs = computed(() => (project.value?.role === 'admin' ? tabs : tabs.slice(0, 2)));
    const canCreate = computed(() => session.value?.username === 'admin');
    const createOpen = ref(false);
    const newName = ref('');
    const newId = ref('');
    const {
        mutate: signOut,
        isPending: loggingOut,
        error: logoutError,
    } = useMutation({
        mutationFn: api.logout,
        onSuccess: async () => {
            notifyAuthChange();
            client.clear();
            await router.replace({ name: 'login' });
        },
    });
    const {
        mutate: create,
        isPending: creating,
        error: createError,
        reset: resetCreate,
    } = useMutation({
        mutationFn: api.createProject,
        onSuccess: async (created) => {
            await client.invalidateQueries({ queryKey: ['projects'] });
            createOpen.value = false;
            await router.push(target('entries', created.id, 'development'));
        },
    });

    watchEffect(() => {
        if (sessionError.value instanceof ApiError && sessionError.value.status === 401) {
            void router.replace({
                name: 'login',
                query: route.name === 'workspace' ? { redirect: route.fullPath } : {},
            });
        }
        if (projects.value.length && route.name !== 'workspace') {
            const initialProject =
                projects.value.find((item) => item.id === 'workshop') ?? projects.value[0];
            void router.replace(target('entries', initialProject.id, initialProject.environments[0]));
        }
    });

    function target(tab: string, id = project.value?.id ?? '', env = environment.value) {
        return { name: 'workspace', params: { project: id, environment: env, tab } };
    }
    async function selectProject(event: Event) {
        const select = event.target as HTMLSelectElement;
        const denied = await router.push(target(section.value.id, select.value));
        if (denied && project.value) select.value = project.value.id;
    }
    async function selectEnvironment(event: Event) {
        const select = event.target as HTMLSelectElement;
        const requested = select.value as Environment;
        const denied = await router.push(target(section.value.id, project.value?.id, requested));
        if (denied) select.value = environment.value;
    }
    function openCreate() {
        resetCreate();
        newId.value = '';
        newName.value = '';
        createOpen.value = true;
    }
</script>

<template>
    <div
        v-if="sessionLoading || (sessionError instanceof ApiError && sessionError.status === 401)"
        class="full-state"
        role="status"
    >
        <BrandMark />
        <p>Opening Flipkiln…</p>
    </div>
    <div v-else-if="sessionError" class="full-state">
        <BrandMark />
        <NoticeBox :message="errorMessage(sessionError)" tone="error" />
        <button class="button button--secondary" @click="reloadSession()">Try again</button>
    </div>
    <div v-else class="shell">
        <a class="skip-link" href="#main-content">Skip to content</a>
        <aside class="sidebar">
            <div class="sidebar-top">
                <RouterLink class="wordmark-link" :to="{ name: 'home' }"><BrandMark /></RouterLink>
                <button
                    class="side-signout button icon-button"
                    type="button"
                    aria-label="Sign out"
                    :disabled="loggingOut"
                    @click="signOut()"
                >
                    <LogOut :size="17" />
                </button>
            </div>
            <div v-if="project" class="side-project select-wrap">
                <GitBranch :size="17" aria-hidden="true" />
                <select aria-label="Project" :value="project.id" @change="selectProject">
                    <option v-for="item in projects" :key="item.id" :value="item.id">
                        {{ item.name }}
                    </option>
                </select>
                <ChevronDown :size="14" aria-hidden="true" />
            </div>
            <div class="side-menu">
                <div class="menu-caption">Workspace</div>
                <nav v-if="project" aria-label="Workspace">
                    <RouterLink
                        v-for="item in allowedTabs"
                        :key="item.id"
                        :to="target(item.id)"
                        class="nav-item"
                        :class="{ 'nav-item--active': section.id === item.id }"
                        :aria-current="section.id === item.id ? 'page' : undefined"
                    >
                        <component :is="item.icon" :size="18" />
                        <span class="nav-label">{{ item.label }}</span>
                        <span class="nav-short">{{ item.short }}</span>
                    </RouterLink>
                </nav>
                <button
                    v-if="canCreate"
                    type="button"
                    class="nav-item new-project"
                    aria-label="New project"
                    @click="openCreate"
                >
                    <FolderPlus :size="18" />
                    <span>New project</span>
                </button>
            </div>
            <div class="sidebar-bottom">
                <div v-if="session" class="profile">
                    <span class="avatar"><CircleUserRound :size="18" /></span>
                    <div>
                        <strong>{{ session.username }}</strong>
                        <small>{{ project?.role ?? 'Signed in' }}</small>
                    </div>
                </div>
                <button
                    type="button"
                    class="side-signout-desktop button icon-button"
                    aria-label="Sign out"
                    :disabled="loggingOut"
                    @click="signOut()"
                >
                    <LogOut :size="17" />
                </button>
            </div>
        </aside>

        <main id="main-content" class="main" tabindex="-1">
            <div class="topbar">
                <div class="breadcrumbs">
                    <span>{{ project?.name ?? 'Workspace' }}</span>
                    <ChevronRight :size="15" />
                    <strong>{{ section.label }}</strong>
                </div>
                <div class="topbar-actions">
                    <div v-if="project" class="top-environment">
                        <EnvironmentBadge :environment="environment" />
                        <ChevronDown :size="14" />
                        <select
                            aria-label="Environment"
                            :value="environment"
                            @change="selectEnvironment"
                        >
                            <option v-for="item in project.environments" :key="item" :value="item">
                                {{ environmentLabel[item] }}
                            </option>
                        </select>
                    </div>
                    <RouterLink
                        class="button button--secondary demo-link"
                        :to="{ name: 'demo' }"
                        target="_blank"
                        rel="noopener"
                    >
                        <AppWindow :size="16" />
                        <span>Checkout demo</span>
                    </RouterLink>
                </div>
            </div>
            <div class="main-content">
                <NoticeBox v-if="logoutError" :message="errorMessage(logoutError)" tone="error" />
                <div v-if="projectsLoading" class="panel panel-placeholder" role="status">
                    Loading projects…
                </div>
                <NoticeBox
                    v-else-if="projectsError"
                    :message="errorMessage(projectsError)"
                    tone="error"
                >
                    <button class="text-link" @click="reloadProjects()">Try again</button>
                </NoticeBox>
                <section v-else-if="!projects.length" class="panel panel-placeholder">
                    <h1>No projects yet</h1>
                    <p>
                        {{
                            canCreate
                                ? 'Create a project to start managing configuration.'
                                : 'A project admin can grant you access.'
                        }}
                    </p>
                    <button
                        v-if="canCreate"
                        type="button"
                        class="button button--primary"
                        @click="openCreate"
                    >
                        Create project
                    </button>
                </section>
                <section
                    v-else-if="route.name === 'workspace' && !project"
                    class="panel panel-placeholder"
                >
                    <h1>Project unavailable</h1>
                    <p>Select a project you can access.</p>
                    <RouterLink class="button button--secondary" :to="{ name: 'home' }">
                        Choose a project
                    </RouterLink>
                </section>
                <section v-else-if="project && !knownEnvironment" class="panel panel-placeholder">
                    <h1>Environment not found</h1>
                    <p>Choose an environment for {{ project.name }}.</p>
                    <RouterLink
                        class="button button--secondary"
                        :to="target('entries', project.id, project.environments[0])"
                    >
                        Open configuration
                    </RouterLink>
                </section>
                <WorkspaceEnvironment
                    v-else-if="project"
                    :key="`${project.id}/${environment}`"
                    :project="project"
                    :environment="environment"
                    :section="section.id"
                />
            </div>
        </main>

        <AppDialog
            v-model:open="createOpen"
            title="Create project"
            description="Each project starts with separate development and production environments."
            :busy="creating"
        >
            <form
                id="create-project"
                class="form-stack"
                @submit.prevent="create({ id: newId.trim(), name: newName.trim() })"
            >
                <div class="field">
                    <label for="project-name">Project name</label>
                    <input id="project-name" v-model="newName" required :disabled="creating" />
                </div>
                <div class="field">
                    <label for="project-id">Project ID</label>
                    <input
                        id="project-id"
                        v-model="newId"
                        required
                        pattern="[a-z][a-z0-9\-]{0,62}"
                        maxlength="63"
                        placeholder="my-project"
                        :disabled="creating"
                    />
                    <p>Use lowercase letters, digits, or hyphens. Start with a letter.</p>
                </div>
                <NoticeBox v-if="createError" :message="errorMessage(createError)" tone="error" />
            </form>
            <template #actions>
                <button
                    class="button button--secondary"
                    :disabled="creating"
                    @click="createOpen = false"
                >
                    Cancel
                </button>
                <button
                    type="submit"
                    form="create-project"
                    class="button button--primary"
                    :disabled="creating"
                >
                    Create project
                </button>
            </template>
        </AppDialog>
    </div>
</template>
