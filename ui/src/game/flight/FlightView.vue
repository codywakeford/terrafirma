<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import type { FlightOrbit } from "../../bridge"
import { useBridge } from "../../bridge/use"
import { decodePreview, paint, type Fields } from "../../worldgen/fields"
import { GlobeRenderer } from "../../worldgen/globe"

/**
 * Above the ceiling: the planet itself, the craft over it and the orbit it's
 * on, with what a pilot needs (apoapsis, periapsis, period). The globe is the
 * world's own preview; the view follows the craft. Keys still fly the craft
 * (this sits on the HUD, not over it as a screen).
 */
const props = defineProps<{ flight: FlightOrbit }>()
const bridge = useBridge()

const canvas = ref<HTMLCanvasElement | null>(null)
const fields = shallowRef<Fields | null>(null)
const problem = ref<string | null>(null)
const box = ref({ w: 1, h: 1 })
let renderer: ReturnType<typeof GlobeRenderer> | null = null

const SUN = { declination: 0, color: [255, 244, 220] as const }
const R = computed(() => props.flight.radius)

// the map's (u, v) for a place: the same map terragen wraps the planet in
const uv = (lat: number, lon: number) => ({ u: (((lon / (2 * Math.PI)) % 1) + 1) % 1, v: 0.5 - lat / Math.PI })

// The view: the craft at the centre, the planet small enough to leave room for the orbit.
const view = computed(() => {
    const f = props.flight
    const reach = Math.min(3, 1 + Math.max(f.altitude, f.apoapsis ?? 3 * R.value) / R.value)
    const p = uv(f.latitude, f.longitude)
    return { spin: { lon: -p.u * 2 * Math.PI, tilt: (p.v - 0.5) * Math.PI }, size: 0.8 / reach }
})

// A place (and its height) to the screen, as the globe's shader draws it.
function project(lat: number, lon: number, altitude: number) {
    const { u, v } = uv(lat, lon)
    const glon = u * 2 * Math.PI, glat = (v - 0.5) * Math.PI, k = 1 + altitude / R.value
    const n = [Math.cos(glat) * Math.sin(glon) * k, Math.sin(glat) * k, Math.cos(glat) * Math.cos(glon) * k] as const
    const s = view.value.spin
    const cy = Math.cos(s.lon), sy = Math.sin(s.lon), cx = Math.cos(s.tilt), sx = Math.sin(s.tilt)
    const x = cy * n[0] + sy * n[2]
    const y = sx * sy * n[0] + cx * n[1] - sx * cy * n[2]
    const z = -cx * sy * n[0] + sx * n[1] + cx * cy * n[2]
    const { w, h } = box.value, scale = view.value.size
    const sxp = (x * scale * (h / w) + 1) / 2 * w, syp = (1 + y * scale) / 2 * h
    // hidden: behind the planet's disc
    const hidden = z < 0 && x * x + y * y < 1
    return { x: sxp, y: syp, hidden: hidden }
}

// the orbit as runs of visible and hidden points
const runs = computed(() => {
    const out: { d: string; hidden: boolean }[] = []
    let current: { d: string; hidden: boolean } | null = null
    for (const [lat, lon, alt] of props.flight.path) {
        const p = project(lat, lon, Math.max(alt, 0))
        if (!current || current.hidden !== p.hidden) {
            current = { d: `M${p.x.toFixed(1)},${p.y.toFixed(1)}`, hidden: p.hidden }
            out.push(current)
        } else current.d += `L${p.x.toFixed(1)},${p.y.toFixed(1)}`
    }
    return out
})
const craft = computed(() => project(props.flight.latitude, props.flight.longitude, props.flight.altitude))

function draw() {
    if (!renderer || !canvas.value) return
    box.value = { w: canvas.value.clientWidth, h: canvas.value.clientHeight }
    renderer.draw(view.value.spin, 0.02, SUN, view.value.size)
}

async function load(code: string) {
    try {
        fields.value = decodePreview(await bridge.send("worldgen.preview", { code: code, date: null, resolution: 256 }))
        renderer?.terrain(paint(fields.value, "terrain"), fields.value.elevation)
        draw()
    } catch (err) {
        problem.value = err instanceof Error ? err.message : String(err)
    }
}

onMounted(() => {
    renderer = GlobeRenderer({ canvas: canvas.value! })
    load(props.flight.code)
    window.addEventListener("resize", draw)
})
onBeforeUnmount(() => window.removeEventListener("resize", draw))
watch(() => props.flight, draw)

const km = (m: number) => `${(m / 1000).toFixed(m < 10000 ? 2 : 1)} km`
const minutes = (s: number) => `${Math.floor(s / 60)}m ${Math.round(s % 60)}s`
const status = computed(() => {
    const f = props.flight
    if (f.apoapsis === undefined) return "escaping the planet"
    if (f.periapsis < 0) return "coming down: this orbit meets the ground"
    if (f.periapsis < f.ceiling * 40) return "in the air at its lowest: it will slow and fall"
    return "in orbit"
})
</script>

<template>
    <div class="flight">
        <canvas ref="canvas" class="globe" />
        <svg class="over" :viewBox="`0 0 ${box.w} ${box.h}`" aria-hidden="true">
            <path v-for="(r, i) in runs" :key="i" :d="r.d" :class="{ hidden: r.hidden }" />
            <circle v-if="!craft.hidden" :cx="craft.x" :cy="craft.y" r="5" class="craft" />
        </svg>
        <p v-if="problem" class="problem">{{ problem }}</p>
        <div class="panel">
            <h3>{{ status }}</h3>
            <dl>
                <dt>Altitude</dt><dd>{{ km(flight.altitude) }}</dd>
                <dt>Speed</dt><dd>{{ Math.round(flight.orbital_speed) }} m/s <span class="dim">({{ Math.round(flight.speed) }} over the ground)</span></dd>
                <dt>Apoapsis</dt><dd>{{ flight.apoapsis !== undefined ? km(flight.apoapsis) : "—" }}</dd>
                <dt>Periapsis</dt><dd :class="{ low: flight.periapsis < 0 }">{{ km(flight.periapsis) }}</dd>
                <dt>Period</dt><dd>{{ flight.period !== undefined ? minutes(flight.period) : "—" }}</dd>
                <dt>Inclination</dt><dd>{{ (flight.inclination * 180 / Math.PI).toFixed(1) }}°</dd>
                <dt>Air</dt><dd>{{ flight.density < 1e-9 ? "none to speak of" : `${flight.density.toExponential(1)} kg/m³` }}</dd>
            </dl>
            <p class="keys">W/S along / against your orbit · Space/Shift up / down · A/D across it · Ctrl: time ×20</p>
            <p class="keys dim">below {{ km(flight.ceiling) }} the world takes you back</p>
        </div>
    </div>
</template>

<style scoped>
.flight { position: absolute; inset: 0; background: radial-gradient(ellipse at center, #0b1020 0%, #020308 75%); }
.globe { position: absolute; inset: 0; width: 100%; height: 100%; }
.over { position: absolute; inset: 0; width: 100%; height: 100%; }
.over path { fill: none; stroke: var(--accent-hi); stroke-width: 2; }
.over path.hidden { stroke-opacity: 0.25; stroke-dasharray: 4 5; }
.craft { fill: #fff; stroke: var(--accent); stroke-width: 2; }
.panel { position: absolute; right: 14px; top: 14px; width: 260px; padding: 10px 14px; border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.86); box-shadow: var(--raised); color: var(--ink); font-size: 12px; }
h3 { margin: 0 0 8px; font: 400 13px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
dl { display: grid; grid-template-columns: auto 1fr; gap: 3px 10px; margin: 0; }
dt { color: var(--ink-dim); }
dd { margin: 0; font-family: var(--font-display); }
dd.low { color: var(--danger); }
.keys { margin: 10px 0 0; font-size: 11px; color: var(--ink-dim); }
.dim { color: var(--ink-faint); }
.problem { position: absolute; left: 14px; top: 14px; color: var(--danger); }
</style>
