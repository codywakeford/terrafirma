<script setup lang="ts">
import { computed } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { Button, Choice, Field, Section, Segmented } from "../../kit"
import { SIZES, WORLD_PRESETS } from "../../worldgen/config"
import { worldName } from "../../worldgen/names"
import { matching, type WorldDraftHandle } from "./draft"
import { RULE_PRESETS, type Mode } from "./rules"
import SeedField from "./SeedField.vue"

/** Everything needed to start a world, in one place. World and Rules hold the detail. */
const props = defineProps<{ draft: WorldDraftHandle }>()

const DICE = ["KKKKKKK", "K.....K", "K.K...K", "K..K..K", "K...K.K", "K.....K", "KKKKKKK"] as const
const MODES = [
    { id: "survival", label: "Survival", note: "gather, build and stay alive" },
    { id: "creative", label: "Creative", note: "every block, no danger, you can fly" },
] as const satisfies readonly { id: Mode; label: string; note: string }[]
const sizes = SIZES.map((s) => ({ value: s, label: `${s / 1024}k` }))
const nameTaken = computed(() => props.draft.taken.some((n) => n.toLowerCase() === props.draft.name.trim().toLowerCase()))

const style = computed({
    get: () => matching(RULE_PRESETS.map((p) => ({ id: p.id, values: p.rules })), props.draft.rules),
    set: (id) => {
        const preset = RULE_PRESETS.find((p) => p.id === id)
        if (preset) Object.assign(props.draft.rules, preset.rules)
    },
})

const land = computed({
    get: () => matching(WORLD_PRESETS, props.draft.config),
    set: (id) => {
        const preset = WORLD_PRESETS.find((p) => p.id === id)
        if (preset) Object.assign(props.draft.config, preset.values)
    },
})
</script>

<template>
    <Section title="World">
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

    <Section title="Mode">
        <Choice v-model="draft.mode" :options="MODES" />
    </Section>

    <Section title="Play style" :hint="draft.mode === 'creative' ? 'survival only' : style ? '' : 'custom — see Rules'">
        <Choice v-model="style" :options="RULE_PRESETS" :disabled="draft.mode === 'creative'" />
    </Section>

    <Section title="Land" :hint="land ? '' : 'custom — see World · Earth · Surface'">
        <Choice v-model="land" :options="WORLD_PRESETS" />
    </Section>

    <Section title="Size" :hint="`${draft.config.size} × ${draft.config.size} · wraps both ways`">
        <Segmented v-model="draft.config.size" :options="sizes" />
    </Section>
</template>

<style scoped>
.taken { color: var(--danger); font-size: 11px; }
</style>
