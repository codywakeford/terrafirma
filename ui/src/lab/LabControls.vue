<script setup lang="ts">
import { reactive, watch } from "vue"
import type { LabControl, LabValue } from "../bridge"
import { Button, Segmented, Slider, Toggle } from "../kit"

/**
 * A scenario's controls, from its lab.json. Shows `values` (what the
 * scenario has); a change is emitted once settled (sliders on release) and
 * shows as soon as the scenario sends it back. Disabled until the
 * scenario's world is running.
 */
const props = defineProps<{ controls: LabControl[]; values: Record<string, LabValue>; disabled: boolean }>()
const emit = defineEmits<{ set: [id: string, value: LabValue | null] }>()

// what the inputs show: the scenario's values, plus a slider being dragged
const shown = reactive<Record<string, LabValue>>({})
watch(() => props.values, (values) => Object.assign(shown, values), { immediate: true, deep: true })

function num(id: string, fallback: number) {
    const v = shown[id]
    return typeof v === "number" ? v : fallback
}
function bool(id: string, fallback: boolean) {
    const v = shown[id]
    return typeof v === "boolean" ? v : fallback
}
function pick(id: string, fallback: string | number) {
    const v = shown[id]
    return typeof v === "boolean" || v === undefined ? fallback : v
}
</script>

<template>
    <div class="controls" :class="{ disabled }">
        <template v-for="c in controls" :key="c.id">
            <Slider v-if="c.kind === 'slider'" :label="c.label" :min="c.min" :max="c.max" :step="c.step"
                :format="(v) => c.unit ? `${v} ${c.unit}` : String(v)"
                :model-value="num(c.id, c.default)" @update:model-value="shown[c.id] = $event"
                @change="emit('set', c.id, $event)" />
            <Toggle v-else-if="c.kind === 'toggle'" :label="c.label" :model-value="bool(c.id, c.default)"
                @update:model-value="emit('set', c.id, $event)" />
            <div v-else-if="c.kind === 'choice'" class="choice">
                <span class="label">{{ c.label }}</span>
                <Segmented :options="c.options" :model-value="pick(c.id, c.default)"
                    @update:model-value="emit('set', c.id, $event)" />
            </div>
        </template>
        <div v-if="controls.some((c) => c.kind === 'button')" class="buttons">
            <template v-for="c in controls" :key="c.id">
                <Button v-if="c.kind === 'button'" @click="emit('set', c.id, null)">{{ c.label }}</Button>
            </template>
        </div>
    </div>
</template>

<style scoped>
.controls { display: grid; gap: 14px; }
.controls.disabled { opacity: 0.5; pointer-events: none; }
.choice { display: grid; gap: 6px; }
.label { font-size: 13px; color: var(--ink-dim); }
.buttons { display: flex; flex-wrap: wrap; gap: 10px; }
</style>
