<script setup lang="ts">
import { ref } from "vue"
import Corner from "../account/Corner.vue"
import { useBridge } from "../bridge/use"
import { Button, Panel } from "../kit"
import SettingsPanel from "../settings/SettingsPanel.vue"
import WorldControlsWidget from "./widgets/WorldControlsWidget.vue"

/** Esc in-game. The engine has paused singleplayer while this is open; Esc again resumes. */
const bridge = useBridge()
const view = ref<"menu" | "settings">("menu")
const settingsQuery = ref("")
</script>

<template>
    <div class="pause">
        <Corner v-if="view === 'menu'" />
        <WorldControlsWidget v-if="view === 'menu'" />
        <Panel v-if="view === 'menu'" title="Paused" class="menu">
            <nav>
                <Button variant="primary" size="lg" @click="bridge.send('screen.close', {})">Resume</Button>
                <Button size="lg" @click="view = 'settings'">Settings</Button>
                <Button size="lg" @click="bridge.send('screen.open', { name: 'map' })">Map</Button>
                <div class="pair">
                    <Button size="lg" @click="bridge.send('game.leave', {})">Main menu</Button>
                    <Button size="lg" variant="danger" @click="bridge.send('game.quit', {})">Quit game</Button>
                </div>
            </nav>
            <template #footer><span class="hint">esc to resume</span></template>
        </Panel>

        <SettingsPanel v-else-if="view === 'settings'" v-model:query="settingsQuery" close-label="Back" @close="view = 'menu'" />
    </div>
</template>

<style scoped>
.pause {
    position: fixed;
    inset: 0;
    display: grid;
    place-items: center;
    background: rgb(4 8 12 / 0.55);
}
.menu { width: max-content; min-width: 440px; }
nav { display: grid; gap: 12px; }
nav > :deep(.btn), .pair :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.hint { margin-right: auto; font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
</style>
