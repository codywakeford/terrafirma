<script setup lang="ts">
import { onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import { GlobeRenderer, type GlobePoint } from "../../worldgen/globe"
import type { UiValues } from "../../bridge"

const props = defineProps<{
    image: ImageData
    elevation: Float32Array
    pose: { u: number; v: number; heading: number; height: number }
    radius: number
    sea: number
    trajectory: UiValues["map_trajectory"] | null
    sun: { declination: number; color: readonly [number, number, number] }
    compact?: boolean
    initialZoom: number
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const frame = ref<HTMLElement | null>(null)
const spin = reactive({ lon: 0, tilt: 0 })
const zoom = ref(props.initialZoom)
let renderer: ReturnType<typeof GlobeRenderer> | null = null
let resize: ResizeObserver | null = null
let drag: { x: number; y: number } | null = null
let following = true
let queued: number | null = null
let hadTrajectory = !!props.trajectory

function fitTrajectory() {
    const points = props.trajectory?.points
    if (!points?.length) return
    const farthest = Math.max(1, ...points.map((point) => 1 + Math.max(0, point[2]) / props.radius))
    zoom.value = Math.min(zoom.value, Math.max(0.12, 0.91 / farthest))
}

function draw() {
    const { u, v, height } = props.pose
    renderer?.marker([u, v, (height - props.sea) / props.radius])
    renderer?.draw(spin, 0.08, props.sun, zoom.value)
}
function scheduleDraw() {
    if (queued !== null) return
    queued = window.setTimeout(() => { queued = null; draw() }, 32)
}

function recenter() {
    const { u, v } = props.pose
    spin.lon = -u * Math.PI * 2
    spin.tilt = Math.max(-1.5, Math.min(1.5, (v - 0.5) * Math.PI + (props.trajectory ? 0.45 : 0)))
    zoom.value = props.initialZoom
    fitTrajectory()
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
    zoom.value = Math.max(0.1, Math.min(8, zoom.value * (event.deltaY < 0 ? 1.2 : 1 / 1.2)))
    following = false
    draw()
}

watch(() => [props.image, props.elevation] as const, ([image, elevation]) => {
    renderer?.terrain(image, elevation)
    renderer?.trajectory(props.trajectory?.points.map((point): GlobePoint => [point[0], point[1], point[2] / props.radius]) ?? null)
    draw()
})
watch(() => props.sun, draw, { deep: true })
watch(() => props.trajectory, (trajectory) => {
    renderer?.trajectory(trajectory?.points.map((point): GlobePoint => [point[0], point[1], point[2] / props.radius]) ?? null)
    if (following && trajectory && !hadTrajectory)
        spin.tilt = Math.max(-1.5, Math.min(1.5, spin.tilt + 0.45))
    hadTrajectory = !!trajectory
    if (following) fitTrajectory()
    draw()
})
watch(() => props.pose, () => {
    if (props.compact && following) recenter()
    else scheduleDraw()
}, { deep: true })

onMounted(() => {
    renderer = GlobeRenderer({ canvas: canvas.value!, detail: props.compact ? 1 : 2 })
    renderer.terrain(props.image, props.elevation)
    renderer.trajectory(props.trajectory?.points.map((point): GlobePoint => [point[0], point[1], point[2] / props.radius]) ?? null)
    recenter()
    resize = new ResizeObserver(draw)
    resize.observe(frame.value!)
})
onBeforeUnmount(() => { resize?.disconnect(); if (queued !== null) clearTimeout(queued) })
defineExpose({ recenter, zoom })
</script>

<template>
    <div ref="frame" class="map-globe" :class="{ compact }">
        <canvas ref="canvas" @pointerdown="onDown" @pointermove="onMove" @pointerup="onUp" @pointercancel="onUp" @wheel.prevent="onWheel" />
    </div>
</template>

<style scoped>
.map-globe { position: relative; width: 100%; height: 100%; overflow: hidden; background: radial-gradient(circle, #18344a 0%, #08121b 62%, #060d15 100%); }
canvas { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
canvas:active { cursor: grabbing; }
.compact { clip-path: circle(49% at center); }
.compact canvas { cursor: default; pointer-events: none; }
</style>
