<script setup lang="ts">
import { computed, ref } from "vue"
import type { Account, Friend, Player, Social } from "../bridge"
import { Button, Field, Notice, Toggle } from "../kit"
import { byPresence, presenceDot, presenceLine } from "../social/presence"

type Page = "friends" | "activity" | "account" | "add"
const props = defineProps<{ account: Account; social: Social | null; error: string | null; busy: boolean; joining: string | null; initialPage?: Page }>()
const emit = defineEmits<{
    signIn: []; cancel: []; signOut: []; retry: []; openUrl: [url: string]
    request: [username: string]; accept: [id: string]; remove: [id: string]
    invite: [id: string]; dismiss: [id: string]; openWorld: [open: boolean]; join: [player: Player]
}>()

const page = ref<Page>(props.initialPage ?? "friends")
const username = ref("")
const confirming = ref<string | null>(null)
const view = computed(() => props.social?.view)
const friends = computed(() => [...(view.value?.friends ?? [])].sort(byPresence))
const online = computed(() => friends.value.filter((friend) => friend.presence).length)
const waiting = computed(() => (view.value?.incoming.length ?? 0) + (view.value?.invites.length ?? 0))
const activity = computed(() => waiting.value + (view.value?.outgoing.length ?? 0))
const displayName = computed(() => {
    if (props.account.status !== "signedIn") return ""
    const name = props.account.name?.trim()
    return name && name.toLowerCase() !== props.account.username.toLowerCase() ? name : props.account.username
})
const open = computed({
    get: () => props.social?.me.open ?? false,
    set: (value: boolean) => emit("openWorld", value),
})
const inMenu = computed(() => props.social?.me.state === "menu")
const myPresence = computed(() => {
    const me = props.social?.me
    if (!me) return "Connecting to ArcLabs"
    if (me.state === "menu") return "Main menu"
    if (me.state === "hosting") return "Hosting"
    return "Playing"
})
const joinable = (friend: Friend) => friend.presence?.state === "hosting" && friend.presence.open
const invitable = (friend: Friend) =>
    props.social?.joinable && friend.presence !== null && friend.presence.world !== props.social.me.world

function add() {
    const name = username.value.trim()
    if (!name || props.busy) return
    emit("request", name)
    username.value = ""
    page.value = "friends"
}
function remove(friend: Friend) {
    if (confirming.value !== friend.id) {
        confirming.value = friend.id
        return
    }
    confirming.value = null
    emit("remove", friend.id)
}
</script>

<template>
    <section class="dropdown" :class="{ connected: account.status === 'signedIn' }" aria-label="Account and friends">
        <template v-if="account.status === 'signedIn'">
            <header class="identity">
                <div class="avatar" aria-hidden="true">{{ account.username.slice(0, 1).toUpperCase() }}</div>
                <div class="identity-text">
                    <strong>{{ displayName }}</strong>
                    <span class="handle"><span class="live" />{{ displayName !== account.username ? `@${account.username}` : "ArcLabs connected" }}</span>
                </div>
            </header>

            <div class="hub">
                <nav class="rail" aria-label="Social sections">
                    <button :class="{ selected: page === 'friends' || page === 'add' }" aria-label="Friends"
                        :aria-current="page === 'friends' || page === 'add' ? 'page' : undefined" @click="page = 'friends'">
                        <svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M4 2h5v1h1v4H9v1H4V7H3V3h1zm-2 8h9v1h2v4H0v-4h2zm10-6h2v1h1v3h-1v1h-2V8h1V5h-1z" /></svg>
                        <span>Friends</span>
                    </button>
                    <button :class="{ selected: page === 'activity' }" aria-label="Activity"
                        :aria-current="page === 'activity' ? 'page' : undefined" @click="page = 'activity'">
                        <svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M2 2h12v11h-3v2H5v-2H2zm2 2v7h8V4zm1 1h6v2H5zm0 3h4v2H5z" /></svg>
                        <span>Inbox</span><b v-if="waiting" class="rail-badge">{{ waiting }}</b>
                    </button>
                    <button class="account-tab" :class="{ selected: page === 'account' }" aria-label="Account"
                        :aria-current="page === 'account' ? 'page' : undefined" @click="page = 'account'">
                        <svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M5 1h6v2h1v5h-1v1H5V8H4V3h1zm-1 10h8v1h2v3H2v-3h2z" /></svg>
                        <span>Account</span>
                    </button>
                </nav>

                <div class="workspace">
                    <div class="content">
                        <Notice v-if="error" tone="danger">{{ error }}</Notice>
                        <Notice v-if="social?.problem" tone="warn">Friends may be out of date: {{ social.problem }}</Notice>

                        <template v-if="page === 'friends'">
                            <div class="section-head">
                                <div><span class="kicker">People · {{ online }} online</span><h2>Your friends</h2></div>
                                <Button size="sm" @click="page = 'add'">+ Add</Button>
                            </div>
                            <p v-if="!social" class="empty">Connecting to friends…</p>
                            <p v-else-if="!view" class="empty">Loading friends…</p>
                            <p v-else-if="!friends.length" class="empty">Your friends will appear here. Add someone by their ArcLabs username.</p>
                            <div v-if="friends.length" class="list">
                                <div v-for="friend in friends" :key="friend.id" class="friend">
                                    <span class="presence" :class="presenceDot(friend.presence)" />
                                    <div class="friend-copy">
                                        <strong>{{ friend.name || friend.username }}</strong>
                                        <small><span v-if="friend.name">@{{ friend.username }} · </span>{{ presenceLine(friend.presence) }}</small>
                                    </div>
                                    <Button v-if="joinable(friend)" size="sm" variant="primary" :disabled="busy || !inMenu"
                                        :title="inMenu ? undefined : 'Leave your world first'" @click="emit('join', friend)">
                                        {{ joining === friend.id ? "Joining…" : "Join" }}
                                    </Button>
                                    <Button v-if="invitable(friend)" size="sm" :disabled="busy" @click="emit('invite', friend.id)">Invite</Button>
                                    <button class="row-more" :class="{ danger: confirming === friend.id }" :disabled="busy"
                                        :aria-label="confirming === friend.id ? `Confirm removing ${friend.username}` : `Friend options for ${friend.username}`"
                                        :title="confirming === friend.id ? 'Press again to remove friend' : 'Friend options'"
                                        @click="remove(friend)">{{ confirming === friend.id ? "Remove" : "•••" }}</button>
                                </div>
                            </div>
                        </template>

                        <template v-else-if="page === 'activity'">
                            <div class="section-head"><div><span class="kicker">Activity · {{ activity }}</span><h2>Invites & requests</h2></div></div>
                            <p v-if="!social" class="empty">Connecting to ArcLabs…</p>
                            <p v-else-if="!view" class="empty">Loading invitations and requests…</p>
                            <p v-if="view && !activity" class="empty">You’re all caught up. Invitations and friend requests will appear here.</p>
                            <div v-if="view && (view.invites.length || view.incoming.length)" class="list">
                                <div v-for="invite in view.invites" :key="invite.id" class="activity">
                                    <span class="activity-mark">»</span>
                                    <div class="friend-copy"><strong>{{ invite.from.username }}</strong><small>Invited you to {{ invite.world }}</small></div>
                                    <Button size="sm" variant="primary" :disabled="busy || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
                                        @click="emit('join', invite.from)">{{ joining === invite.from.id ? "Joining…" : "Join" }}</Button>
                                    <Button size="sm" :disabled="busy" aria-label="Dismiss invite" @click="emit('dismiss', invite.id)">×</Button>
                                </div>
                                <div v-for="player in view.incoming" :key="player.id" class="activity">
                                    <span class="activity-mark">+</span>
                                    <div class="friend-copy"><strong>{{ player.username }}</strong><small>Wants to be friends</small></div>
                                    <Button size="sm" variant="primary" :disabled="busy" @click="emit('accept', player.id)">Accept</Button>
                                    <Button size="sm" :disabled="busy" aria-label="Decline friend request" @click="emit('remove', player.id)">×</Button>
                                </div>
                            </div>
                            <template v-if="view?.outgoing.length">
                                <div class="section-head sent-head"><div><span class="kicker">Pending</span><h2>Sent requests</h2></div></div>
                                <div class="list">
                                    <div v-for="player in view.outgoing" :key="player.id" class="activity">
                                        <span class="activity-mark muted">…</span>
                                        <div class="friend-copy"><strong>{{ player.username }}</strong><small>Waiting for a reply</small></div>
                                        <button class="text-action" :disabled="busy" @click="emit('remove', player.id)">Cancel</button>
                                    </div>
                                </div>
                            </template>
                        </template>

                        <template v-else-if="page === 'add'">
                            <button class="back" @click="page = 'friends'">← Friends</button>
                            <div class="section-head"><div><span class="kicker">Find a player</span><h2>Add a friend</h2></div></div>
                            <p class="copy">Enter their exact ArcLabs username. They'll see your request the next time they play.</p>
                            <form class="add-form" @submit.prevent="add">
                                <Field v-model="username" label="ArcLabs username" placeholder="Username" :maxlength="64" />
                                <Button variant="primary" :disabled="busy || !username.trim()">Send request »</Button>
                            </form>
                        </template>

                        <template v-else>
                            <div class="section-head"><div><span class="kicker">ArcLabs / Profile</span><h2>Account details</h2></div></div>
                            <dl class="details">
                                <div><dt>Username</dt><dd>@{{ account.username }}</dd></div>
                                <div v-if="displayName !== account.username"><dt>Display name</dt><dd>{{ displayName }}</dd></div>
                                <div><dt>Email</dt><dd class="muted">Managed by ArcLabs</dd></div>
                                <div><dt>Connection</dt><dd><span class="live" /> Connected</dd></div>
                            </dl>
                            <p class="copy">Your ArcLabs identity connects you with friends across Terra Firma worlds.</p>
                            <div class="account-actions"><Button size="sm" @click="emit('signOut')">Sign out on this device</Button></div>
                        </template>
                    </div>
                    <footer class="presence-footer">
                        <div><span class="live" /><strong>{{ myPresence }}</strong><small v-if="social?.me.world">{{ social.me.world }}</small></div>
                        <Toggle v-if="social?.joinable" v-model="open" label="Friends may join" />
                    </footer>
                </div>
            </div>
        </template>

        <div v-else class="signed-out">
            <span class="eyebrow">» ArcLabs account</span>
            <h2>{{ account.status === "unreachable" ? "Connection lost" : "Play together" }}</h2>
            <template v-if="account.status === 'signedOut'">
                <p>Sign in to find friends, send invites, and join their worlds. Singleplayer works without an account.</p>
                <Notice v-if="account.problem" tone="warn">{{ account.problem }}</Notice>
                <Button variant="primary" @click="emit('signIn')">Sign in »</Button>
            </template>
            <template v-else-if="account.status === 'pending'">
                <p>Approve the sign-in in your browser. Check that it shows this code:</p>
                <output class="code">{{ account.code }}</output>
                <div class="signed-out-actions">
                    <Button size="sm" @click="emit('openUrl', account.url)">Open browser</Button>
                    <Button size="sm" @click="emit('cancel')">Cancel</Button>
                </div>
            </template>
            <template v-else-if="account.status === 'unreachable'">
                <Notice tone="warn">Couldn't check your sign-in: {{ account.problem }}.</Notice>
                <div class="signed-out-actions">
                    <Button size="sm" variant="primary" @click="emit('retry')">Retry</Button>
                    <Button size="sm" @click="emit('signOut')">Sign out</Button>
                </div>
            </template>
            <p v-else>{{ account.status === "starting" ? "Getting a sign-in code…" : "Checking your sign-in…" }}</p>
        </div>
    </section>
</template>

<style scoped>
.dropdown {
    width: min(440px, calc(100vw - 40px)); max-height: min(650px, calc(100vh - 88px));
    margin-top: 10px; display: flex; flex-direction: column; overflow: hidden;
    border: var(--b) solid var(--edge); background: var(--stone-1);
    box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}
.dropdown.connected { height: min(540px, calc(100vh - 88px)); min-height: min(400px, calc(100vh - 88px)); }
.identity {
    flex: none; display: flex; align-items: center; gap: 10px; min-height: 54px; padding: 8px 10px;
    background: var(--stone-2); border-bottom: var(--b) solid var(--edge); box-shadow: var(--raised);
}
.avatar {
    flex: none; display: grid; place-items: center; width: 34px; height: 34px;
    background: var(--stone-3); border: var(--b) solid var(--edge); box-shadow: var(--raised);
    font: 15px var(--font-display); color: var(--accent-hi);
}
.identity-text { min-width: 0; display: flex; flex-direction: column; justify-content: center; line-height: 1.3; }
.identity-text strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font: 12px var(--font-display); text-transform: uppercase; }
.eyebrow { font: 9px var(--font-display); color: var(--ink-dim); text-transform: uppercase; letter-spacing: .06em; }
.handle { color: var(--ink-dim); font-size: 10px; overflow: hidden; text-overflow: ellipsis; }
.live, .presence { display: inline-block; width: 7px; height: 7px; margin-right: 5px; background: var(--accent-hi); border: 1px solid var(--edge); }
.hub { display: grid; grid-template-columns: 62px minmax(0, 1fr); min-height: 0; flex: 1; }
.rail { display: flex; flex-direction: column; gap: 5px; min-height: 0; padding: 8px 5px; border-right: var(--b) solid var(--edge); background: var(--stone-2); }
.rail button { position: relative; display: flex; flex-direction: column; align-items: center; gap: 3px; min-width: 0; padding: 8px 2px 7px; border: var(--b) solid transparent; background: none; color: var(--ink-faint); font: 8px var(--font-display); text-transform: uppercase; }
.rail button:hover { color: var(--ink); background: var(--stone-3); }
.rail button.selected { border-color: var(--edge); background: var(--accent-lo); box-shadow: var(--sunk); color: var(--ink); }
.rail svg { width: 18px; height: 18px; }
.rail button.selected svg { color: var(--accent-hi); }
.rail .account-tab { margin-top: auto; }
.rail-badge { position: absolute; top: 3px; right: 2px; min-width: 15px; height: 15px; padding: 0 2px; border: 1px solid var(--edge); background: var(--accent); color: var(--ink); font: 8px/13px var(--font-display); }
.workspace { display: flex; flex-direction: column; min-width: 0; min-height: 0; }
.content { min-height: 0; flex: 1; overflow-y: auto; padding: 16px 16px 18px; display: flex; flex-direction: column; gap: 10px; background: var(--stone-1); }
.presence-footer { flex: none; min-height: 48px; display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 8px 12px; border-top: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised); }
.presence-footer > div { min-width: 0; display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: center; }
.presence-footer .live { grid-row: 1 / span 2; }
.presence-footer strong, .presence-footer small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.presence-footer strong { font-size: 10px; }
.presence-footer small { color: var(--ink-faint); font-size: 9px; }
.presence-footer :deep(.toggle) { flex: none; width: auto; gap: 7px; font-size: 9px; }
.section-head { display: flex; justify-content: space-between; align-items: end; gap: 10px; margin-bottom: 4px; }
.section-head:not(:first-child) { margin-top: 8px; }
.section-head > div { min-width: 0; }
.section-head :deep(.btn) { flex: none; }
.kicker { color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
h2 { margin: 2px 0 0; font: 15px var(--font-display); text-transform: uppercase; }
.list { border-top: 1px solid var(--stone-4); border-bottom: 1px solid var(--stone-4); }
.friend, .activity { display: flex; align-items: center; gap: 8px; min-width: 0; min-height: 50px; padding: 8px 2px; background: transparent; }
.friend + .friend, .activity + .activity, .activity + .friend { border-top: 1px solid var(--stone-3); }
.friend:hover, .activity:hover { background: rgb(255 255 255 / 0.025); }
.presence { flex: none; width: 9px; height: 9px; margin: 0 3px 0 0; }
.presence.online { background: #5fae5a; }
.presence.offline { background: var(--stone-4); }
.friend-copy { flex: 1; min-width: 0; display: flex; flex-direction: column; line-height: 1.35; }
.friend-copy strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 12px; }
.friend-copy small { color: var(--ink-dim); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 10px; }
.activity-mark { color: var(--accent-hi); font: 16px var(--font-display); }
.activity-mark.muted { color: var(--ink-faint); }
.friend :deep(.btn), .activity :deep(.btn) { padding: 0 9px; height: 28px; font-size: 10px; }
.row-more { flex: none; border: 0; background: none; color: var(--ink-faint); padding: 4px; font-size: 17px; line-height: 1; }
.row-more:hover, .row-more.danger { color: var(--danger); }
.row-more.danger { font: 10px var(--font-display); }
.back, .text-action { align-self: start; border: 0; background: none; padding: 2px; color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
.back:hover, .text-action:hover { text-decoration: underline; }
.copy, .empty { margin: 2px 0 8px; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.add-form { display: grid; gap: 14px; margin-top: 4px; }
.add-form :deep(.btn) { justify-self: start; }
.details { margin: 4px 0 6px; border-top: 1px solid var(--stone-4); }
.details > div { display: grid; grid-template-columns: 116px minmax(0, 1fr); gap: 12px; padding: 11px 2px; border-bottom: 1px solid var(--stone-3); font-size: 11px; }
.details dt { color: var(--ink-dim); }
.details dd { margin: 0; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-weight: 700; }
.details dd.muted { color: var(--ink-faint); font-weight: 400; }
.account-actions { margin-top: auto; padding-top: 12px; border-top: 1px solid var(--stone-4); }
.signed-out { display: grid; gap: 14px; padding: 24px; }
.signed-out h2 { margin: -8px 0 0; font-size: 22px; }
.signed-out p { margin: 0; color: var(--ink-dim); font-size: 12px; }
.signed-out-actions { display: flex; gap: 8px; }
.code { padding: 10px; background: var(--stone-0); border: var(--b) solid var(--edge); box-shadow: var(--sunk); color: var(--accent-hi); text-align: center; font: 26px var(--font-display); }
</style>
