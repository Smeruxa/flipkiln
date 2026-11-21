<script setup lang="ts">
    import { computed, onUnmounted, ref, shallowRef } from 'vue';
    import { useRoute } from 'vue-router';
    import {
        ArrowLeft,
        ArrowRight,
        CircleCheck,
        CreditCard,
        FlaskConical,
        Link2,
        Radio,
        RefreshCw,
        ShoppingBag,
        Zap,
    } from '@lucide/vue';
    import { environmentLabel, errorMessage } from '../lib/api';
    import type { Environment, Snapshot } from '../lib/api';
    import { checkoutSettings, readSnapshot } from '../lib/demo';
    import NoticeBox from '../components/NoticeBox.vue';

    const route = useRoute();
    const project = ref(typeof route.query.project === 'string' ? route.query.project : 'workshop');
    const environment = ref<Environment>(
        route.query.environment === 'production' ? 'production' : 'development',
    );
    const token = ref('');
    const snapshot = shallowRef<Snapshot | null>(null);
    const connection = shallowRef<{ project: string; environment: Environment; token: string }>();
    const connecting = ref(false);
    const refreshing = ref(false);
    const error = ref('');
    const editing = ref(true);
    const paymentStep = ref(false);
    const settings = computed(() =>
        checkoutSettings(
            snapshot.value,
            connection.value?.project ?? '',
            connection.value?.environment ?? 'development',
        ),
    );
    const express = computed(() => settings.value.express && settings.value.expressLimit >= 1);
    const status = computed(() =>
        error.value && snapshot.value
            ? 'Using last published release'
            : snapshot.value
              ? 'Published configuration'
              : 'Using local defaults',
    );
    let etag: string | null = null;
    let timer: ReturnType<typeof setTimeout> | undefined;
    let controller: AbortController | undefined;

    function stop() {
        clearTimeout(timer);
        controller?.abort();
    }
    onUnmounted(stop);
    async function poll() {
        const current = connection.value;
        if (!current || refreshing.value) return;
        stop();
        const pending = new AbortController();
        controller = pending;
        refreshing.value = true;
        try {
            const next = await readSnapshot(
                current,
                etag,
                AbortSignal.any([pending.signal, AbortSignal.timeout(5000)]),
            );
            if (pending.signal.aborted) return;
            if (next) {
                if (next.snapshot.revision !== snapshot.value?.revision) paymentStep.value = false;
                snapshot.value = next.snapshot;
                etag = next.etag;
            } else if (!snapshot.value) {
                throw new Error('The server did not provide a starting snapshot.');
            }
            error.value = '';
            if (connecting.value) editing.value = false;
        } catch (failure) {
            if (!pending.signal.aborted)
                error.value =
                    failure instanceof TypeError || failure instanceof DOMException
                        ? 'Flipkiln is temporarily unreachable. The demo will retry automatically.'
                        : errorMessage(failure);
        } finally {
            if (!pending.signal.aborted) {
                refreshing.value = false;
                connecting.value = false;
                timer = setTimeout(() => void poll(), 3000);
            }
        }
    }
    function connect() {
        stop();
        snapshot.value = null;
        etag = null;
        paymentStep.value = false;
        error.value = '';
        refreshing.value = false;
        connecting.value = true;
        connection.value = {
            project: project.value.trim(),
            environment: environment.value,
            token: token.value.trim(),
        };
        token.value = '';
        void poll();
    }
</script>

<template>
    <main class="demo-page">
        <header class="demo-topbar">
            <RouterLink :to="{ name: 'home' }" class="text-link">
                <ArrowLeft :size="17" />
                Flipkiln console
            </RouterLink>
            <span>
                <FlaskConical :size="16" />
                Checkout demo
            </span>
        </header>
        <div class="demo-wrap">
            <div class="demo-status" :class="{ 'demo-status--stale': error && snapshot }">
                <div class="demo-status-info" aria-live="polite">
                    <Radio :size="18" />
                    <div>
                        <strong>{{ status }}</strong>
                        <small v-if="snapshot">
                            Revision {{ snapshot.revision }} · {{ connection?.project }} /
                            {{ environmentLabel[connection!.environment] }}
                        </small>
                        <small v-else>Connect a scoped read token to follow releases.</small>
                    </div>
                </div>
                <div v-if="connection && !editing" class="stack-actions">
                    <button
                        class="button button--secondary button--compact"
                        :disabled="refreshing"
                        @click="poll"
                    >
                        <RefreshCw :size="15" />
                        Refresh now
                    </button>
                    <button class="button button--secondary button--compact" @click="editing = true">
                        Connection
                    </button>
                </div>
            </div>
            <form v-if="editing" class="demo-connection" @submit.prevent="connect">
                <div class="field">
                    <label for="demo-project">Project</label>
                    <input
                        id="demo-project"
                        v-model="project"
                        required
                        maxlength="63"
                        autocomplete="off"
                        :disabled="connecting"
                    />
                </div>
                <div class="field">
                    <label for="demo-environment">Environment</label>
                    <select id="demo-environment" v-model="environment" :disabled="connecting">
                        <option value="development">Development</option>
                        <option value="production">Production</option>
                    </select>
                </div>
                <div class="field">
                    <label for="demo-token">Read token</label>
                    <input
                        id="demo-token"
                        v-model="token"
                        type="password"
                        placeholder="Paste a client read token"
                        minlength="20"
                        maxlength="256"
                        required
                        autocomplete="new-password"
                        :disabled="connecting"
                    />
                </div>
                <div class="stack-actions">
                    <button class="button button--primary" :disabled="connecting">
                        <Link2 :size="16" />
                        {{ connecting ? 'Connecting…' : 'Connect demo' }}
                    </button>
                    <button
                        v-if="snapshot"
                        class="button button--secondary"
                        type="button"
                        @click="editing = false"
                    >
                        Cancel
                    </button>
                </div>
            </form>
            <NoticeBox v-if="error" :message="error" tone="error" />
            <div class="checkout-grid" :class="{ 'checkout-grid--express': express }">
                <div class="checkout-product">
                    <img src="/field-journal.jpg" alt="Two sage linen journals and a ceramic pen" />
                    <div class="product-caption">
                        <span>Stationery</span>
                        <strong>Field Journal Set</strong>
                    </div>
                </div>
                <section class="order-card" aria-labelledby="checkout-heading">
                    <span class="checkout-mode">
                        <Zap v-if="express" :size="16" />
                        <ShoppingBag v-else :size="16" />
                        {{ express ? 'Express checkout' : 'Standard checkout' }}
                    </span>
                    <h1 id="checkout-heading">{{ settings.headline }}</h1>
                    <div class="order-item">
                        <span>
                            Field Journal Set
                            <small>Qty 1</small>
                        </span>
                        <strong>$28.00</strong>
                    </div>
                    <div class="order-item muted">
                        <span>Shipping</span>
                        <span>{{ express ? 'Included' : '$5.00' }}</span>
                    </div>
                    <div class="order-total">
                        <strong>Total</strong>
                        <strong>{{ express ? '$28.00' : '$33.00' }}</strong>
                    </div>
                    <div v-if="paymentStep" class="checkout-step" role="status">
                        <CircleCheck :size="19" />
                        {{ express ? 'Express payment ready' : 'Payment step ready' }}
                    </div>
                    <button v-else class="button checkout-button" @click="paymentStep = true">
                        <CreditCard v-if="express" :size="18" />
                        {{ express ? 'Pay with express checkout' : settings.button }}
                        <ArrowRight v-if="!express" :size="17" />
                    </button>
                    <p>
                        Demo cart · Up to {{ settings.maxItems }}
                        {{ settings.maxItems === 1 ? 'item' : 'items' }} per order
                    </p>
                </section>
            </div>
        </div>
    </main>
</template>
