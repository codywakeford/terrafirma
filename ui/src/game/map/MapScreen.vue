<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useBridge } from "../../bridge/use"
import { useStore } from "../store"
import { Button, Notice, Panel } from "../../kit"
import MapView from "../../worldgen/MapView.vue"
import { declination } from "../../worldgen/sun"
import { star } from "../../worldgen/config"
import { mapImage, worldMapData } from "./data"
import MapGlobe from "./MapGlobe.vue"

const bridge = useBridge()
const store = useStore()
const data = worldMapData()
const map = computed(() => store.ui.map)
const position = computed(() => store.ui.map_position)
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
watch(() => [position.value?.x, position.value?.z, map.value?.size], ([x, z, size]) => {
    if (x === undefined || z === undefined || size === undefined) return
    flatMarker.value = { x: ((x + size / 2) % size + size) % size, z: size / 2 - z }
}, { immediate: true })
</script>

<template>
    <div class="map-screen">
        <Panel title="World map" class="map-panel">
            <template #actions><Button size="sm" @click="bridge.send('screen.close', {})">Done</Button></template>
            <div class="layout">
                <div class="stage">
                    <template v-if="map && image && fields && position && sun">
                        <MapGlobe v-if="view === 'globe'" ref="globe" :image="image" :elevation="fields.elevation"
                            :position="position" :size="map.size" :sun="sun"
                            :initial-zoom="map.mode === 'creative' ? 1 : 4" />
                        <MapView v-else v-model:spawn="flatMarker" :image="image" :size="map.size" :allow-spawn="false" />
                    </template>
                    <div v-else class="pending">{{ data.problem.value ?? 'Charting planet…' }}</div>
                </div>
                <aside>
                    <div class="section"><span class="kicker">Planet / {{ map?.mode ?? '—' }}</span><h2>{{ explored }}</h2>
                        <p>Created from this world's terrain model. Your position is marked in blue.</p></div>
                    <div class="section"><span class="kicker">View</span>
                        <div class="pair"><Button size="sm" :variant="view === 'globe' ? 'primary' : 'plain'" @click="view = 'globe'">Globe</Button>
                            <Button size="sm" :variant="view === 'flat' ? 'primary' : 'plain'" @click="view = 'flat'">Flat</Button></div>
                        <Button v-if="view === 'globe'" size="sm" @click="globe?.recenter()">Find me</Button>
                        <p>Drag to explore · scroll to zoom · V or Esc to close</p></div>
                    <div class="section"><span class="kicker">Layers</span>
                        <div class="layers"><Button v-for="item in overlays" :key="item.id" size="sm"
                            :variant="overlay === item.id ? 'primary' : 'plain'" @click="overlay = item.id">{{ item.label }}</Button></div></div>
                    <div class="section coords"><span class="kicker">Your position</span>
                        <strong v-if="position">{{ Math.round(position.x) }}, {{ Math.round(position.z) }}</strong>
                        <span v-else>Waiting for position…</span></div>
                    <Notice v-if="data.problem.value" tone="danger">{{ data.problem.value }}</Notice>
                </aside>
            </div>
        </Panel>
    </div>
</template>

<style scoped>
.map-screen { position: fixed; inset: 0; z-index: 5; display: grid; place-items: center; background: rgb(4 8 12 / .72); }
.map-panel { width: min(1440px, calc(100% - 28px)); height: min(900px, calc(100% - 28px)); }
.map-panel :deep(.body) { padding: 0; overflow: hidden; }
.layout { display: grid; grid-template-columns: minmax(0, 1fr) 274px; width: 100%; height: 100%; min-height: 0; }
.stage { min-width: 0; min-height: 0; overflow: hidden; background: #07111a; }
.stage > * { width: 100%; height: 100%; }
.stage :deep(.map) { border: 0; }
.pending { display: grid; place-items: center; color: var(--ink-dim); font: 13px var(--font-display); }
aside { min-width: 0; overflow-y: auto; padding: 17px; background: var(--stone-1); border-left: var(--b) solid var(--edge); }
.section { display: grid; gap: 8px; padding: 0 0 18px; margin: 0 0 18px; border-bottom: 1px solid var(--stone-4); }
.kicker { color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
h2 { margin: 0; font: 15px var(--font-display); text-transform: uppercase; }
p { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.pair, .layers { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px; }
.pair :deep(.btn), .layers :deep(.btn) { min-width: 0; width: 100%; padding-inline: 5px; font-size: 10px; }
.section > :deep(.btn) { width: 100%; }
.coords strong { font: 13px var(--font-display); }
@media (max-width: 720px) { .layout { grid-template-columns: minmax(0, 1fr) 190px; } aside { padding: 10px; } }
</style>
