import { createApp } from 'vue';
import { VueQueryPlugin, QueryClient } from '@tanstack/vue-query';
import { createRouter, createWebHistory } from 'vue-router';
import '@fontsource-variable/instrument-sans';
import '@fontsource/ibm-plex-mono/400.css';
import '@fontsource/ibm-plex-mono/500.css';
import './styles/base.scss';
import './styles/workspace.scss';
import './styles/editor.scss';
import './styles/dialog.scss';
import './styles/auth.scss';
import './styles/demo.scss';
import { ApiError, api, authChangeKey } from './lib/api';
import App from './App.vue';
import DemoView from './views/DemoView.vue';
import LoginView from './views/LoginView.vue';
import WorkspaceView from './views/WorkspaceView.vue';

const queryClient = new QueryClient({
    defaultOptions: { queries: { retry: false, refetchOnWindowFocus: false, staleTime: 5000 } },
});

const router = createRouter({
    history: createWebHistory(),
    routes: [
        { path: '/login', name: 'login', component: LoginView, meta: { title: 'Sign in' } },
        { path: '/demo', name: 'demo', component: DemoView, meta: { title: 'Checkout demo' } },
        { path: '/:pathMatch(.*)*', component: WorkspaceView, children: [] },
        { path: '/', name: 'home', component: WorkspaceView },
        { path: '/projects/:project/:environment/:tab', name: 'workspace', component: WorkspaceView },
    ],
});

router.afterEach((route) => {
    document.title = route.meta.title ? `${route.meta.title} · Flipkiln` : 'Flipkiln Console';
});
function resetWorkspace() {
    if (router.currentRoute.value.name === 'demo') return;
    queryClient.clear();
    void router.replace({ name: 'login' });
}
window.addEventListener('flipkiln-session-expired', resetWorkspace);
window.addEventListener('storage', (event) => {
    if (event.key === authChangeKey) resetWorkspace();
});
window.addEventListener('focus', async () => {
    if (router.currentRoute.value.name !== 'workspace') return;
    const current = queryClient.getQueryData<{ username: string }>(['session']);
    if (!current) return;
    try {
        const session = await api.session();
        if (session.username !== current.username) resetWorkspace();
    } catch (error) {
        if (error instanceof ApiError && error.status === 401) resetWorkspace();
    }
});

createApp(App).use(VueQueryPlugin, { queryClient }).use(router).mount('#app');
