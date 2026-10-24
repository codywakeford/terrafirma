<script setup lang="ts">
import { defineAsyncComponent, ref } from "vue"
import Corner from "../account/Corner.vue"
import PixelIcon from "../game/PixelIcon.vue"
import { IconTile } from "../kit"
import CreateWorld from "./create/CreateWorld.vue"
import Home from "./Home.vue"
import Settings from "./Settings.vue"
import type { Screen } from "./screens"
import WorldSelect from "./WorldSelect.vue"

// the wiki brings three.js: loaded when first opened
const WikiScreen = defineAsyncComponent(() => import("../wiki/WikiScreen.vue"))

defineProps<{ error: string | null }>()
defineEmits<{ dismiss: [] }>()

const screen = ref<Screen>("home")

// a 9×9 pictogram for the workshop row: an open book
const WIKI_ICON = {
    rows: [".........", "BBBB.BBBB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BBBBBBBBB", "....B...."],
    palette: { B: "#c9d4dc", P: "#f0ead8", L: "#8a8478" },
} as const
</script>

<template>
    <main class="menu">
        <div v-if="error" class="error" role="alert">
            <span><strong>Session ended:</strong> {{ error }}</span>
            <button @click="$emit('dismiss')">✕</button>
        </div>

        <Transition name="screen" mode="out-in">
            <Home v-if="screen === 'home'" @go="screen = $event" />
            <WorldSelect v-else-if="screen === 'worlds'" @go="screen = $event" />
            <CreateWorld v-else-if="screen === 'create'" @go="screen = $event" />
            <Settings v-else-if="screen === 'settings'" @go="screen = $event" />
            <WikiScreen v-else-if="screen === 'wiki'" place="menu" @close="screen = 'home'" />
        </Transition>

        <!-- the home screen's corners: outside the screen transition, whose
             transform would carry anything pinned to the window along with it -->
        <Transition name="corners">
            <div v-if="screen === 'home'" class="corners">
                <Corner />
                <!-- the workshop: places to see the engine's building blocks and mechanics -->
                <nav class="tools" aria-label="Workshop">
                    <IconTile label="Wiki — what everything is made of, and how it behaves" side="left" @click="screen = 'wiki'">
                        <PixelIcon :rows="WIKI_ICON.rows" :palette="WIKI_ICON.palette" style="--u: 3px" />
                    </IconTile>
                </nav>
                <footer>v0.1 · built on luanti</footer>
            </div>
        </Transition>
    </main>
</template>

<style scoped>
.menu {
    position: relative;
    height: 100%;
    display: grid;
    place-items: center;
    overflow: hidden;
    background: transparent;
}
.menu::before {
    content: "";
    position: absolute;
    inset: 0;
    pointer-events: none;
    background:
        radial-gradient(ellipse 27% 48% at 50% 52%, rgb(3 8 15 / 0.68), transparent 100%),
        linear-gradient(90deg, rgb(3 8 15 / 0.18), transparent 35%, transparent 70%, rgb(3 8 15 / 0.08));
}
.menu > :deep(section), .menu > :deep(.home) { position: relative; z-index: 1; }

.error {
    position: absolute;
    top: 20px;
    z-index: 2;
    display: flex;
    gap: 16px;
    align-items: center;
    padding: 10px 14px;
    border: var(--b) solid var(--edge);
    background: var(--danger-lo);
    box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35);
}
.error button { border: 0; background: none; padding: 0 4px; }

.corners { position: absolute; inset: 0; z-index: 1; pointer-events: none; }
.corners > * { pointer-events: auto; }
.tools { position: absolute; right: 20px; bottom: 40px; display: flex; gap: 10px; }
footer {
    position: absolute;
    right: 20px;
    bottom: 14px;
    font: 11px var(--font-display);
    color: var(--ink-faint);
    text-transform: uppercase;
}
.corners-enter-active, .corners-leave-active { transition: opacity 140ms steps(4); }
.corners-enter-from, .corners-leave-to { opacity: 0; }

.screen-enter-active, .screen-leave-active { transition: opacity 140ms steps(4), transform 140ms steps(4); }
.screen-enter-from { opacity: 0; transform: translateY(12px); }
.screen-leave-to { opacity: 0; transform: translateY(-12px); }
</style>
