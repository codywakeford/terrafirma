<script setup lang="ts">
import { onMounted, ref } from "vue"
import { BridgeError } from "../bridge"
import { useBridge } from "../bridge/use"
import { Slider, Tabs, Toggle } from "../kit"
import { GROUPS } from "./options"

/**
 * Every setting, grouped. No frame of its own: the main menu and the in-game
 * pause screen each put it in theirs. Values come from the engine; a change
 * is saved when a slider is released or a toggle flipped.
 */
const bridge = useBridge()

const values = ref<Record<string, number | boolean> | null>(null)
const group = ref(GROUPS[0]!.name)
const problem = ref<string | null>(null)
const tabs = GROUPS.map((g) => ({ id: g.name, label: g.name }))

async function save(key: string, value: number | boolean) {
    problem.value = null
    try {
        await bridge.send("settings.set", { key: key, value: value })
    } catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
        values.value = await bridge.send("settings.get", {}) // show what is really set
    }
}

function num(key: string) {
    const v = values.value?.[key]
    if (typeof v !== "number") throw new Error(`setting ${key} is not a number`)
    return v
}

function flag(key: string) {
    const v = values.value?.[key]
    if (typeof v !== "boolean") throw new Error(`setting ${key} is not a flag`)
    return v
}

function set(key: string, value: number | boolean) {
    values.value = { ...values.value, [key]: value }
}

onMounted(async () => (values.value = await bridge.send("settings.get", {})))
</script>

<template>
    <div class="settings">
        <Tabs v-model="group" :tabs="tabs" direction="column" />
        <div v-if="values" class="options">
            <template v-for="g in GROUPS" :key="g.name">
                <template v-if="g.name === group">
                    <template v-for="o in g.options" :key="o.key">
                        <Slider v-if="o.kind === 'slider'" :model-value="num(o.key)" :label="o.label" :min="o.min"
                            :max="o.max" :step="o.step" :format="o.format"
                            @update:model-value="set(o.key, $event)" @change="save(o.key, $event)" />
                        <Toggle v-else :model-value="flag(o.key)" :label="o.label"
                            @update:model-value="set(o.key, $event); save(o.key, $event)" />
                    </template>
                </template>
            </template>
            <p v-if="problem" class="problem">{{ problem }}</p>
        </div>
    </div>
</template>

<style scoped>
.settings { display: grid; grid-template-columns: 170px minmax(0, 1fr); gap: 22px; }
.options { display: flex; flex-direction: column; gap: 22px; max-width: 420px; min-width: 0; }
.problem { margin: 0; color: var(--danger); }
</style>
