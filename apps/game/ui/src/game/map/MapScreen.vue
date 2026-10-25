<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { useStore } from "../store.ts"
import { Button, IconTile, Notice, Panel } from "../../kit/index.ts"
import PixelIcon from "../PixelIcon.vue"
import MapView from "../../worldgen/MapView.vue"
import { declination } from "../../worldgen/sun.ts"
import { star } from "../../worldgen/config.ts"
import { OVERLAY_ICONS } from "../../worldgen/overlayIcons.ts"
import { mapCoordinates, mapImage, mapUv, worldMapData } from "./data.ts"
import MapGlobe from "./MapGlobe.vue"

const bridge = useBridge()
const store = useStore()
const data = worldMapData()
const map = computed(() => store.ui.map)
const position = computed(() => store.ui.map_position)
const pose = computed(() => store.state.mapPose ?? (position.value && map.value ? {
    ...mapUv(position.value.x, position.value.z, map.value.size), heading: 0,
} : null))
const coordinates = computed(() => pose.value && map.value ? mapCoordinates(pose.value, map.value.size) : null)
const fields = computed(() => {
    const current = data.preview.value
    return current && current.code === map.value?.code ? current.fields : null
})
const view = ref<"globe" | "flat">("globe")
const overlay = ref<"terrain" | "biomes" | "elevation" | "temperature" | "moisture" | "geology">("terrain")
const overlays = [
    { id: "terrain", label: "Terrain" }, { id: "biomes", label: "Biomes" },
    { id: "elevation", label: "Elevation" }, { id: "temperature", label: "Temperature" },
    { id: "moisture", label: "Rainfall" }, { id: "geology", label: "Geology" },
] as const
const viewIcons = {
    globe: { rows: ["...AAA...", ".AABBBA..", "AABBBBAA.", "ABBBGGGBA", "ABBBGGGBA", "AABBBBBA.", ".AABBAA..", "...AAA...", "........."], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    flat: { rows: ["AAAAAAAAA", "ABBBBGGBA", "ABBBGGGBA", "ABBGGGGBA", "ABBGGGBBA", "ABBBGBBBA", "ABBBBBBBA", "ABBBBBBBA", "AAAAAAAAA"], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    find: { rows: ["....A....", "....A....", "...AAA...", "AAABBBAAA", "AABBBBAAA", "AAABBBAAA", "...AAA...", "....A....", "....A...."], palette: { A: "#dceaf2", B: "#36b6e8" } },
} as const
const image = computed(() => map.value && fields.value ? mapImage(fields.value, map.value, overlay.value) : null)
const sun = computed(() => map.value && position.value ? {
    declination: declination(map.value.tilt, position.value.date), color: star(map.value.star).color,
} : null)
const globe = ref<InstanceType<typeof MapGlobe> | null>(null)
const flatMarker = ref<{ x: number; z: number } | null>(null)
const explored = computed(() => {
    if (!map.value || map.value.mode === "creative") return "Whole planet"
    const bytes = Uint8Array.from(atob(map.value.explored), (c) => c.charCodeAt(0))
    let count = 0
    for (const byte of bytes) {
        let bits = byte
        while (bits) { bits &= bits - 1; count++ }
    }
    return `${(100 * count / (map.value.grid * map.value.grid)).toFixed(1)}% explored`
})

watch(() => map.value?.code, (code) => { if (code) void data.ensure(bridge, code) }, { immediate: true })
watch(() => [pose.value?.u, pose.value?.v, map.value?.size], ([u, v, size]) => {
    if (u === undefined || v === undefined || size === undefined) return
    flatMarker.value = { x: u * size, z: v * size }
}, { immediate: true })
</script>

<template>
    <div class="map-screen">
        <Panel title="World map" class="map-panel">
            <template #actions><Button size="sm" @click="bridge.send('screen.close', {})">Done</Button></template>
            <div class="stage">
                <template v-if="map && image && fields && pose && sun">
                    <MapGlobe v-if="view === 'globe'" ref="globe" :image="image" :elevation="fields.elevation"
                        :pose="pose" :sun="sun" :initial-zoom="map.mode === 'creative' ? 1 : 4" />
                    <div v-else class="flat-stage"><MapView v-model:spawn="flatMarker" :image="image" :size="map.size" :allow-spawn="false" /></div>
                </template>
                <div v-else class="pending">{{ data.problem.value ?? 'Charting planet…' }}</div>
                <nav class="view-tools" aria-label="Map views">
                    <IconTile label="Globe view" :size="36" :on="view === 'globe'" @click="view = 'globe'">
                        <PixelIcon :rows="viewIcons.globe.rows" :palette="viewIcons.globe.palette" style="--u: 2px" />
                    </IconTile>
                    <IconTile label="Flat map" :size="36" :on="view === 'flat'" @click="view = 'flat'">
                        <PixelIcon :rows="viewIcons.flat.rows" :palette="viewIcons.flat.palette" style="--u: 2px" />
                    </IconTile>
                    <IconTile v-if="view === 'globe'" label="Find me" :size="36" @click="globe?.recenter()">
                        <PixelIcon :rows="viewIcons.find.rows" :palette="viewIcons.find.palette" style="--u: 2px" />
                    </IconTile>
                </nav>
                <nav class="overlays" aria-label="Planet overlays">
                    <IconTile v-for="item in overlays" :key="item.id" :label="item.label" side="left" :size="36"
                        :on="overlay === item.id" @click="overlay = item.id">
                        <PixelIcon :rows="OVERLAY_ICONS[item.id].rows" :palette="OVERLAY_ICONS[item.id].palette" style="--u: 2px" />
                    </IconTile>
                </nav>
                <div class="map-readout">
                    <strong>{{ explored }}</strong>
                    <span v-if="coordinates">{{ coordinates.x }}, {{ coordinates.z }}</span>
                    <span>Drag to explore · scroll to zoom · V or Esc to close</span>
                </div>
                <Notice v-if="data.problem.value" class="problem" tone="danger">{{ data.problem.value }}</Notice>
            </div>
        </Panel>
    </div>
</template>

<style scoped>
.map-screen { position: fixed; inset: 0; z-index: 5; display: grid; place-items: center; background: rgb(4 8 12 / .72); }
.map-panel { width: min(1440px, calc(100% - 28px)); height: min(900px, calc(100% - 28px)); }
.map-panel :deep(.body) { padding: 0; overflow: hidden; }
.stage { position: relative; width: 100%; height: 100%; overflow: hidden; background: #07111a; }
.stage > .map-globe { width: 100%; height: 100%; }
.flat-stage { width: 100%; height: 100%; display: grid; place-items: center; }
.flat-stage :deep(.map) { width: min(100%, 78vh); aspect-ratio: 1; border: 0; }
.view-tools, .overlays { position: absolute; z-index: 2; top: 14px; display: grid; gap: 6px; }
.view-tools { left: 14px; }
.overlays { right: 14px; grid-template-columns: repeat(2, 36px); }
.map-readout { position: absolute; z-index: 2; left: 14px; bottom: 14px; display: flex; flex-wrap: wrap; gap: 9px 20px; align-items: baseline; max-width: calc(100% - 28px); padding: 8px 10px; border: var(--b) solid var(--edge); background: rgb(12 18 24 / .9); color: var(--ink-dim); font: 10px var(--font-body); }
.map-readout strong { color: var(--accent-hi); font: 11px var(--font-display); text-transform: uppercase; }
.pending { display: grid; place-items: center; width: 100%; height: 100%; color: var(--ink-dim); font: 13px var(--font-display); }
.problem { position: absolute; left: 14px; bottom: 55px; }
</style>
