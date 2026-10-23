<script setup lang="ts">
import { defineAsyncComponent } from "vue"
import { useBridge } from "./bridge/use"
import Hud from "./game/Hud.vue"
import InventoryScreen from "./game/inventory/InventoryScreen.vue"
import PauseScreen from "./game/PauseScreen.vue"
import { useStore } from "./game/store"
import { useLab } from "./lab/flow"
import LabView from "./lab/LabView.vue"
import MainMenu from "./menu/MainMenu.vue"

// the wiki brings three.js: loaded when first opened
const WikiScreen = defineAsyncComponent(() => import("./wiki/WikiScreen.vue"))
const bridge = useBridge()
const store = useStore()
const lab = useLab()
</script>

<template>
    <p v-if="!bridge.connected" class="detached">Not running inside the engine.</p>
    <!-- the Lab page outlives the scenario worlds under it -->
    <LabView v-else-if="lab.state.open && (store.mode === 'menu' || store.screen === 'lab')" />
    <MainMenu v-else-if="store.mode === 'menu'" :error="store.error" @dismiss="store.error = null" />
    <template v-else-if="store.mode === 'game'">
        <Hud />
        <PauseScreen v-if="store.screen === 'pause'" />
        <InventoryScreen v-else-if="store.screen === 'inventory'" />
        <WikiScreen v-else-if="store.screen === 'wiki'" place="game" @close="bridge.send('screen.close', {})" />
    </template>
</template>

<style scoped>
.detached { padding: 24px; color: var(--ink-dim); background: var(--stone-0); height: 100%; margin: 0; }
</style>
