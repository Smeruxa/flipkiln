<script setup lang="ts">
    import { ref } from 'vue';
    import { useMutation, useQueryClient } from '@tanstack/vue-query';
    import { useRouter } from 'vue-router';
    import { Braces, Layers3, Radio } from '@lucide/vue';
    import { api, errorMessage, notifyAuthChange } from '../lib/api';
    import BrandMark from '../components/BrandMark.vue';
    import NoticeBox from '../components/NoticeBox.vue';

    const username = ref('');
    const password = ref('');
    const router = useRouter();
    const queryClient = useQueryClient();
    const {
        mutate: signIn,
        error,
        isPending,
    } = useMutation({
        mutationFn: api.login,
        onSuccess: async (session) => {
            notifyAuthChange();
            queryClient.clear();
            queryClient.setQueryData(['session'], session);
            password.value = '';
            const redirect = router.currentRoute.value.query.redirect;
            await router.replace(
                typeof redirect === 'string' && redirect.startsWith('/projects/')
                    ? redirect
                    : { name: 'home' },
            );
        },
    });
</script>

<template>
    <main class="auth-page">
        <div class="auth-main">
            <div class="auth-brand"><BrandMark /></div>
            <div class="auth-content">
                <h1>Sign in to Flipkiln</h1>
                <p>Manage flags, remote configuration, and releases.</p>
                <form
                    class="form-stack"
                    @submit.prevent="signIn({ username: username.trim(), password })"
                >
                    <div class="field">
                        <label for="username">Username</label>
                        <input
                            id="username"
                            v-model="username"
                            autocomplete="username"
                            required
                            :disabled="isPending"
                        />
                    </div>
                    <div class="field">
                        <label for="password">Password</label>
                        <input
                            id="password"
                            v-model="password"
                            type="password"
                            autocomplete="current-password"
                            required
                            :disabled="isPending"
                        />
                    </div>
                    <NoticeBox v-if="error" :message="errorMessage(error)" tone="error" />
                    <button class="button button--primary" :disabled="isPending">
                        {{ isPending ? 'Signing in…' : 'Sign in' }}
                    </button>
                </form>
                <RouterLink :to="{ name: 'demo' }" class="text-link">Open checkout demo</RouterLink>
            </div>
        </div>
        <div class="auth-art" aria-hidden="true">
            <div class="auth-art-pattern"></div>
            <div class="auth-sheet auth-sheet--back">
                <Layers3 :size="36" />
                <code>snapshot</code>
            </div>
            <div class="auth-sheet auth-sheet--mid">
                <Braces :size="36" />
                <code>draft</code>
            </div>
            <div class="auth-sheet auth-sheet--front">
                <Radio :size="36" />
                <code>release</code>
            </div>
        </div>
    </main>
</template>
