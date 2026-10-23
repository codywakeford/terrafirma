<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { body as bodyOf, moonsOf, planets, STAR, type Body, type SystemHandle } from "../system/model"
import { useSystemPreview } from "../system/preview"
import { star as starInfo, type StarId } from "./config"

type Vec3 = { x: number; y: number; z: number }
type Point = { x: number; y: number; depth: number; scale: number }
type Pick = { id: string; x: number; y: number; radius: number }
const ORIGIN: Vec3 = { x: 0, y: 0, z: 0 }
const TWO_PI = Math.PI * 2

const props = defineProps<{
    date: number
    year: number
    star: StarId
    system: SystemHandle
    surfaces: readonly { id: string; image: ImageData }[]
}>()
const selected = defineModel<string>({ required: true })
const cards = useSystemPreview()
const canvas = ref<HTMLCanvasElement | null>(null)
const chosen = computed<Body | null>(() => (selected.value === STAR ? null : bodyOf(props.system, selected.value)))
const card = computed(() => (chosen.value ? cards.card(chosen.value.id) : null))
const extent = computed(() => Math.max(3, ...planets(props.system).map((body) => displayOrbit(body.orbit))) + 0.4)
let observer: ResizeObserver | null = null
let picks: Pick[] = []
let drag: { x: number; y: number; moved: boolean } | null = null
let yaw = 0.55, pitch = 0.48, zoom = 1

// Distances are compressed for legibility. The camera and orbital planes are 3D;
// the orbit model is still a concept until system codes exist.
function displayOrbit(distance: number) { return 0.48 + Math.log2(1 + distance) * 0.88 }
function orbitRadius(mean: number, eccentricity: number, angle: number) {
    return mean * (1 - eccentricity * eccentricity) / (1 + eccentricity * Math.cos(angle))
}
function planetPosition(body: Body): Vec3 {
    const angle = TWO_PI * (props.date / Math.sqrt(body.orbit ** 3) + body.phase)
    const radius = orbitRadius(displayOrbit(body.orbit), body.eccentricity, angle)
    const inclination = body.inclination * Math.PI / 180
    return { x: radius * Math.cos(angle), y: radius * Math.sin(angle) * Math.sin(inclination), z: radius * Math.sin(angle) * Math.cos(inclination) }
}
function position(body: Body | null): Vec3 {
    if (body === null) return ORIGIN // the star
    if (body.parent === null) return planetPosition(body)
    const parent = bodyOf(props.system, body.parent)
    const center = planetPosition(parent)
    const config = body
    const parentMass = 10 ** parent.mass
    const period = Math.max(1 / props.year, 0.96 / props.year * Math.sqrt(config.orbit ** 3 / parentMass))
    const angle = TWO_PI * (props.date / period + body.phase)
    const radius = orbitRadius(0.15 + config.orbit * 0.012, config.eccentricity, angle)
    const inclination = config.inclination * Math.PI / 180
    return { x: center.x + radius * Math.cos(angle), y: center.y + radius * Math.sin(angle) * Math.sin(inclination),
        z: center.z + radius * Math.sin(angle) * Math.cos(inclination) }
}

function camera(point: Vec3, width: number, height: number): Point {
    const cy = Math.cos(yaw), sy = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch)
    const right = point.x * cy - point.z * sy
    const up = -point.x * sy * sp + point.y * cp - point.z * cy * sp
    const depth = point.x * sy * cp + point.y * sp + point.z * cy * cp
    const perspective = 7 / (7 - depth)
    const scale = Math.min(width, height) * 0.46 / extent.value * zoom * perspective
    return { x: width * 0.5 + right * scale, y: height * 0.46 - up * scale, depth, scale: perspective }
}

function sphere(image: ImageData, lightX: number, lightY: number) {
    const size = 48, r = size / 2 - 1
    const tile = document.createElement("canvas")
    tile.width = tile.height = size
    const ctx = tile.getContext("2d")!
    const out = ctx.createImageData(size, size)
    const lightLength = Math.hypot(lightX, lightY, 0.8)
    for (let py = 0; py < size; py++)
        for (let px = 0; px < size; px++) {
            const x = (px + 0.5 - size / 2) / r, y = (py + 0.5 - size / 2) / r
            const d = x * x + y * y
            if (d > 1) continue
            const z = Math.sqrt(1 - d)
            const u = ((Math.atan2(x, z) / TWO_PI) % 1 + 1) % 1
            const v = Math.asin(y) / Math.PI + 0.5
            const source = (Math.floor(v * (image.height - 1)) * image.width + Math.floor(u * (image.width - 1))) * 4
            const lit = 0.18 + 0.82 * Math.max(0, (x * lightX + y * lightY + z * 0.8) / lightLength)
            const dest = (py * size + px) * 4
            for (let c = 0; c < 3; c++) out.data[dest + c] = image.data[source + c]! * lit
            out.data[dest + 3] = 255
        }
    ctx.putImageData(out, 0, 0)
    return tile
}

function draw() {
    const el = canvas.value
    if (!el) return
    const width = el.clientWidth, height = el.clientHeight
    if (!width || !height) return
    const dpr = window.devicePixelRatio || 1
    el.width = Math.round(width * dpr)
    el.height = Math.round(height * dpr)
    const ctx = el.getContext("2d")!
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    ctx.clearRect(0, 0, width, height)
    const center = camera(ORIGIN, width, height)

    ctx.lineWidth = 1
    ctx.setLineDash([2, 5])
    for (const body of planets(props.system)) {
        const config = body
        const mean = displayOrbit(config.orbit)
        const tilt = config.inclination * Math.PI / 180
        ctx.beginPath()
        for (let i = 0; i <= 96; i++) {
            const angle = i / 96 * TWO_PI
            const radius = orbitRadius(mean, config.eccentricity, angle)
            const point = camera({ x: radius * Math.cos(angle), y: radius * Math.sin(angle) * Math.sin(tilt),
                z: radius * Math.sin(angle) * Math.cos(tilt) }, width, height)
            if (i === 0) ctx.moveTo(point.x, point.y)
            else ctx.lineTo(point.x, point.y)
        }
        ctx.strokeStyle = body.id === selected.value || moonsOf(props.system, body.id).some((moon) => moon.id === selected.value) ? "#53b5dc" : "#344650"
        ctx.stroke()
    }
    const selectedParent = chosen.value === null ? null : chosen.value.parent === null ? chosen.value : bodyOf(props.system, chosen.value.parent)
    if (selectedParent) {
        const origin = position(selectedParent)
        for (const moon of moonsOf(props.system, selectedParent.id)) {
            const config = moon
            const mean = 0.15 + config.orbit * 0.012
            ctx.beginPath()
            for (let i = 0; i <= 48; i++) {
                const angle = i / 48 * TWO_PI
                const radius = orbitRadius(mean, config.eccentricity, angle)
                const point = camera({ x: origin.x + radius * Math.cos(angle), y: origin.y,
                    z: origin.z + radius * Math.sin(angle) }, width, height)
                if (i === 0) ctx.moveTo(point.x, point.y)
                else ctx.lineTo(point.x, point.y)
            }
            ctx.strokeStyle = "#405764"
            ctx.stroke()
        }
    }
    ctx.setLineDash([])

    picks = []
    const everything: (Body | null)[] = [null, ...props.system.bodies]
    const visible = everything.map((body) => ({ body, point: camera(position(body), width, height) }))
        .sort((a, b) => a.point.depth - b.point.depth)
    for (const { body, point } of visible) {
        // sizes from the body model's radius once it has answered
        const radiusOf = body ? cards.card(body.id)?.radius ?? 1 : 1
        const baseRadius = body === null ? 14 : body.parent !== null ? 4.5 : 9
        const radius = body ? baseRadius * Math.max(0.55, Math.min(1.9, radiusOf ** 0.3)) : baseRadius
        const size = Math.max(3, radius * point.scale * zoom)
        const id = body ? body.id : STAR
        if (body === null) {
            ctx.shadowColor = `rgb(${starInfo(props.star).color.join(" ")})`
            ctx.shadowBlur = 18 + props.system.activity * 25
            ctx.fillStyle = ctx.shadowColor
            ctx.beginPath(); ctx.arc(point.x, point.y, size, 0, TWO_PI); ctx.fill()
            ctx.shadowBlur = 0
        } else {
            // until its card arrives, a body is a dot in its own colour
            const texture = props.surfaces.find((item) => item.id === body.id)?.image
            if (texture) {
                const tile = sphere(texture, center.x - point.x, center.y - point.y)
                ctx.imageSmoothingEnabled = false
                ctx.drawImage(tile, point.x - size, point.y - size, size * 2, size * 2)
            } else {
                ctx.fillStyle = body.color
                ctx.beginPath(); ctx.arc(point.x, point.y, size, 0, TWO_PI); ctx.fill()
            }
        }
        if (id === selected.value) {
            ctx.strokeStyle = "#40b8ec"
            ctx.lineWidth = 1.5
            ctx.beginPath(); ctx.arc(point.x, point.y, size + 5, 0, TWO_PI); ctx.stroke()
        }
        if (body === null || body.parent === null) {
            ctx.fillStyle = "#e8ecef"
            ctx.font = '10px "Silkscreen", monospace'
            ctx.textAlign = "center"
            ctx.fillText(body ? body.name : starInfo(props.star).label, point.x, point.y - size - 10)
        }
        picks.push({ id: id, x: point.x, y: point.y, radius: Math.max(size + 6, 9) })
    }
    ctx.fillStyle = "rgb(232 236 239 / 0.5)"
    ctx.font = '10px "JetBrains Mono", monospace'
    ctx.textAlign = "right"
    ctx.fillText("DRAG TO ORBIT · SCROLL TO ZOOM", width - 8, 16)
}

function onDown(event: PointerEvent) {
    drag = { x: event.clientX, y: event.clientY, moved: false }
    canvas.value?.setPointerCapture(event.pointerId)
}
function onMove(event: PointerEvent) {
    if (!drag) return
    const dx = event.clientX - drag.x, dy = event.clientY - drag.y
    if (Math.abs(dx) + Math.abs(dy) > 2) drag.moved = true
    if (drag.moved) {
        yaw += dx * 0.008
        pitch = Math.max(-1.35, Math.min(1.35, pitch - dy * 0.008))
        draw()
    }
    drag.x = event.clientX; drag.y = event.clientY
}
function onUp(event: PointerEvent) {
    if (drag && !drag.moved) {
        const rect = canvas.value!.getBoundingClientRect()
        const x = event.clientX - rect.left, y = event.clientY - rect.top
        const nearest = [...picks].reverse().find((pick) => Math.hypot(x - pick.x, y - pick.y) < pick.radius)
        if (nearest) selected.value = nearest.id
    }
    drag = null
}
function onWheel(event: WheelEvent) {
    zoom = Math.max(0.65, Math.min(2, zoom * (event.deltaY < 0 ? 1.12 : 1 / 1.12)))
    draw()
}

watch(() => [props.date, props.year, props.star, props.surfaces, props.system.bodies, selected.value, cards.cards.value], draw, { deep: true })
onMounted(() => {
    observer = new ResizeObserver(draw)
    observer.observe(canvas.value!)
    draw()
})
onBeforeUnmount(() => observer?.disconnect())
</script>

<template>
    <div class="system-view">
        <canvas ref="canvas" @pointerdown="onDown" @pointermove="onMove" @pointerup="onUp" @pointercancel="drag = null"
            @wheel.prevent="onWheel" aria-label="Interactive three-dimensional star system" />
        <div class="readout">
            <span class="eyebrow">{{ chosen === null ? 'star' : chosen.parent ? 'moon' : 'planet' }}{{ card ? ` · ${card.kind}` : '' }}</span>
            <strong>{{ chosen?.name ?? starInfo(star).label }}</strong>
            <span v-if="chosen && card" class="facts">{{ (10 ** chosen.mass).toPrecision(2) }} Earth masses · {{ card.gravity.toFixed(2) }}g · {{ Math.round(card.temperature - 273) }} °C</span>
            <span v-else-if="chosen === null" class="facts">{{ system.age.toFixed(1) }} billion years · wind {{ Math.round(system.activity * 100) }}%</span>
        </div>
    </div>
</template>

<style scoped>
.system-view { position: relative; width: 100%; height: 100%; background: radial-gradient(ellipse at 50% 45%, #172632, transparent 70%); }
canvas { display: block; width: 100%; height: 100%; cursor: grab; touch-action: none; }
canvas:active { cursor: grabbing; }
.readout { position: absolute; bottom: 0; right: 0; display: flex; flex-direction: column; gap: 2px; width: min(75%, 330px); min-height: 90px; padding: 8px 10px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.88); box-shadow: var(--sunk); pointer-events: none; }
.eyebrow { font: 10px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
strong { font: 14px var(--font-display); text-transform: uppercase; }
.note, .facts { color: var(--ink-dim); font-size: 10px; line-height: 1.25; }
.facts { color: var(--ink); }
</style>
