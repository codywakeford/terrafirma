<script setup lang="ts">
import { computed, ref, toRef, watch } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { IconTile, Slider } from "../../kit/index.ts"
import { body as bodyOf, STAR, type Body } from "../../system/model.ts"
import { useSystemPreview } from "../../system/preview.ts"
import { star } from "../../worldgen/config.ts"
import GlobeView from "../../worldgen/GlobeView.vue"
import MapView from "../../worldgen/MapView.vue"
import { OVERLAYS, paint, type OverlayId } from "../../worldgen/fields.ts"
import { OVERLAY_ICONS } from "../../worldgen/overlayIcons.ts"
import { paintWeather } from "../../worldgen/weather.ts"
import StarView from "../../worldgen/StarView.vue"
import { declination, season } from "../../worldgen/sun.ts"
import type { WorldDraftHandle } from "./draft.ts"
import { useLooks } from "./looks.ts"
import { useBridge } from "../../bridge/use.ts"
import { encodeWorldCode } from "../../worldgen/config.ts"
import { usePlanetarium } from "../../planetarium/use.ts"

// frame: what the page covers of the window's sides (Dock's covers): the planet is framed in the rest
const props = defineProps<{ draft: WorldDraftHandle; frame: { left: number; right: number } }>()
const view = defineModel<"globe" | "map">("view", { required: true })
const cards = useSystemPreview()
const looks = useLooks()
const previewer = looks.terrain
const date = looks.date
const selected = computed<string>({ get: () => props.draft.system.selected, set: (id) => { props.draft.system.selected = id } })
const chosen = computed<Body | null>(() => selected.value === STAR ? null : bodyOf(props.draft.system, selected.value))
const playable = computed(() => chosen.value?.playable ?? false)
const chosenCard = computed(() => chosen.value ? cards.card(chosen.value.id) : null)
const overlay = ref<OverlayId | "inside">("terrain")
const conceptSpawn = ref<{ x: number; z: number } | null>(null)
const surfaceOverlay = computed<OverlayId>(() => overlay.value === "inside" ? "terrain" : overlay.value)
const cutaway = computed(() => view.value === "globe" && overlay.value === "inside" && !!chosenCard.value)
const VIEW_ICONS = {
    globe: { rows: ["...AAA...", ".AABBBA..", "AABBBBAA.", "ABBBGGGBA", "ABBBGGGBA", "AABBBBBA.", ".AABBAA..", "...AAA...", "........."], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    map: { rows: ["AAAAAAAAA", "ABBBBGGBA", "ABBBGGGBA", "ABBGGGGBA", "ABBGGGBBA", "ABBBGBBBA", "ABBBBBBBA", "ABBBBBBBA", "AAAAAAAAA"], palette: { A: "#d5e0e8", B: "#2680bc", G: "#62b46b" } },
    inside: { rows: ["..AAAAA..", ".ABBBBBA.", "ABBCBBBBA", "ABCCDBBBA", "ABCDDBBBA", "ABCCDBBBA", ".ABBBBBA.", "..AAAAA..", "........."], palette: { A: "#d5e0e8", B: "#8b6955", C: "#c15b32", D: "#f2a03c" } },
} as const

watch(overlay, (value) => { if (value === "weather") looks.wantWeather() })
watch(view, (value) => { if (value === "map" && overlay.value === "inside") overlay.value = "terrain" })
const image = computed(() => {
    const fields = previewer.fields.value
    if (!fields) return null
    if (surfaceOverlay.value !== "weather") return paint(fields, surfaceOverlay.value)
    const weather = looks.weather.at(date.value)
    return weather ? paintWeather(fields, weather) : null
})
const selectedSurface = computed(() => chosen.value && !chosen.value.playable ? looks.surfaces.value.get(chosen.value.id) ?? null : null)
const sun = computed(() => ({
    declination: declination(playable.value ? props.draft.config.tilt : chosen.value?.tilt ?? 0, date.value),
    color: star(props.draft.config.star).color,
}))
const day = computed(() => Math.floor(date.value * props.draft.config.year) + 1)
// The globe is the native Planetarium under the page (src/shell/planetarium.h):
// the world's whole system, the camera round the body selected; the playable
// world block for block. The playable world's overlays and the cutaway are
// still the page's globe (paintings of the world model, native layers later).
const square = ref<HTMLElement | null>(null)
const native = computed(() => view.value === "globe" && selected.value !== STAR && overlay.value !== "inside" &&
    (!playable.value || surfaceOverlay.value === "terrain"))
const planet = usePlanetarium({
    bridge: useBridge(),
    element: square,
    active: native,
    whole: true, // the universe behind the whole screen, as behind the menu
    frame: toRef(props, "frame"),
    body: computed(() => ({ code: encodeWorldCode(props.draft.config) })),
})
// the camera flies to the body selected: in from the menu's view when the
// screen opens, across the system when another is picked
watch([selected, native], ([id, on], was) => {
    if (on && id !== STAR) planet.fly({ body: id, radii: 2.5, seconds: was && was[1] ? 2 : 2.5 })
}, { immediate: true })
const kindNote = computed(() => chosenCard.value ? `${chosenCard.value.kind} · a sketch of its kind; only Earth has generated land` : "waiting for the body model…")
</script>

<template>
    <div class="viewer">
        <div class="stage" :class="{ 'with-overlays': chosen }">
            <nav class="view-tools" aria-label="Planet view">
                <IconTile label="Globe view" :size="36" :on="view === 'globe'" @click="view = 'globe'">
                    <PixelIcon :rows="VIEW_ICONS.globe.rows" :palette="VIEW_ICONS.globe.palette" style="--u: 2px" />
                </IconTile>
                <IconTile label="Map view" :size="36" :on="view === 'map'" @click="view = 'map'">
                    <PixelIcon :rows="VIEW_ICONS.map.rows" :palette="VIEW_ICONS.map.palette" style="--u: 2px" />
                </IconTile>
            </nav>
            <div ref="square" class="square">
                <StarView v-if="selected === STAR" :star="draft.config.star" />
                <template v-else-if="playable && image && previewer.fields.value">
                    <MapView v-if="view === 'map'" v-model:spawn="draft.spawn" :image="image" :size="draft.config.size" />
                    <div v-else-if="native" class="native" />
                    <GlobeView v-else :image="image" :elevation="previewer.fields.value.elevation" :relief="0.08" :sun="sun"
                        :layers="cutaway ? chosenCard?.layers : undefined" />
                </template>
                <template v-else-if="!playable && selectedSurface && chosen">
                    <MapView v-if="view === 'map'" v-model:spawn="conceptSpawn" :image="selectedSurface.image" :size="draft.config.size"
                        :allow-spawn="false" :show-coords="false" />
                    <div v-else-if="native" class="native" />
                    <GlobeView v-else :image="selectedSurface.image" :elevation="selectedSurface.elevation" :relief="0.08" :sun="sun"
                        :rotation-period="chosen.day" :layers="cutaway ? chosenCard?.layers : undefined" />
                </template>
                <div v-else-if="native" class="native" />
                <div v-else class="pending">{{ previewer.problem.value ?? looks.weather.problem.value ?? (surfaceOverlay === 'weather' && previewer.fields.value ? 'the weather is settling…' : 'generating…') }}</div>
            </div>

            <nav v-if="chosen" class="overlays" aria-label="Planet overlays">
                <IconTile v-for="item in playable ? OVERLAYS : []" :key="item.id" :label="item.label" side="left" :size="36"
                    :on="overlay === item.id" @click="overlay = item.id">
                    <PixelIcon :rows="OVERLAY_ICONS[item.id].rows" :palette="OVERLAY_ICONS[item.id].palette" style="--u: 2px" />
                </IconTile>
                <IconTile v-if="view === 'globe' && chosenCard" label="Inside cutaway" side="left" :size="36"
                    :on="overlay === 'inside'" @click="overlay = 'inside'">
                    <PixelIcon :rows="VIEW_ICONS.inside.rows" :palette="VIEW_ICONS.inside.palette" style="--u: 2px" />
                </IconTile>
            </nav>
            <div v-if="chosen && !playable" class="concept-note">{{ kindNote }}</div>
            <div v-if="native && planet.problem.value" class="concept-note">{{ planet.problem.value }}</div>
        </div>

        <div class="foot">
            <div class="date">
                <Slider v-model="date" :label="playable ? 'Earth date' : 'Earth reference date'" :min="0" :max="0.999" :step="0.005"
                    :format="() => `${season(date)} · day ${day} of ${draft.config.year}`" />
            </div>
            <p v-if="playable && view === 'map'" class="spawn">
                <span>Spawn</span><strong>{{ draft.spawn ? `${draft.spawn.x}, ${draft.spawn.z}` : 'random land' }}</strong>
                <button v-if="draft.spawn" @click="draft.spawn = null">reset</button>
            </p>
        </div>
    </div>
</template>

<style scoped>
.viewer { display: flex; flex-direction: column; gap: 8px; min-height: 0; height: 100%; }
.stage { position: relative; flex: 1; min-height: 0; display: grid; place-items: center; container-type: size; }
.stage.with-overlays { padding-right: 90px; }
.square { width: min(100cqw, 100cqh); aspect-ratio: 1; }
.with-overlays .square { width: min(calc(100cqw - 90px), 100cqh); }
.square > :deep(*) { width: 100%; height: 100%; }
.pending { display: grid; place-items: center; color: var(--ink-faint); font: 12px var(--font-display); }
.view-tools { position: absolute; z-index: 1; top: 0; left: 0; display: flex; flex-direction: column; gap: 6px; }
.overlays { position: absolute; top: 0; right: 0; display: grid; grid-template-columns: repeat(2, 36px); align-content: start; gap: 6px; max-height: 100%; overflow-y: auto; }
.concept-note { position: absolute; right: 88px; bottom: 0; max-width: 220px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--ink-dim); font-size: 10px; }
.foot { display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: end; gap: 24px; }
.date { max-width: 420px; }
.spawn { display: flex; gap: 12px; align-items: center; margin: 0; font-size: 12px; color: var(--ink-dim); white-space: nowrap; }
.spawn strong { font: 400 12px var(--font-display); color: var(--ink); }
.spawn button { border: 0; background: none; padding: 0; color: var(--accent-hi); font-size: 12px; }
</style>
