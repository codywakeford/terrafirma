<script setup lang="ts">
import { ref, watch } from "vue"
import { useBridge } from "../bridge/use.ts"
import { decodeWorldCode, star } from "../worldgen/config.ts"
import { decodePreview, paint, type Fields } from "../worldgen/fields.ts"
import GlobeView from "../worldgen/GlobeView.vue"
import { declination } from "../worldgen/sun.ts"

const props = defineProps<{ code: string }>()
const bridge = useBridge()
const image = ref<ImageData | null>(null)
const fields = ref<Fields | null>(null)
const problem = ref<string | null>(null)
const sun = ref<{ declination: number; color: readonly [number, number, number] }>({ declination: 0, color: star("yellow").color })
let generation = 0

watch(() => props.code, async (code) => {
    const current = ++generation
    image.value = null
    fields.value = null
    problem.value = null
    try {
        const config = decodeWorldCode(code)
        sun.value = { declination: declination(config.tilt, 0.25), color: star(config.star).color }
        const wire = await bridge.send("worldgen.preview", { code, date: 0.25, resolution: 128 })
        if (current !== generation) return
        fields.value = decodePreview(wire)
        image.value = paint(fields.value, "terrain")
    } catch (err) {
        if (current !== generation) return
        problem.value = err instanceof Error ? err.message : String(err)
    }
}, { immediate: true })
</script>

<template>
    <div class="preview">
        <GlobeView v-if="image && fields" :image="image" :elevation="fields.elevation" :relief="0.08" :sun="sun" :auto-spin="false" />
        <p v-else>{{ problem ?? "Generating the planet…" }}</p>
    </div>
</template>

<style scoped>
.preview { width: min(100%, 260px); aspect-ratio: 1; overflow: hidden; background: radial-gradient(circle, #18344a 0%, #08121b 62%, #060d15 100%); border: var(--b) solid var(--edge); box-shadow: var(--sunk); }
.preview :deep(.globe) { width: 100%; height: 100%; }
.preview p { display: grid; place-items: center; height: 100%; margin: 0; color: var(--ink-dim); font-size: 12px; }
</style>
