<script setup lang="ts">
import { onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import { GlobeRenderer } from "../../worldgen/globe"
import { mapUv } from "./data"

const props = defineProps<{
    image: ImageData
    elevation: Float32Array
    position: { x: number; z: number }
    size: number
    sun: { declination: number; color: readonly [number, number, number] }
    compact?: boolean
    initialZoom: number
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const frame = ref<HTMLElement | null>(null)
const spin = reactive({ lon: 0, tilt: 0 })
const zoom = ref(props.initialZoom)
const marker = ref<{ left: string; top: string } | null>(null)
let renderer: ReturnType<typeof GlobeRenderer> | null = null
let resize: ResizeObserver | null = null
let drag: { x: number; y: number } | null = null
let following = true

function projectPlayer() {
    const el = frame.value
    if (!el) return
    const { u, v } = mapUv(props.position.x, props.position.z, props.size)
    const lon = u * Math.PI * 2, lat = (v - 0.5) * Math.PI
    const nx = Math.cos(lat) * Math.sin(lon), ny = Math.sin(lat), nz = Math.cos(lat) * Math.cos(lon)
    const cy = Math.cos(spin.lon), sy = Math.sin(spin.lon), cx = Math.cos(spin.tilt), sx = Math.sin(spin.tilt)
    const x = cy * nx + sy * nz
    const y = sx * sy * nx + cx * ny - sx * cy * nz
    const z = -cx * sy * nx + sx * ny + cx * cy * nz
    const scale = 0.92 * zoom.value / 1.08
    const px = (1 + x * scale * el.clientHeight / el.clientWidth) * 50
    const py = (1 + y * scale) * 50
    marker.value = z > 0 && px >= 0 && px <= 100 && py >= 0 && py <= 100
        ? { left: `${px}%`, top: `${py}%` } : null
}

function draw() {
    renderer?.draw(spin, 0.08, props.sun, zoom.value)
    projectPlayer()
}

function recenter() {
    const { u, v } = mapUv(props.position.x, props.position.z, props.size)
    spin.lon = -u * Math.PI * 2
    spin.tilt = (v - 0.5) * Math.PI
    zoom.value = props.initialZoom
    following = true
    draw()
}

function onDown(event: PointerEvent) {
    if (props.compact) return
    drag = { x: event.clientX, y: event.clientY }
    following = false
    canvas.value?.setPointerCapture(event.pointerId)
}
function onMove(event: PointerEvent) {
    if (!drag) return
    const height = canvas.value?.clientHeight ?? 1
    spin.lon += (event.clientX - drag.x) * 2 / height / Math.max(1, zoom.value / 2)
    spin.tilt = Math.max(-1.5, Math.min(1.5,
        spin.tilt - (event.clientY - drag.y) * 2 / height / Math.max(1, zoom.value / 2)))
    drag = { x: event.clientX, y: event.clientY }
    draw()
}
function onUp() { drag = null }
function onWheel(event: WheelEvent) {
    if (props.compact) return
    zoom.value = Math.max(1, Math.min(8, zoom.value * (event.deltaY < 0 ? 1.2 : 1 / 1.2)))
    following = false
    draw()
}

watch(() => [props.image, props.elevation] as const, ([image, elevation]) => {
    renderer?.terrain(image, elevation)
    draw()
})
watch(() => props.sun, draw, { deep: true })
watch(() => props.position, () => {
    if (props.compact && following) recenter()
    else draw()
}, { deep: true })

onMounted(() => {
    renderer = GlobeRenderer({ canvas: canvas.value!, detail: props.compact ? 1 : 2 })
    renderer.terrain(props.image, props.elevation)
    recenter()
    resize = new ResizeObserver(draw)
    resize.observe(frame.value!)
})
onBeforeUnmount(() => { resize?.disconnect() })
defineExpose({ recenter, zoom })
</script>

<template>
    <div ref="frame" class="map-globe" :class="{ compact }">
        <canvas ref="canvas" @pointerdown="onDown" @pointermove="onMove" @pointerup="onUp" @pointercancel="onUp" @wheel.prevent="onWheel" />
        <span v-if="marker" class="player" :style="marker" aria-label="Your position"><span /></span>
    </div>
</template>

<style scoped>
.map-globe { position: relative; width: 100%; height: 100%; overflow: hidden; background: radial-gradient(circle, #18344a 0%, #08121b 62%, #060d15 100%); }
canvas { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
canvas:active { cursor: grabbing; }
.compact { clip-path: circle(49% at center); }
.compact canvas { cursor: default; pointer-events: none; }
.player { position: absolute; z-index: 1; width: 16px; height: 16px; transform: translate(-50%, -50%); display: grid; place-items: center; border: 2px solid #e8f7ff; background: #06141a; box-shadow: 0 0 0 2px #06141a, 0 0 14px var(--accent-hi); pointer-events: none; }
.player span { width: 6px; height: 6px; background: var(--accent-hi); }
.compact .player { width: 12px; height: 12px; }
.compact .player span { width: 4px; height: 4px; }
</style>
