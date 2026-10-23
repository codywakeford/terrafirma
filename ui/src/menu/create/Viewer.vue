<script setup lang="ts">
import { computed, ref, watch } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { IconTile, Segmented, Slider } from "../../kit"
import { body as bodyOf, STAR, type Body } from "../../system/model"
import { useSystemPreview } from "../../system/preview"
import { star } from "../../worldgen/config"
import GlobeView from "../../worldgen/GlobeView.vue"
import MapView from "../../worldgen/MapView.vue"
import { OVERLAYS, paint, type OverlayId } from "../../worldgen/fields"
import { OVERLAY_ICONS } from "../../worldgen/overlayIcons"
import { paintWeather } from "../../worldgen/weather"
import StarView from "../../worldgen/StarView.vue"
import Interior from "../../system/Interior.vue"
import SystemView from "../../worldgen/SystemView.vue"
import { declination, season } from "../../worldgen/sun"
import type { WorldDraftHandle } from "./draft"
import { useLooks } from "./looks"

/**
 * The preview pane. The system selects a body; Globe and Map show it. The
 * playable world shows its generated terrain; other bodies show a sketch of
 * the kind of world they became (from the body model's cards).
 */
const props = defineProps<{ draft: WorldDraftHandle }>()
const cards = useSystemPreview()
const looks = useLooks()
const previewer = looks.terrain
const date = looks.date

const selected = computed<string>({ get: () => props.draft.system.selected, set: (id) => { props.draft.system.selected = id } })
const chosen = computed<Body | null>(() => (selected.value === STAR ? null : bodyOf(props.draft.system, selected.value)))
const playable = computed(() => chosen.value?.playable ?? false)
const chosenCard = computed(() => (chosen.value ? cards.card(chosen.value.id) : null))
const view = ref<"globe" | "map" | "system" | "interior">("globe")
const overlay = ref<OverlayId>("terrain")
const relief = ref(0.08)
const conceptSpawn = ref<{ x: number; z: number } | null>(null)

const views = [
    { value: "globe", label: "Globe" },
    { value: "map", label: "Map" },
    { value: "system", label: "System" },
    { value: "interior", label: "Inside" },
] as const
const reliefs = [
    { value: 0, label: "flat" },
    { value: 0.04, label: "1×" },
    { value: 0.08, label: "2×" },
    { value: 0.16, label: "4×" },
] as const

watch(overlay, (o) => { if (o === "weather") looks.wantWeather() })
const image = computed(() => {
    const fields = previewer.fields.value
    if (!fields) return null
    if (overlay.value !== "weather") return paint(fields, overlay.value)
    const weather = looks.weather.at(date.value)
    return weather ? paintWeather(fields, weather) : null
})
const systemSurfaces = computed(() => props.draft.system.bodies.flatMap((b) => {
    const image = looks.image(b.id)
    return image ? [{ id: b.id, image: image }] : []
}))
const selectedSurface = computed(() => (chosen.value && !chosen.value.playable ? looks.surfaces.value.get(chosen.value.id) ?? null : null))
const sun = computed(() => ({
    declination: declination(playable.value ? props.draft.config.tilt : chosen.value?.tilt ?? 0, date.value),
    color: star(props.draft.config.star).color,
}))
const day = computed(() => Math.floor(date.value * props.draft.config.year) + 1)
const kindNote = computed(() => {
    const card = chosen.value ? cards.card(chosen.value.id) : null
    return card ? `${card.kind} · a sketch of its kind; only Earth has generated land` : "waiting for the body model…"
})
</script>

<template>
    <div class="viewer">
        <div class="bar">
            <Segmented v-model="view" :options="views" class="views" />
            <Segmented v-if="view === 'globe' && playable" v-model="relief" :options="reliefs" class="relief" />
            <span class="hint">{{ view === 'system' ? 'drag to orbit' : view === 'map' ? playable ? 'drag · scroll · click to set spawn' : 'drag · scroll' : 'drag to spin' }}</span>
        </div>

        <div class="stage">
            <div class="square">
                <SystemView v-if="view === 'system'" v-model="selected" :date="date" :year="draft.config.year"
                    :star="draft.config.star" :system="draft.system" :surfaces="systemSurfaces" />
                <StarView v-else-if="selected === STAR" :star="draft.config.star" />
                <template v-else-if="view === 'interior' && chosen">
                    <Interior v-if="chosenCard" :card="chosenCard" :name="chosen.name" />
                    <div v-else class="pending">working it out…</div>
                </template>
                <template v-else-if="playable && image && previewer.fields.value">
                    <MapView v-if="view === 'map'" v-model:spawn="draft.spawn" :image="image" :size="draft.config.size" />
                    <GlobeView v-else :image="image" :elevation="previewer.fields.value.elevation" :relief="relief" :sun="sun" />
                </template>
                <template v-else-if="!playable && selectedSurface && chosen">
                    <MapView v-if="view === 'map'" v-model:spawn="conceptSpawn" :image="selectedSurface.image" :size="draft.config.size"
                        :allow-spawn="false" :show-coords="false" />
                    <GlobeView v-else :image="selectedSurface.image" :elevation="selectedSurface.elevation" :relief="relief" :sun="sun"
                        :rotation-period="chosen.day" />
                </template>
                <div v-else class="pending">{{ previewer.problem.value ?? looks.weather.problem.value ?? (overlay === 'weather' && previewer.fields.value ? 'the weather is settling…' : 'generating…') }}</div>
            </div>


            <nav v-if="playable && view !== 'system'" class="strip overlays" aria-label="Overlay">
                <IconTile v-for="o in OVERLAYS" :key="o.id" :label="o.label" side="left" :size="36"
                    :on="overlay === o.id" @click="overlay = o.id">
                    <PixelIcon :rows="OVERLAY_ICONS[o.id].rows" :palette="OVERLAY_ICONS[o.id].palette" style="--u: 2px" />
                </IconTile>
            </nav>
            <div v-if="chosen && !playable && view !== 'system'" class="concept-note">{{ kindNote }}</div>
        </div>

        <div class="foot">
            <div class="date">
                <Slider v-model="date" label="Earth date" :min="0" :max="0.999" :step="0.005"
                    :format="() => `${season(date)} · day ${day} of ${draft.config.year}`" />
            </div>
            <p v-if="playable && view === 'map'" class="spawn">
                <span>Spawn</span>
                <strong>{{ draft.spawn ? `${draft.spawn.x}, ${draft.spawn.z}` : 'random land' }}</strong>
                <button v-if="draft.spawn" @click="draft.spawn = null">reset</button>
            </p>
        </div>
    </div>
</template>

<style scoped>
.viewer { display: flex; flex-direction: column; gap: 10px; min-height: 0; height: 100%; }
.bar { display: flex; align-items: center; gap: 12px; min-height: 32px; }
.views { width: 340px; flex: none; }
.relief { width: 220px; }
.hint { margin-left: auto; font-size: 11px; color: var(--ink-faint); white-space: nowrap; }

.stage { position: relative; flex: 1; min-height: 0; display: grid; place-items: center; container-type: size; }
.square { width: min(100cqw, 100cqh); height: min(100cqw, 100cqh); }
.square > :deep(*) { width: 100%; height: 100%; }
.pending { display: grid; place-items: center; color: var(--ink-faint); font: 12px var(--font-display); }

/* Tile strips over the stage's lower corners. */
.strip { position: absolute; bottom: 0; display: flex; flex-direction: column; gap: 6px; }
.overlays { right: 0; }
.concept-note { position: absolute; right: 0; bottom: 0; max-width: 250px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--ink-dim); font-size: 10px; }

.foot { display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: end; gap: 24px; }
.date { max-width: 420px; }
.spawn { display: flex; gap: 12px; align-items: center; margin: 0; font-size: 12px; color: var(--ink-dim); white-space: nowrap; }
.spawn strong { font: 400 12px var(--font-display); color: var(--ink); }
.spawn button { border: 0; background: none; padding: 0; color: var(--accent-hi); font-size: 12px; }
</style>
