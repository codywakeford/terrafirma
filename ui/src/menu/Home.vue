<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"
import { Button, IconTile } from "../kit"
import { useBridge } from "../bridge/use"
import type { Screen } from "./screens"

defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()

// a 9×9 pictogram for the workshop row: an open book
const TOOL_ICONS = {
    wiki: {
        rows: [".........", "BBBB.BBBB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BBBBBBBBB", "....B...."],
        palette: { B: "#c9d4dc", P: "#f0ead8", L: "#8a8478" },
    },
} as const

const REPO = "https://github.com/codywakeford/terrafirma"
</script>

<template>
    <div class="home">
        <h1>
            <span class="chev">»</span>
            <span class="word">Terra</span>
            <span class="word">Firma</span>
        </h1>
        <p class="tagline">solid ground. build on it.</p>

        <nav>
            <Button variant="primary" size="lg" @click="$emit('go', 'worlds')">Play</Button>
            <Button size="lg" @click="$emit('go', 'settings')">Settings</Button>
            <div class="pair">
                <Button size="lg" @click="bridge.send('open', { url: REPO })">
                    <svg viewBox="0 0 8 8" width="18" height="18" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M2 0h4v1h1v1h1v4h-1v1h-2v-2h-2v2h-2v-1h-1v-4h1v-1h1z" />
                    </svg>
                    GitHub
                </Button>
                <Button size="lg" @click="bridge.send('quit', {})">Quit</Button>
            </div>
        </nav>

        <!-- the workshop: places to see the engine's building blocks and mechanics -->
        <nav class="tools" aria-label="Workshop">
            <IconTile label="Wiki — what everything is made of, and how it behaves" side="left" @click="$emit('go', 'wiki')">
                <PixelIcon :rows="TOOL_ICONS.wiki.rows" :palette="TOOL_ICONS.wiki.palette" style="--u: 3px" />
            </IconTile>
        </nav>

        <footer>v0.1 · built on luanti</footer>
    </div>
</template>

<style scoped>
.home { display: flex; flex-direction: column; align-items: center; }

h1 {
    display: grid;
    grid-template-columns: auto 1fr;
    column-gap: 0.18em;
    margin: 0;
    font: 400 clamp(54px, 5.8vw, 104px) / 1 var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.02em;
    color: #eef3f6;
    text-shadow:
        4px 4px 0 #0a3a52,
        8px 8px 0 rgb(0 0 0 / 0.45);
    animation: drop 500ms steps(6) both;
}

.chev { color: var(--accent); text-shadow: 4px 4px 0 #062636, 8px 8px 0 rgb(0 0 0 / 0.45); }
.chev { grid-row: 1 / 3; }

.tagline {
    margin: 18px 0 52px;
    font: 13px var(--font);
    letter-spacing: 0.32em;
    text-transform: uppercase;
    color: var(--ink-dim);
}

nav { display: grid; gap: 12px; width: min(420px, 84vw); }
nav > :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.pair :deep(.btn) { width: 100%; }

.tools {
    position: fixed;
    right: 20px;
    bottom: 40px;
    display: flex;
    gap: 10px;
}

footer {
    position: fixed;
    right: 20px;
    bottom: 14px;
    font: 11px var(--font-display);
    color: var(--ink-faint);
    text-transform: uppercase;
}

@keyframes drop { from { opacity: 0; transform: translateY(-24px); } }

@media (max-width: 850px) {
    h1 { font-size: clamp(48px, 7vw, 72px); }
    .tagline { letter-spacing: 0.18em; }
}
</style>
