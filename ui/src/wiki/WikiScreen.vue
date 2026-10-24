<script setup lang="ts">
import { onMounted } from "vue"
import { useBridge } from "../bridge/use"
import { useStore } from "../game/store"
import PixelIcon from "../game/PixelIcon.vue"
import { Button, Panel } from "../kit"
import FlightSection from "./FlightSection.vue"
import LabSection from "./LabSection.vue"
import MatterSection from "./MatterSection.vue"
import { provideWiki, WikiNav, type Section } from "./nav"
import ReactionsSection from "./ReactionsSection.vue"
import ReactorSection from "./ReactorSection.vue"

/**
 * The wiki: what everything is made of and how it behaves, shown rather
 * than told. An icon rail picks the section; each has its list, a live
 * view, and the facts. The same page in the menu and in game (key G, where
 * the world's rules allow it).
 */
const props = defineProps<{ place: "menu" | "game" }>()
defineEmits<{ close: [] }>()
const bridge = useBridge()
const store = useStore()
const wiki = WikiNav({
    place: props.place,
    seen: () => (props.place === "game" ? new Set(store.ui.discovered ?? []) : null),
})
provideWiki(wiki)

onMounted(async () => {
    const t = await bridge.send("matter.list", {})
    wiki.table.value = { ...t, bySymbol: new Map(t.elements.map((e) => [e.symbol, e])), byId: new Map(t.substances.map((s) => [s.id, s])) }
})

// 9×9 pictograms for the rail
const RAIL: { id: Section; label: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "matter", label: "Matter", palette: { O: "#c9d4dc", N: "#ff8a3d" },
        rows: ["...O.O...", "..O...O..", ".O.....O.", "O...N...O", "O..NNN..O", "O...N...O", ".O.....O.", "..O...O..", "...O.O..."] },
    { id: "reactions", label: "Reactions", palette: { F: "#ffb040", R: "#e0603d", W: "#8a5a32" },
        rows: ["....F....", "...FRF...", "..FRRF...", "..FRRRF..", ".FRRRRF..", ".FRRRRRF.", "..FRRRF..", ".WWWWWWW.", "..W...W.."] },
    { id: "reactor", label: "Reactor", palette: { G: "#c9d4dc", L: "#40b8ec", F: "#ffb040" },
        rows: ["..G...G..", "..G...G..", ".G.....G.", ".GLLLLLG.", ".GLLLLLG.", "..GLLLG..", "...GGG...", "..F.F.F..", ".F.F.F.F."] },
    { id: "lab", label: "Lab", palette: { W: "#c9d4dc", G: "#c9d4dc", L: "#40b8ec" },
        rows: ["...WWW...", "....G....", "....G....", "...G.G...", "..G...G..", ".GLLLLLG.", ".GLLLLLG.", ".GLLLLLG.", "..GGGGG.."] },
    { id: "flight", label: "Flight: Newton's cannon", palette: { P: "#3c6e46", O: "#ffb040", W: "#c9d4dc" },
        rows: ["...OOO...", ".O.....O.", "O..PPP..O", "O.PPPPP.O", "O.PPPPP.W", "O.PPPPP.O", "O..PPP..O", ".O.....O.", "...OOO..."] },
]
</script>

<template>
    <Panel title="Wiki" class="wiki">
        <template #actions><Button size="sm" @click="$emit('close')">{{ place === "menu" ? "Back" : "Close" }}</Button></template>
        <div class="frame">
            <nav class="rail" aria-label="Sections">
                <button v-for="r in RAIL" :key="r.id" :class="{ on: wiki.state.section === r.id }" :title="r.label" @click="wiki.go(r.id)">
                    <PixelIcon :rows="r.rows" :palette="r.palette" style="--u: 3px" />
                    <span>{{ r.label }}</span>
                </button>
            </nav>
            <div v-if="wiki.table.value" class="section">
                <MatterSection v-if="wiki.state.section === 'matter'" />
                <ReactionsSection v-else-if="wiki.state.section === 'reactions'" />
                <ReactorSection v-else-if="wiki.state.section === 'reactor'" />
                <LabSection v-else-if="wiki.state.section === 'lab'" />
                <FlightSection v-else-if="wiki.state.section === 'flight'" />
            </div>
            <p v-else class="loading">reading the tables…</p>
        </div>
    </Panel>
</template>

<style scoped>
.wiki { width: min(1340px, 97vw); height: min(840px, 94vh); }
.wiki :deep(.body) { overflow: hidden; }
.frame { display: grid; grid-template-columns: 64px minmax(0, 1fr); gap: 0 18px; height: 100%; min-height: 0; }
.rail { display: flex; flex-direction: column; gap: 6px; padding-right: 14px; border-right: var(--b) solid var(--edge); }
.rail button { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 8px 2px; border: var(--b) solid transparent;
    background: none; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
.rail button:hover { color: var(--ink); background: var(--stone-1); }
.rail button.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.section { min-height: 0; height: 100%; }
.loading { color: var(--ink-faint); }
</style>
