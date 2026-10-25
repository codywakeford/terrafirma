<script setup lang="ts">
import { computed, onMounted } from "vue"
import { useBridge } from "../bridge/use"
import { useStore } from "../game/store"
import { Button, WorkbenchPanel } from "../kit"
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
const RAIL: { id: Section; label: string; shortLabel?: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "matter", label: "Matter", palette: { O: "#c9d4dc", N: "#ff8a3d" },
        rows: ["...O.O...", "..O...O..", ".O.....O.", "O...N...O", "O..NNN..O", "O...N...O", ".O.....O.", "..O...O..", "...O.O..."] },
    { id: "reactions", label: "Reactions", shortLabel: "React", palette: { F: "#ffb040", R: "#e0603d", W: "#8a5a32" },
        rows: ["....F....", "...FRF...", "..FRRF...", "..FRRRF..", ".FRRRRF..", ".FRRRRRF.", "..FRRRF..", ".WWWWWWW.", "..W...W.."] },
    { id: "reactor", label: "Reactor", palette: { G: "#c9d4dc", L: "#40b8ec", F: "#ffb040" },
        rows: ["..G...G..", "..G...G..", ".G.....G.", ".GLLLLLG.", ".GLLLLLG.", "..GLLLG..", "...GGG...", "..F.F.F..", ".F.F.F.F."] },
    { id: "lab", label: "Lab", palette: { W: "#c9d4dc", G: "#c9d4dc", L: "#40b8ec" },
        rows: ["...WWW...", "....G....", "....G....", "...G.G...", "..G...G..", ".GLLLLLG.", ".GLLLLLG.", ".GLLLLLG.", "..GGGGG.."] },
    { id: "flight", label: "Flight: Newton's cannon", shortLabel: "Flight", palette: { P: "#3c6e46", O: "#ffb040", W: "#c9d4dc" },
        rows: ["...OOO...", ".O.....O.", "O..PPP..O", "O.PPPPP.O", "O.PPPPP.W", "O.PPPPP.O", "O..PPP..O", ".O.....O.", "...OOO..."] },
]
const section = computed({
    get: () => wiki.state.section,
    set: (id: Section) => wiki.go(id),
})
const sectionLabel = computed(() => RAIL.find((item) => item.id === section.value)?.label ?? "Wiki")
const query = wiki.search
</script>

<template>
    <WorkbenchPanel v-model:group="section" v-model:query="query" class="wiki"
        title="Wiki" :section="sectionLabel" :groups="RAIL" rail-label="Wiki sections"
        :search-label="section === 'flight' ? undefined : `Search ${sectionLabel.toLowerCase()}`"
        :search-placeholder="`Search ${sectionLabel.toLowerCase()}…`">
        <template #actions><Button size="sm" @click="$emit('close')">{{ place === "menu" ? "Back" : "Close" }}</Button></template>
        <template #workspace>
            <div v-if="wiki.table.value" class="section">
                <MatterSection v-if="wiki.state.section === 'matter'" />
                <ReactionsSection v-else-if="wiki.state.section === 'reactions'" />
                <ReactorSection v-else-if="wiki.state.section === 'reactor'" />
                <LabSection v-else-if="wiki.state.section === 'lab'" />
                <FlightSection v-else-if="wiki.state.section === 'flight'" />
            </div>
            <p v-else class="loading">reading the tables…</p>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.section { min-width: 0; min-height: 0; height: 100%; }
.loading { color: var(--ink-faint); }
</style>
