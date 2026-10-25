<script setup lang="ts">
import { computed } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { Button, Field, IconChoice, Section, Segmented } from "../../kit/index.ts"
import { DEFAULTS, WORLD_PRESETS, type WorldConfig } from "../../worldgen/config.ts"
import { worldName } from "../../worldgen/names.ts"
import { matching, type WorldDraftHandle } from "./draft.ts"
import { RULE_PRESETS, type Mode } from "./rules.ts"
import SeedField from "./SeedField.vue"

const props = defineProps<{ draft: WorldDraftHandle }>()
const emit = defineEmits<{ fineTuneLand: []; fineTuneRules: [] }>()

const DICE = ["KKKKKKK", "K.....K", "K.K...K", "K..K..K", "K...K.K", "K.....K", "KKKKKKK"] as const
const MODES = [
    { id: "survival", label: "Survival", rows: ["...SS...", "..SSSS..", ".SSSSSS.", "...SS...", "...SS...", "...SS...", "..SSSS.."], palette: { S: "#c9d4dc" } },
    { id: "creative", label: "Creative", rows: ["...C...", ".C.C.C.", "..CCC..", "CCCCCCC", "..CCC..", ".C.C.C.", "...C..."], palette: { C: "#d9ae69" } },
] as const satisfies readonly { id: Mode; label: string; rows: readonly string[]; palette: Record<string, string> }[]
const LAND_ICONS = {
    continents: ["BBBBBBBBB", "BBLLBBBBB", "BLLLLBBLB", "BLLLLBLLB", "BBLLBBBLL", "BBBLBBBBB", "BBBBBBBBB"],
    archipelago: ["BBBBBBBBB", "BLBBBBLBB", "BLLBBBBBB", "BBBBLLBBB", "BBLBBBBLB", "BBBBBLBBB", "BBBBBBBBB"],
    pangaea: ["BBBBBBBBB", "BBBLLLBBB", "BBLLLLLBB", "BLLLLLLBB", "BBLLLLLBB", "BBBLLLBBB", "BBBBBBBBB"],
    highlands: ["BBBBBBBBB", "BBBMMBBBB", "BBMLMMBBB", "BMLMMLMBB", "BBLLMLBBB", "BBBLLBBBB", "BBBBBBBBB"],
    waterworld: ["BBBBBBBBB", "BBBBBBBBB", "BBBLBBBBB", "BBLLBBBBB", "BBBBBBLBB", "BBBBBBBBB", "BBBBBBBBB"],
} as const
const LAND_OPTIONS = WORLD_PRESETS.map((preset) => ({
    id: preset.id,
    label: preset.id === "archipelago" ? "Islands" : preset.id === "waterworld" ? "Ocean" : preset.label,
    rows: LAND_ICONS[preset.id],
    palette: { B: "#17416a", L: "#6caa73", M: "#bec8bd" },
}))
const LAND_KEYS = ["continents", "land", "coast", "mountains", "roughness", "temperature", "humidity", "rivers", "caves", "ores"] as const
const nameTaken = computed(() => props.draft.taken.some((n) => n.toLowerCase() === props.draft.name.trim().toLowerCase()))
const style = computed({
    get: () => matching(RULE_PRESETS.map((p) => ({ id: p.id, values: p.rules })), props.draft.rules),
    set: (id) => {
        const preset = RULE_PRESETS.find((p) => p.id === id)
        if (preset) Object.assign(props.draft.rules, preset.rules)
    },
})
const styleNote = computed(() => RULE_PRESETS.find((p) => p.id === style.value)?.note ?? "Your own mix of survival rules")
const land = computed({
    get: () => WORLD_PRESETS.find((preset) => LAND_KEYS.every((key) =>
        Math.abs(props.draft.config[key] - ((preset.values as Partial<WorldConfig>)[key] ?? DEFAULTS[key])) < 0.005))?.id ?? null,
    set: (id) => {
        const preset = WORLD_PRESETS.find((p) => p.id === id)
        if (!preset) return
        const reset = Object.fromEntries(LAND_KEYS.map((key) => [key, DEFAULTS[key]])) as Pick<WorldConfig, typeof LAND_KEYS[number]>
        Object.assign(props.draft.config, reset, preset.values)
        props.draft.spawn = null
    },
})
const landNote = computed(() => WORLD_PRESETS.find((p) => p.id === land.value)?.note ?? "A custom land shape")
</script>

<template>
    <div class="start-page">
        <div class="start-intro">
            <span>START / QUICK START</span>
            <h2>Make it yours</h2>
        </div>

        <Section title="Your world">
            <Field v-model="draft.name" label="Name" :maxlength="64">
                <template #action>
                    <Button size="sm" title="Another name" @click="draft.name = worldName(draft.taken)">
                        <PixelIcon :rows="DICE" :palette="{ K: 'currentColor' }" style="--u: 2px" />
                    </Button>
                </template>
                <template #hint><span v-if="nameTaken" class="taken">already a world</span></template>
            </Field>
            <SeedField :draft="draft" />
        </Section>

        <Section title="How you play">
            <IconChoice v-model="draft.mode" label="Game mode" :options="MODES" />
            <p class="choice-note">{{ draft.mode === 'survival' ? 'Gather, build and stay alive.' : 'Build freely with every block and no danger.' }}</p>
            <div v-if="draft.mode === 'survival'" class="style-block">
                <span class="sub-label">Survival style</span>
                <Segmented v-model="style" :options="RULE_PRESETS.map((preset) => ({ value: preset.id, label: preset.label }))" />
                <p class="choice-note">{{ styleNote }}</p>
            </div>
            <button class="detail-link" type="button" @click="emit('fineTuneRules')">Fine tune rules »</button>
        </Section>

        <Section title="Planet shape">
            <IconChoice v-model="land" label="Land preset" :options="LAND_OPTIONS" :columns="5" layout="stacked" />
            <p class="choice-note">{{ landNote }}</p>
            <button class="detail-link" type="button" @click="emit('fineTuneLand')">Fine tune land & water »</button>
        </Section>
    </div>
</template>

<style scoped>
.start-page { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
.start-page :deep(.section .body) { gap: 10px; }
.start-intro { display: grid; gap: 5px; padding-bottom: 10px; border-bottom: 1px solid var(--stone-4); }
.start-intro span { color: var(--accent-hi); font: 9px var(--font-display); }
.start-intro h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.taken { color: var(--danger); font-size: 11px; }
.choice-note { margin: -5px 0 0; min-height: 16px; color: var(--ink-dim); font-size: 11px; }
.style-block { display: grid; gap: 10px; }
.sub-label { color: var(--ink-dim); font-size: 11px; }
.detail-link { align-self: flex-start; padding: 0; border: 0; background: none; color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
.detail-link:hover, .detail-link:focus-visible { color: var(--ink); text-decoration: underline; }
</style>
