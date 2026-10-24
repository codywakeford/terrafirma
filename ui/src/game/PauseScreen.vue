<script setup lang="ts">
import { ref } from "vue"
import { useBridge } from "../bridge/use"
import { Button, Panel } from "../kit"
import SettingsPanel from "../settings/SettingsPanel.vue"
import SocialPanel from "../social/SocialPanel.vue"
import { useStore } from "./store"

/** Esc in-game. The engine has paused singleplayer while this is open; Esc again resumes. */
const bridge = useBridge()
const store = useStore()
const view = ref<"menu" | "settings" | "friends">("menu")
</script>

<template>
    <div class="pause">
        <Panel v-if="view === 'menu'" title="Paused" class="menu">
            <nav>
                <Button variant="primary" size="lg" @click="bridge.send('screen.close', {})">Resume</Button>
                <Button size="lg" @click="view = 'settings'">Settings</Button>
                <Button v-if="store.social" size="lg" @click="view = 'friends'">Friends</Button>
                <div class="pair">
                    <Button size="lg" @click="bridge.send('game.leave', {})">Main menu</Button>
                    <Button size="lg" variant="danger" @click="bridge.send('game.quit', {})">Quit game</Button>
                </div>
            </nav>
            <template #footer><span class="hint">esc to resume</span></template>
        </Panel>

        <SocialPanel v-else-if="view === 'friends' && store.social" @close="view = 'menu'" />

        <Panel v-else-if="view === 'settings'" title="Settings" class="settings">
            <template #actions><Button size="sm" @click="view = 'menu'">Back</Button></template>
            <SettingsPanel />
        </Panel>
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
.settings { width: min(1200px, 96vw); height: min(760px, 92vh); }
.settings :deep(.body) { padding: 0; overflow: hidden; }
nav { display: grid; gap: 12px; }
nav > :deep(.btn), .pair :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.hint { margin-right: auto; font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-faint); }
</style>
