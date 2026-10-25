<script setup lang="ts">
import { computed, watch } from "vue"
import { useBridge } from "../../bridge/use"
import { useStore } from "../store"
import { widgetConfig } from "./config"
import WidgetFrame from "./WidgetFrame.vue"
import { mapImage, worldMapData } from "../map/data"
import LocalMap from "../map/LocalMap.vue"
import { mapUv } from "../map/data"

const bridge = useBridge()
const store = useStore()
const data = worldMapData()
const map = computed(() => store.ui.map)
const position = computed(() => store.ui.map_position)
const fields = computed(() => {
    const current = data.preview.value
    return current && current.code === map.value?.code ? current.fields : null
})
const image = computed(() => map.value && fields.value ? mapImage(fields.value, map.value) : null)
const pose = computed(() => store.state.mapPose ?? (position.value && map.value ? {
    ...mapUv(position.value.x, position.value.z, map.value.size), heading: 0,
} : null))
const width = computed(() => 128 + widgetConfig.mapScale * 24)

watch(() => [map.value?.code, widgetConfig.ready, widgetConfig.map] as const, ([code, ready, enabled]) => {
    if (code && ready && enabled) void data.ensure(bridge, code)
}, { immediate: true })
</script>

<template>
    <WidgetFrame v-if="widgetConfig.ready && widgetConfig.map && map && store.state.hud?.visible && !store.screen"
        title="Map" :corner="widgetConfig.mapCorner" interactive bare>
        <button class="open-map" :style="{ width: `${width}px` }" aria-label="Open world map"
            @click="bridge.send('screen.open', { name: 'map' })">
            <LocalMap v-if="image && pose" :image="image" :pose="pose" :size="map.size" :round="widgetConfig.mapShape === 1" />
            <div v-else class="loading">{{ data.problem.value ?? 'Charting planet…' }}</div>
            <span class="readout"><span v-if="position">{{ Math.round(position.x) }}, {{ Math.round(position.z) }}</span><span>V · OPEN</span></span>
        </button>
    </WidgetFrame>
</template>

<style scoped>
.open-map { display: block; position: relative; padding: 4px; border: var(--b) solid var(--edge); box-shadow: var(--raised), 4px 4px 0 rgb(0 0 0 / .35); background: #111820; color: var(--ink); text-align: left; }
.open-map > :first-child { aspect-ratio: 1; }
.open-map:hover { outline: var(--b) solid var(--accent-hi); }
.readout { position: absolute; left: 4px; right: 4px; bottom: 4px; display: flex; justify-content: space-between; gap: 8px; padding: 6px 7px 4px; font: 9px var(--font-display); white-space: nowrap; color: white; background: linear-gradient(transparent, rgb(4 9 13 / .84)); text-shadow: 1px 1px #050b10; }
.loading { display: grid; place-items: center; font: 10px var(--font-display); color: var(--ink-dim); }
</style>
