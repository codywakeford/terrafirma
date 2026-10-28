<script setup lang="ts">
import { computed, ref, toRef, watch } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { IconTile, Slider } from "../../kit/index.ts"
import { body as bodyOf, STAR, type Body } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import { OVERLAYS, paint, type OverlayId } from "../../worldgen/fields.ts"
import { OVERLAY_ICONS } from "../../worldgen/overlayIcons.ts"
import { paintWeather } from "../../worldgen/weather.ts"
import { season } from "../../worldgen/sun.ts"
import type { WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"
import { useBridge } from "../../bridge/use.ts"
import { encodeWorldCode } from "../../worldgen/config.ts"
import { usePlanetarium } from "../../planetarium/use.ts"
import { useStore } from "../../game/store.ts"

// frame: what the page covers of the window's sides (Dock's covers): the planet is framed in the rest
const props = defineProps<{ draft: WorldDraftHandle; frame: { left: number; right: number } }>()
const cards = useSystemPreview()
const store = useStore()
const looks = useLooks()
const previewer = looks.terrain
const date = looks.date
const selected = computed<string>({ get: () => props.draft.system.selected, set: (id) => { props.draft.system.selected = id } })
const chosen = computed<Body | null>(() => selected.value === STAR ? null : bodyOf(props.draft.system, selected.value))
const playable = computed(() => chosen.value?.playable ?? false)
const chosenCard = computed(() => chosen.value ? cards.card(chosen.value.id) : null)
const overlay = ref<OverlayId | "inside">("terrain")
const surfaceOverlay = computed<OverlayId>(() => overlay.value === "inside" ? "terrain" : overlay.value)
const VIEW_ICONS = {
    seams: { rows: ["..AAAAA..", ".A..B..A.", "A...B...A", "A...B...A", "ABBBBBBBA", "A...B...A", "A...B...A", ".A..B..A.", "..AAAAA.."], palette: { A: "#d5e0e8", B: "#f2c84b" } },
    inside: { rows: ["..AAAAA..", ".ABBBBBA.", "ABBCBBBBA", "ABCCDBBBA", "ABCDDBBBA", "ABCCDBBBA", ".ABBBBBA.", "..AAAAA..", "........."], palette: { A: "#d5e0e8", B: "#8b6955", C: "#c15b32", D: "#f2a03c" } },
} as const

watch(overlay, (value) => { if (value === "weather") looks.wantWeather() })
const paintings = new WeakMap<object, Map<OverlayId, ImageData>>() // by the world's fields
const image = computed(() => {
    const fields = previewer.fields.value
    if (!fields) return null
    if (surfaceOverlay.value !== "weather") {
        // painted once a world: switching back to an overlay is free
        let painted = paintings.get(fields)
        if (!painted) paintings.set(fields, painted = new Map())
        let picture = painted.get(surfaceOverlay.value)
        if (!picture) painted.set(surfaceOverlay.value, picture = paint(fields, surfaceOverlay.value))
        return picture
    }
    const weather = looks.weather.at(date.value)
    return weather ? paintWeather(fields, weather) : null
})
const day = computed(() => Math.floor(shownDate.value * props.draft.config.year) + 1)
// The globe is the native Planetarium under the page (src/shell/planetarium.h):
// the world's whole system, the camera round the body selected, its layer
// over the land. Shown for as long as the viewer is.
const planet = usePlanetarium({
    bridge: useBridge(),
    place: "whole", // the universe behind the whole screen, as behind the menu
    active: ref(true),
    frame: toRef(props, "frame"),
    body: computed(() => ({ code: encodeWorldCode(props.draft.config) })),
})
const playableId = computed(() => props.draft.system.bodies.find((b) => b.playable)!.id)
// the camera flies to the body selected: in from the menu's view when the
// screen opens, across the system when another is picked; out to the whole
// system for the star
watch(selected, (id, was) => {
    const seconds = was === undefined ? 2.5 : 2
    planet.fly(id === STAR ? { body: playableId.value, radii: 400, seconds: seconds } : { body: id, radii: 2.5, seconds: seconds })
}, { immediate: true })
// what's over the land: the overlay's painting (the playable world's), the
// body cut open, or the land itself
watch([overlay, image, selected], () => {
    if (overlay.value === "inside" && selected.value !== STAR) planet.layer("inside")
    else if (playable.value && overlay.value !== "terrain" && image.value) planet.layer(image.value)
    else planet.layer(null)
}, { immediate: true })
// a click on the playable world is where to start: its column, as the
// engine rounds a place to one (src/mapgen/terra_column.cpp: equirect)
watch(() => store.picked, (pick) => {
    if (!pick || !playable.value) return
    const size = props.draft.config.size, half = size / 2
    let x = Math.round(pick.longitude / (2 * Math.PI) * size)
    x -= Math.floor((x + half) / size) * size
    const z = Math.min(half, Math.max(1 - half, Math.round(pick.latitude / Math.PI * size)))
    props.draft.spawn = { x: x, z: z }
})
// the clock: running, the date follows the native clock (the planets move,
// the Earth turns); scrubbing stops it there, and the climate follows the date
const running = ref(true)
const shownDate = computed<number>({
    get: () => running.value && store.clock ? store.clock.date : date.value,
    set: (value) => {
        running.value = false
        date.value = value
        planet.time({ running: false, date: value })
    },
})
function toggleClock() {
    running.value = !running.value
    if (!running.value && store.clock) date.value = store.clock.date // stopped where it was
    planet.time({ running: running.value, date: running.value ? undefined : date.value })
}
const seams = ref(true)
watch(seams, (shown) => planet.seams(shown))
const kindNote = computed(() => chosenCard.value ? `${chosenCard.value.kind} · a sketch of its kind; only Earth has generated land` : "waiting for the body model…")
</script>

<template>
    <div class="viewer">
        <nav v-if="chosen" class="overlays" aria-label="Planet overlays">
            <IconTile v-for="item in playable ? OVERLAYS : []" :key="item.id" :label="item.label" side="left" :size="36"
                :on="overlay === item.id" @click="overlay = item.id">
                <PixelIcon :rows="OVERLAY_ICONS[item.id].rows" :palette="OVERLAY_ICONS[item.id].palette" style="--u: 2px" />
            </IconTile>
            <IconTile label="Region seams" side="left" :size="36" :on="seams" @click="seams = !seams">
                <PixelIcon :rows="VIEW_ICONS.seams.rows" :palette="VIEW_ICONS.seams.palette" style="--u: 2px" />
            </IconTile>
            <IconTile v-if="chosenCard" label="Inside cutaway" side="left" :size="36"
                :on="overlay === 'inside'" @click="overlay = 'inside'">
                <PixelIcon :rows="VIEW_ICONS.inside.rows" :palette="VIEW_ICONS.inside.palette" style="--u: 2px" />
            </IconTile>
        </nav>
        <div class="notes">
            <div v-if="chosen && !playable" class="concept-note">{{ kindNote }}</div>
            <div v-if="planet.problem.value" class="concept-note">{{ planet.problem.value }}</div>
            <div v-if="overlay === 'weather' && !image" class="concept-note">{{ looks.weather.problem.value ?? 'the weather is settling…' }}</div>
        </div>

        <div class="foot">
            <div class="date">
                <button class="clock" :aria-label="running ? 'Stop the clock' : 'Run the clock'" :title="running ? 'Stop' : 'Run'"
                    @click="toggleClock">{{ running ? "❚❚" : "▶" }}</button>
                <Slider v-model="shownDate" :label="playable ? 'Earth date' : 'Earth reference date'" :min="0" :max="0.999" :step="0.005"
                    :format="() => `${season(shownDate)} · day ${day} of ${draft.config.year}`" />
            </div>
            <p v-if="playable" class="spawn">
                <span>Spawn</span><strong>{{ draft.spawn ? `${draft.spawn.x}, ${draft.spawn.z}` : 'random land · click the planet' }}</strong>
                <button v-if="draft.spawn" @click="draft.spawn = null">reset</button>
            </p>
        </div>
    </div>
</template>

<style scoped>
/* laid against the edges of the free space: the planet is behind it all */
.viewer { position: relative; height: 100%; min-height: 0; }
.overlays { position: absolute; top: 0; right: 0; display: grid; grid-template-columns: repeat(2, 36px); align-content: start; gap: 6px; max-height: calc(100% - 140px); overflow-y: auto; }
.pending { color: var(--ink-faint); font: 12px var(--font-display); }
.notes { position: absolute; right: 90px; bottom: 110px; display: grid; gap: 6px; justify-items: end; }
.concept-note { max-width: 240px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--ink-dim); font-size: 10px; }
.foot { position: absolute; left: 0; bottom: 0; display: grid; gap: 8px; width: min(420px, 45%); }
.date { display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: end; gap: 10px; }
.clock { width: 34px; height: 34px; border: var(--b) solid var(--edge); background: var(--stone-2); color: var(--ink); font: 12px var(--font-display); cursor: pointer; }
.clock:hover { background: var(--stone-3); }
.spawn { display: flex; gap: 12px; align-items: center; margin: 0; font-size: 12px; color: var(--ink-dim); white-space: nowrap; }
.spawn strong { font: 400 12px var(--font-display); color: var(--ink); }
.spawn button { border: 0; background: none; padding: 0; color: var(--accent-hi); font-size: 12px; }
</style>
