<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import { GlobeRenderer } from "./globe"

/**
 * The world as a planet: drag to spin. The world wraps east–west (longitude)
 * and runs pole to pole north–south, so this is its true shape; the relief is
 * the terrain's real height, exaggerated by `relief`, lit by its sun.
 */
const props = defineProps<{
    image: ImageData
    elevation: Float32Array
    relief: number
    sun: { declination: number; color: readonly [number, number, number] }
    rotationPeriod?: number
}>()

const canvas = ref<HTMLCanvasElement | null>(null)
const spin = { lon: 0, tilt: -0.3 }
let renderer: ReturnType<typeof GlobeRenderer> | null = null
let drag: { x: number; y: number } | null = null
let auto = true
let frame = 0

function draw() {
    renderer?.draw(spin, props.relief, props.sun)
}

function tick() {
    if (auto && !drag) {
        spin.lon += 0.003 * 24 / (props.rotationPeriod ?? 24)
        draw()
    }
    frame = requestAnimationFrame(tick)
}

function onDown(e: MouseEvent) {
    drag = { x: e.clientX, y: e.clientY }
    auto = false // the player has taken over the spin
}

function onMove(e: MouseEvent) {
    if (!drag) return
    spin.lon += (e.clientX - drag.x) * 0.01
    spin.tilt = Math.max(-1.3, Math.min(1.3, spin.tilt - (e.clientY - drag.y) * 0.01))
    drag = { x: e.clientX, y: e.clientY }
    draw()
}

function onUp() {
    drag = null
}

watch(() => [props.image, props.elevation] as const, ([image, elevation]) => {
    renderer?.terrain(image, elevation)
    draw()
})
watch(() => [props.relief, props.sun], draw, { deep: true })

onMounted(() => {
    renderer = GlobeRenderer({ canvas: canvas.value! })
    renderer.terrain(props.image, props.elevation)
    draw()
    frame = requestAnimationFrame(tick)
})
onBeforeUnmount(() => cancelAnimationFrame(frame))
</script>

<template>
    <canvas ref="canvas" class="globe" @mousedown="onDown" @mousemove="onMove" @mouseup="onUp" @mouseleave="onUp" />
</template>

<style scoped>
.globe { display: block; width: 100%; height: 100%; cursor: grab; }
</style>
