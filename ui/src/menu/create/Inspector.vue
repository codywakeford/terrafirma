<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { Section, Segmented, Slider, Field, Toggle } from "../../kit"
import BodyFacts from "../../system/BodyFacts.vue"
import History from "../../system/History.vue"
import { body as bodyOf, LIMITS, STAR, type Body } from "../../system/model"
import { useSystemPreview } from "../../system/preview"
import type { WorldDraftHandle } from "./draft"
import StarPicker from "./StarPicker.vue"
import WorldTab from "./WorldTab.vue"

/**
 * Everything about the selected body. What it became (from the engine's
 * body model) sits on top; its causes are grouped in small tabs below. The
 * playable world also has Surface: its land, which is the world code.
 */
const props = defineProps<{ draft: WorldDraftHandle }>()
const preview = useSystemPreview()

const chosen = computed<Body | null>(() => (props.draft.system.selected === STAR ? null : bodyOf(props.draft.system, props.draft.system.selected)))
const card = computed(() => (chosen.value ? preview.card(chosen.value.id) : null))
const moon = computed(() => chosen.value?.parent != null)

type Part = "surface" | "orbit" | "makeup" | "spin" | "history"
const PARTS: readonly { value: Part; label: string; playable: boolean }[] = [
    { value: "surface", label: "Surface", playable: true },
    { value: "orbit", label: "Orbit", playable: false },
    { value: "makeup", label: "Make-up", playable: false },
    { value: "spin", label: "Spin", playable: false },
    { value: "history", label: "History", playable: false },
]
const parts = computed(() => PARTS.filter((p) => !p.playable || chosen.value?.playable))
const part = ref<Part>("surface")
// a body without the open part (Surface is the playable world's) opens on Orbit
watch(parts, (now) => { if (!now.some((p) => p.value === part.value)) part.value = "orbit" }, { immediate: true })

const percent = (v: number) => `${Math.round(v * 100)}%`
const mass = (v: number) => {
    const m = 10 ** v
    return `${m < 0.1 ? m.toPrecision(2) : m < 10 ? m.toFixed(2) : Math.round(m)} Earth masses`
}
const formed = (v: number) => (v < 0.25 ? "close to its star: rock and metal" : v < 0.5 ? "inside the frost line"
    : v < 0.7 ? "near the frost line" : "beyond the frost line: icy")
const day = (h: number) => (h < 48 ? `${Math.round(h)} hours` : `${(h / 24).toFixed(1)} days`)
</script>

<template>
    <aside class="inspector">
        <template v-if="!chosen">
            <header class="title"><span class="eyebrow">star</span></header>
            <Section title="Star" hint="its type sets Earth's light and climate">
                <StarPicker v-model="draft.config.star" />
                <Slider v-model="draft.system.age" label="Age" :min="0.5" :max="12" :step="0.1" :format="(v) => `${v.toFixed(1)} billion years`" />
                <Slider v-model="draft.system.activity" label="Stellar wind" :min="0" :max="1" :step="0.01" :format="percent" />
            </Section>
            <p class="copy">Age is how long every world has had to cool, outgas, lose air and grow its story; the wind strips
                the air from worlds without a magnetic field.</p>
        </template>

        <template v-else>
            <header class="title">
                <span class="swatch" :style="{ background: chosen.color }" />
                <Field v-model="chosen.name" label="Name" :maxlength="32" />
            </header>
            <BodyFacts v-if="card" :card="card" />
            <p v-else class="copy">{{ preview.problem.value ?? 'working it out…' }}</p>

            <Segmented v-model="part" :options="parts" class="parts" />

            <div class="part">
                <template v-if="part === 'surface'">
                    <WorldTab :draft="draft" />
                </template>

                <template v-else-if="part === 'orbit'">
                    <Section :title="moon ? 'Around its planet' : 'Around the star'">
                        <Slider v-model="chosen.orbit" :label="moon ? 'Distance' : 'Distance from the star'"
                            :min="moon ? LIMITS.orbit.planet.min : LIMITS.orbit.star.min" :max="moon ? 100 : 40" :step="moon ? 0.5 : 0.01"
                            :format="(v) => moon ? `${v} planet radii` : `${v.toFixed(2)} AU`" />
                        <Slider v-model="chosen.eccentricity" label="Eccentricity" :min="0" :max="0.6" :step="0.005" :format="(v) => v.toFixed(3)" />
                        <Slider v-model="chosen.inclination" label="Inclination" :min="-30" :max="30" :step="1" :format="(v) => `${v}°`" />
                    </Section>
                </template>

                <template v-else-if="part === 'makeup'">
                    <Section title="What it's made of">
                        <Slider v-model="chosen.mass" label="Mass" :min="LIMITS.mass.min" :max="LIMITS.mass.max" :step="0.01" :format="mass" />
                        <Slider v-model="chosen.formed" label="Where it formed" :min="0" :max="1" :step="0.01" :format="formed" />
                        <Slider v-model="chosen.volatiles" label="Water and gases it formed with" :min="0" :max="1" :step="0.01" :format="percent" />
                    </Section>
                </template>

                <template v-else-if="part === 'spin'">
                    <Section title="Spin and life">
                        <Slider v-model="chosen.day" label="Day" :min="4" :max="2000" :step="1" :format="day" />
                        <Slider v-if="!chosen.playable" v-model="chosen.tilt" label="Axial tilt" :min="0" :max="90" :step="1" :format="(v) => `${v}°`" />
                        <p v-else class="copy">Earth's tilt and year are part of its world code: they're on Surface.</p>
                        <Toggle v-model="chosen.life" label="Life arises where it can" />
                    </Section>
                </template>

                <template v-else-if="part === 'history'">
                    <History v-if="card" :card="card" :age="draft.system.age" />
                </template>
            </div>
        </template>
    </aside>
</template>

<style scoped>
.inspector { display: flex; flex-direction: column; gap: 12px; min-height: 0; overflow-y: auto; padding-right: 6px; }
.title { display: flex; align-items: end; gap: 10px; }
.title :deep(.field) { flex: 1; }
.eyebrow { font: 12px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.swatch { flex: none; width: 14px; height: 32px; border: var(--b) solid var(--edge); }
.parts { flex: none; }
.part { display: flex; flex-direction: column; gap: 18px; }
.copy { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
</style>
