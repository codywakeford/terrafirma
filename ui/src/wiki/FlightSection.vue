<script setup lang="ts">
import { computed, ref, shallowRef, watch } from "vue"
import type { Intents } from "../bridge"
import { useBridge } from "../bridge/use"
import { Segmented, Slider } from "../kit"
import WikiLayout from "./WikiLayout.vue"

/**
 * Newton's cannonball on this world's planet: fire from the equator and the
 * engine flies it (the same flight as the game's craft), through the real air
 * if you leave it on. Slow, it falls back; fast enough above the air, the
 * ground curves away as fast as it falls, and it's in orbit.
 */
type Shot = Intents["orbit.coast"]["response"]
const bridge = useBridge()

const speed = ref(1500)       // m/s over the ground
const angle = ref(0)          // degrees above the horizon
const altitude = ref(100)     // km
const heading = ref<"east" | "west">("east")
const air = ref<"on" | "off">("on")
const shot = shallowRef<Shot | null>(null)
const problem = ref<string | null>(null)

const BALL = { mass: 50, drag_area: 0.02 } // a 50 kg iron ball
let busy = false, again = false
async function fire() {
    if (busy) { again = true; return }
    busy = true
    try {
        shot.value = await bridge.send("orbit.coast", {
            altitude: altitude.value * 1000, speed: speed.value,
            angle: heading.value === "east" ? angle.value : 180 - angle.value,
            mass: BALL.mass, drag_area: air.value === "on" ? BALL.drag_area : 0, seconds: 4 * 3600,
        })
        problem.value = null
    } catch (err) {
        problem.value = err instanceof Error ? err.message : String(err)
    }
    busy = false
    if (again) { again = false; fire() }
}
watch([speed, angle, altitude, heading, air], fire, { immediate: true })

// the picture: planet radii to pixels, the planet in the middle
const SIZE = 520, PLANET = 150
const px = (x: number, y: number) => `${(SIZE / 2 + x * PLANET).toFixed(1)},${(SIZE / 2 - y * PLANET).toFixed(1)}`
const path = computed(() => shot.value ? "M" + shot.value.points.map(([x, y]) => px(x, y)).join("L") : "")
const airEdge = computed(() => shot.value ? PLANET * (1 + 8 * shot.value.scale_height / shot.value.radius) : PLANET)
const launch = computed(() => px(0, 1 + altitude.value * 1000 / (shot.value?.radius ?? 1)))

const km = (m: number) => `${Math.round(m / 1000)} km`
const outcome = computed(() => {
    const s = shot.value
    if (!s) return ""
    const minutes = Math.round(s.seconds / 60)
    if (s.end === "ground") return `It comes down after ${minutes} min.`
    if (s.end === "around") return `It goes all the way round in ${minutes} min: an orbit.`
    return `Still flying after ${minutes} min${s.bound ? "" : ": it has left the planet behind"}.`
})
</script>

<template>
    <WikiLayout wide>
        <template #list>
            <section class="controls">
                <h3>Newton's cannon</h3>
                <Slider v-model="speed" label="Speed over the ground" :min="0" :max="4000" :step="25" :format="(v) => `${v} m/s`" />
                <Slider v-model="angle" label="Above the horizon" :min="0" :max="90" :step="1" :format="(v) => `${v}°`" />
                <Slider v-model="altitude" label="Fired from" :min="0" :max="400" :step="5" :format="(v) => `${v} km up`" />
                <Segmented v-model="heading" :options="[{ value: 'east', label: 'East' }, { value: 'west', label: 'West' }]" />
                <Segmented v-model="air" :options="[{ value: 'on', label: 'Air' }, { value: 'off', label: 'No air' }]" />
                <p class="note">East, the planet's spin is on your side: its equator already moves ~460 m/s that way.</p>
            </section>
        </template>
        <template #view>
            <div class="stage">
                <svg :viewBox="`0 0 ${SIZE} ${SIZE}`" class="picture" aria-label="The planet, its air and the cannonball's path">
                    <circle :cx="SIZE / 2" :cy="SIZE / 2" :r="airEdge" class="air" />
                    <circle :cx="SIZE / 2" :cy="SIZE / 2" :r="PLANET" class="planet" />
                    <path :d="path" class="path" />
                    <circle :cx="launch.split(',')[0]" :cy="launch.split(',')[1]" r="4" class="cannon" />
                </svg>
                <div v-if="shot" class="readout">
                    <p class="outcome">{{ outcome }}</p>
                    <p>Speed against the stars <b>{{ Math.round(shot.orbital_speed) }} m/s</b></p>
                    <p>Lowest point <b>{{ km(shot.periapsis) }}</b><template v-if="shot.apoapsis !== undefined"> · highest <b>{{ km(shot.apoapsis) }}</b></template></p>
                    <p class="note">The planet is {{ km(shot.radius) }} in radius; its air thins tenfold every {{ (shot.scale_height * 2.3 / 1000).toFixed(0) }} km.</p>
                </div>
                <p v-if="problem" class="problem">{{ problem }}</p>
            </div>
        </template>
    </WikiLayout>
</template>

<style scoped>
.controls { display: flex; flex-direction: column; gap: 14px; }
h3 { margin: 0; }
.note { margin: 0; font-size: 11px; color: var(--ink-dim); }
.stage { display: flex; gap: 18px; align-items: flex-start; flex-wrap: wrap; }
.picture { width: min(100%, 520px); background: #05070d; border: var(--b) solid var(--edge); }
.air { fill: rgb(90 150 230 / 0.18); }
.planet { fill: #3c6e46; stroke: #2a4f33; stroke-width: 2; }
.path { fill: none; stroke: var(--accent-hi); stroke-width: 2; }
.cannon { fill: #fff; }
.readout { display: flex; flex-direction: column; gap: 6px; max-width: 260px; font-size: 12px; }
.readout p { margin: 0; }
.outcome { font: 14px var(--font-display); color: var(--accent-hi); }
b { font-weight: 400; font-family: var(--font-display); }
.problem { color: var(--danger); }
</style>
