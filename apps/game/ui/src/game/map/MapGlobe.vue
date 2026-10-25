<script setup lang="ts">
import { onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import { GlobeRenderer } from "../../worldgen/globe"

const props = defineProps<{
    image: ImageData
    elevation: Float32Array
    pose: { u: number; v: number; heading: number }
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
    const { u, v } = props.pose
    const cy = Math.cos(spin.lon), sy = Math.sin(spin.lon), cx = Math.cos(spin.tilt), sx = Math.sin(spin.tilt)
    const scale = 0.92 * zoom.value / 1.08
    const project = (longitude: number, latitude: number) => {
        const lon = longitude * Math.PI * 2, lat = (latitude - 0.5) * Math.PI
        const nx = Math.cos(lat) * Math.sin(lon), ny = Math.sin(lat), nz = Math.cos(lat) * Math.cos(lon)
        const x = cy * nx + sy * nz
        const y = sx * sy * nx + cx * ny - sx * cy * nz
        const z = -cx * sy * nx + sx * ny + cx * cy * nz
        return { x: (1 + x * scale * el.clientHeight / el.clientWidth) * 50, y: (1 + y * scale) * 50, z }
    }
    const here = project(u, v)
    marker.value = here.z > 0 && here.x >= 0 && here.x <= 100 && here.y >= 0 && here.y <= 100
        ? { left: `${here.x}%`, top: `${here.y}%` } : null
}

function draw() {
    renderer?.draw(spin, 0.08, props.sun, zoom.value)
    projectPlayer()
}

function recenter() {
    const { u, v } = props.pose
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
watch(() => props.pose, () => {
    if (props.compact && following) recenter()
    else projectPlayer()
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
.player { position: absolute; z-index: 1; width: 20px; height: 20px; transform: translate(-50%, -50%); border: 3px solid #e9f8ff; border-radius: 50%; background: #06141a; box-shadow: 0 0 0 2px #06141a, 0 0 12px 3px rgb(35 173 230 / .5); display: grid; place-items: center; pointer-events: none; }
.player span { width: 6px; height: 6px; border-radius: 50%; background: var(--accent-hi); }
.compact .player { width: 16px; height: 16px; }
</style>
