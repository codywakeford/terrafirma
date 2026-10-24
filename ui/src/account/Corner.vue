<script setup lang="ts">
import { computed, ref } from "vue"
import { useBridge } from "../bridge/use"
import { useStore } from "../game/store"
import { Button } from "../kit"
import SocialPanel from "../social/SocialPanel.vue"
import AccountCard from "./AccountCard.vue"

/** The menu's top-right corner: friends and the account, each opening its panel below. */
const bridge = useBridge()
const store = useStore()
const open = ref<"friends" | "account" | null>(null)
const toggle = (panel: "friends" | "account") => (open.value = open.value === panel ? null : panel)

const LABELS = {
    signedOut: "Sign in",
    starting: "Signing in…",
    pending: "Signing in…",
    checking: "Signing in…",
    unreachable: "Offline",
} as const

const label = computed(() => {
    const account = store.account
    if (!account) return null
    return account.status === "signedIn" ? account.username : LABELS[account.status]
})

// What's waiting on the player: requests to answer and invites.
const waiting = computed(() => {
    const view = store.social?.view
    return view ? view.incoming.length + view.invites.length : 0
})
</script>

<template>
    <div v-if="store.account && label" class="corner">
        <div class="buttons">
            <Button v-if="store.social" size="sm" @click="toggle('friends')">
                Friends<span v-if="waiting" class="badge">{{ waiting }}</span>
            </Button>
            <Button size="sm" :variant="store.account.status === 'signedIn' ? 'plain' : 'primary'" @click="toggle('account')">
                <span class="chev">»</span> {{ label }}
            </Button>
        </div>
        <SocialPanel v-if="open === 'friends' && store.social" @close="open = null" />
        <AccountCard
            v-else-if="open === 'account'"
            class="card"
            :account="store.account"
            @sign-in="bridge.send('account.signIn', {})"
            @cancel="bridge.send('account.cancel', {})"
            @sign-out="bridge.send('account.signOut', {})"
            @retry="bridge.send('account.retry', {})"
            @open="bridge.send('open', { url: $event })"
        />
    </div>
</template>

<style scoped>
.corner { position: fixed; top: 20px; right: 20px; z-index: 3; display: flex; flex-direction: column; align-items: flex-end; gap: 10px; max-width: calc(100vw - 40px); }
.buttons { display: flex; gap: 8px; }
.chev { color: var(--accent-hi); }
.badge {
    min-width: 18px;
    padding: 0 4px;
    font-size: 11px;
    line-height: 18px;
    text-align: center;
    background: var(--accent);
    box-shadow: var(--raised);
}
.card { max-width: 100%; }
</style>
