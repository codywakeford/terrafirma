<script setup lang="ts">
import AccountCard from "../../account/AccountCard.vue"
import type { Account, Social } from "../../bridge"
import FriendsPanel from "../../social/FriendsPanel.vue"
import { cannedSocial } from "../mock"
import Specimen from "../Specimen.vue"

/** Every state of the account card and the friends panel. The menu's corner (Screens) runs both against the mock. */
const STATES: { note: string; account: Account }[] = [
    { note: "signed out", account: { status: "signedOut" } },
    { note: "signed out, after a problem", account: { status: "signedOut", problem: "The sign-in code expired. Try again." } },
    { note: "asking for a code", account: { status: "starting" } },
    { note: "waiting for approval", account: { status: "pending", code: "TFM-4K2", url: "https://axon.arclabs.it/auth/device" } },
    { note: "checking a saved sign-in", account: { status: "checking" } },
    { note: "signed in", account: { status: "signedIn", id: "u-1", username: "cody", name: "Cody Wakeford" } },
    { note: "backend unreachable", account: { status: "unreachable", problem: "couldn't reach https://axon.arclabs.it" } },
]

const FRIENDS: { note: string; social: Social; error?: string; joining?: string }[] = [
    { note: "in the menu: invites, requests, friends in every place", social: cannedSocial() },
    { note: "joining ash", social: cannedSocial(), joining: "00000000-0000-4000-8000-000000000001" },
    { note: "in your own world: open it, invite", social: cannedSocial({ inWorld: true }) },
    { note: "a refusal, and a stale list", social: { ...cannedSocial(), problem: "couldn't reach ArcLabs" }, error: "no ArcLabs account has that username" },
    { note: "no friends yet", social: { me: { state: "menu", world: null, open: false }, joinable: false, view: { friends: [], incoming: [], outgoing: [], invites: [] } } },
    { note: "before the first answer", social: { me: { state: "menu", world: null, open: false }, joinable: false, view: null } },
]
</script>

<template>
    <div class="grid">
        <Specimen v-for="state in STATES" :key="state.note" name="AccountCard" :note="state.note">
            <AccountCard :account="state.account" />
        </Specimen>
        <Specimen v-for="state in FRIENDS" :key="state.note" name="FriendsPanel" :note="state.note">
            <FriendsPanel :social="state.social" :error="state.error ?? null" :busy="false" :joining="state.joining ?? null" />
        </Specimen>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(420px, 1fr)); gap: 28px; }
</style>
