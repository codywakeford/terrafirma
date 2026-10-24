<script setup lang="ts">
import { ref, watch } from "vue"
import { useBridge } from "../bridge/use"
import { decodeWorldCode } from "../worldgen/config"
import { decodePreview, paint } from "../worldgen/fields"
import MapView from "../worldgen/MapView.vue"

const props = defineProps<{ code: string }>()
const bridge = useBridge()
const image = ref<ImageData | null>(null)
const problem = ref<string | null>(null)
const spawn = ref<{ x: number; z: number } | null>(null)
const size = ref(0)
let generation = 0

watch(() => props.code, async (code) => {
    const current = ++generation
    image.value = null
    problem.value = null
    try {
        size.value = decodeWorldCode(code).size
        const wire = await bridge.send("worldgen.preview", { code, date: 0.25, resolution: 128 })
        if (current !== generation) return
        image.value = paint(decodePreview(wire), "terrain")
    } catch (err) {
        if (current !== generation) return
        problem.value = err instanceof Error ? err.message : String(err)
    }
}, { immediate: true })
</script>

<template>
    <div class="preview">
        <MapView v-if="image" v-model:spawn="spawn" :image="image" :size="size" :allow-spawn="false" :show-coords="false" />
        <p v-else>{{ problem ?? "Generating the planet…" }}</p>
    </div>
</template>

<style scoped>
.preview { width: min(100%, 300px); aspect-ratio: 1; background: var(--stone-0); border: var(--b) solid var(--edge); box-shadow: var(--sunk); }
.preview :deep(.map) { width: 100%; height: 100%; border: 0; }
.preview p { display: grid; place-items: center; height: 100%; margin: 0; color: var(--ink-dim); font-size: 12px; }
</style>
