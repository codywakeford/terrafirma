<script setup lang="ts">
import { computed } from "vue"
import type { FlightSpace } from "../../bridge"

/**
 * In space: the orbit you're on, as a pilot reads it. The diagram is the
 * orbit's shape in its own plane (planet at the focus, the boundary dashed,
 * you on it), drawn from the elements; the world itself is what's around you.
 */
const props = defineProps<{ flight: FlightSpace }>()

const SIZE = 120 // px: the diagram's box

const km = (m: number) => `${(m / 1000).toFixed(Math.abs(m) < 10000 ? 2 : 0)} km`
const minutes = (s: number) => `${Math.floor(s / 60)}m ${Math.round(s % 60)}s`

const status = computed(() => {
    const f = props.flight
    if (f.apoapsis === undefined) return "escaping the planet"
    if (f.periapsis < f.boundary) return "falling back: this orbit comes down"
    return "in orbit"
})

// The ellipse r(θ) = a(1−e²)/(1+e·cosθ), periapsis to the right, and where you are on it.
const diagram = computed(() => {
    const f = props.flight
    if (f.apoapsis === undefined) return null
    const R = f.radius
    const ra = R + f.apoapsis, rp = R + Math.max(f.periapsis, -R + 1)
    const a = (ra + rp) / 2, e = (ra - rp) / (ra + rp), p = a * (1 - e * e)
    const k = (SIZE / 2 - 4) / ra // metres to px, the far side fitting
    const at = (r: number, th: number) => ({ x: SIZE / 2 + r * Math.cos(th) * k, y: SIZE / 2 - r * Math.sin(th) * k })
    let d = ""
    for (let i = 0; i <= 96; i++) {
        const th = (i / 96) * 2 * Math.PI, pt = at(p / (1 + e * Math.cos(th)), th)
        d += `${i ? "L" : "M"}${pt.x.toFixed(1)},${pt.y.toFixed(1)}`
    }
    const r = R + f.altitude
    const cos = e > 1e-6 ? Math.max(-1, Math.min(1, (p / r - 1) / e)) : 1
    const nu = f.climb >= 0 ? Math.acos(cos) : -Math.acos(cos)
    return { d: d, planet: R * k, boundary: (R + f.boundary) * k, you: at(r, nu) }
})
</script>

<template>
    <div class="space">
        <svg v-if="diagram" :width="SIZE" :height="SIZE" :viewBox="`0 0 ${SIZE} ${SIZE}`" aria-hidden="true">
            <circle :cx="SIZE / 2" :cy="SIZE / 2" :r="diagram.boundary" class="boundary" />
            <circle :cx="SIZE / 2" :cy="SIZE / 2" :r="diagram.planet" class="planet" />
            <path :d="diagram.d" class="orbit" />
            <circle :cx="diagram.you.x" :cy="diagram.you.y" r="3.5" class="you" />
        </svg>
        <div class="read">
            <h3 :class="{ low: status !== 'in orbit' }">{{ status }}</h3>
            <dl>
                <dt>Altitude</dt><dd>{{ km(flight.altitude) }} <span class="dim">{{ flight.climb >= 0 ? "▲" : "▼" }} {{ Math.abs(Math.round(flight.climb)) }} m/s</span></dd>
                <dt>Speed</dt><dd>{{ Math.round(flight.orbital_speed) }} m/s <span class="dim">({{ Math.round(flight.speed) }} over the ground)</span></dd>
                <dt>Apoapsis</dt><dd>{{ flight.apoapsis !== undefined ? km(flight.apoapsis) : "—" }}</dd>
                <dt>Periapsis</dt><dd :class="{ low: flight.periapsis < flight.boundary }">{{ km(flight.periapsis) }}</dd>
                <dt>Period</dt><dd>{{ flight.period !== undefined ? minutes(flight.period) : "—" }}</dd>
                <dt>Inclination</dt><dd>{{ (flight.inclination * 180 / Math.PI).toFixed(1) }}°</dd>
                <dt>Air</dt><dd>{{ flight.density < 1e-9 ? "none to speak of" : `${flight.density.toExponential(1)} kg/m³` }}</dd>
            </dl>
            <p class="keys">
                {{ flight.riding
                    ? flight.lone ? "W/S where you look · A/D · Space/Shift · Ctrl: more, or time ×20 coasting" : "sharing this bubble: your thrust moves you about it"
                    : "floating · walk keys move you · right-click the pack to strap in" }}
            </p>
            <p v-if="!flight.buildable" class="keys dim">below {{ km(flight.boundary) }} at its lowest: no building here</p>
        </div>
    </div>
</template>

<style scoped>
.space { position: absolute; top: 14px; left: 50%; transform: translateX(-50%); display: flex; gap: 12px; align-items: flex-start;
    padding: 8px 12px; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.86); box-shadow: var(--raised);
    color: var(--ink); font-size: 12px; }
.boundary { fill: none; stroke: var(--ink-faint); stroke-dasharray: 2 3; }
.planet { fill: #1d4a6b; stroke: var(--edge); }
.orbit { fill: none; stroke: var(--accent-hi); stroke-width: 1.5; }
.you { fill: #fff; stroke: var(--accent); stroke-width: 1.5; }
.read { width: 250px; }
h3 { margin: 0 0 6px; font: 400 12px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
dl { display: grid; grid-template-columns: auto 1fr; gap: 2px 10px; margin: 0; }
dt { color: var(--ink-dim); }
dd { margin: 0; font-family: var(--font-display); }
.low { color: var(--danger); }
.keys { margin: 6px 0 0; font-size: 11px; color: var(--ink-dim); }
.dim { color: var(--ink-faint); }
</style>
