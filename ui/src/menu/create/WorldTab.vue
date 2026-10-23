<script setup lang="ts">
import { computed } from "vue"
import { Button, Section, Slider } from "../../kit"
import { encodeWorldCode } from "../../worldgen/config"
import { Copier } from "./copy"
import type { WorldDraftHandle } from "./draft"

/** The playable world's surface (the inspector's Surface part): every land setting, and the code that carries them. */
const props = defineProps<{ draft: WorldDraftHandle }>()

const percent = (v: number) => `${Math.round(v * 100)}%`
const signed = (v: number) => (Math.round(v * 100) === 0 ? "0" : `${v > 0 ? "+" : ""}${Math.round(v * 100)}`)

const code = computed(() => encodeWorldCode(props.draft.config))
const copier = Copier()
</script>

<template>
    <Section title="Landmass">
        <Slider v-model="draft.config.continents" label="Continents" :min="1" :max="8" :step="1" />
        <Slider v-model="draft.config.land" label="Land" :min="0.1" :max="0.9" :step="0.01" :format="percent" />
        <Slider v-model="draft.config.coast" label="Coastline" :min="0" :max="1" :step="0.01"
            :format="(v) => (v < 0.33 ? 'smooth' : v < 0.66 ? 'varied' : 'jagged')" />
    </Section>

    <Section title="Relief">
        <Slider v-model="draft.config.mountains" label="Mountains" :min="0" :max="1" :step="0.01" :format="percent" />
        <Slider v-model="draft.config.roughness" label="Roughness" :min="0" :max="1" :step="0.01" :format="percent" />
    </Section>

    <Section title="Climate">
        <Slider v-model="draft.config.temperature" label="Temperature" :min="-1" :max="1" :step="0.01" :format="signed" />
        <Slider v-model="draft.config.humidity" label="Humidity" :min="-1" :max="1" :step="0.01" :format="signed" />
    </Section>

    <Section title="Earth seasons" hint="the star is in the tree · scrub the date under the viewer">
        <Slider v-model="draft.config.tilt" label="Axial tilt" :min="0" :max="45" :step="1"
            :format="(v) => `${v}° · ${v < 8 ? 'no real seasons' : v < 18 ? 'mild seasons' : v < 30 ? 'earthlike' : 'harsh seasons'}`" />
        <Slider v-model="draft.config.year" label="Year length" :min="8" :max="128" :step="4"
            :format="(v) => `${v} days · ${Math.round(v / 4)} per season`" />
    </Section>

    <Section title="Water" hint="rain runs off, carves valleys, pools in lakes">
        <Slider v-model="draft.config.rivers" label="Rivers" :min="0" :max="1" :step="0.01"
            :format="(v) => (v < 0.33 ? 'few, large' : v < 0.66 ? 'some' : 'many, branching')" />
    </Section>

    <Section title="Underground" hint="not in the preview yet">
        <Slider v-model="draft.config.caves" label="Caves" :min="0" :max="1" :step="0.01" :format="percent" />
        <Slider v-model="draft.config.ores" label="Ore richness" :min="0" :max="1" :step="0.01" :format="percent" />
    </Section>

    <Section title="This world's code" hint="seed + every setting above · paste it into Seed">
        <div class="code-row">
            <code class="code">{{ code }}</code>
            <Button size="sm" @click="copier.copy(code)">{{ copier.state.value === 'copied' ? 'Copied' : 'Copy' }}</Button>
        </div>
    </Section>
</template>

<style scoped>
.code {
    padding: 8px 10px;
    border: var(--b) dashed var(--accent-lo);
    background: rgb(0 148 210 / 0.08);
    font-size: 12px;
    color: var(--accent-hi);
    word-break: break-all;
    user-select: text;
}
.code-row { display: flex; gap: 8px; align-items: stretch; }
.code-row .code { flex: 1; min-width: 0; }
</style>
