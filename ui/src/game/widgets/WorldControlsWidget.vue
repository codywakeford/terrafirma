<script setup lang="ts">
import { computed, ref } from "vue"
import { useBridge } from "../../bridge/use"
import { Button, Notice } from "../../kit"
import { useStore } from "../store"
import { widgetConfig } from "./config"
import WidgetFrame from "./WidgetFrame.vue"

const bridge = useBridge()
const store = useStore()
const controls = computed(() => store.ui.world_controls)
const problem = ref<string | null>(null)
const busy = ref(false)
const times = ["dawn", "noon", "dusk", "midnight"] as const

async function send(name: "world.mode" | "world.time" | "world.clock", payload: { mode: "creative" | "survival" } | { time: "dawn" | "noon" | "dusk" | "midnight" } | { paused: boolean }) {
    if (busy.value) return
    busy.value = true
    problem.value = null
    try {
        if (name === "world.mode" && "mode" in payload) await bridge.send("ui.send", { name, payload })
        else if (name === "world.time" && "time" in payload) await bridge.send("ui.send", { name, payload })
        else if (name === "world.clock" && "paused" in payload) await bridge.send("ui.send", { name, payload })
    } catch (error) { problem.value = error instanceof Error ? error.message : String(error) }
    finally { busy.value = false }
}
</script>

<template>
    <WidgetFrame v-if="widgetConfig.ready && widgetConfig.worldControls && controls" title="World controls"
        :corner="widgetConfig.worldControlsCorner" :below-profile="widgetConfig.worldControlsCorner === 1" interactive>
        <div class="controls">
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <label>Build mode</label>
            <div class="pair">
                <Button size="sm" :variant="controls.mode === 'survival' ? 'primary' : undefined" :disabled="busy" @click="send('world.mode', { mode: 'survival' })">Survival</Button>
                <Button size="sm" :variant="controls.mode === 'creative' ? 'primary' : undefined" :disabled="busy" @click="send('world.mode', { mode: 'creative' })">Creative</Button>
            </div>
            <p class="detail">{{ controls.mode === 'creative' ? 'Creative protects you from damage' : controls.damage ? 'World damage is on' : 'World damage is off' }}</p>
            <label>Time of day</label>
            <div class="times">
                <Button v-for="time in times" :key="time" size="sm" :disabled="busy"
                    @click="send('world.time', { time })">{{ time }}</Button>
            </div>
            <Button size="sm" :disabled="busy" @click="send('world.clock', { paused: !controls.paused })">{{ controls.paused ? 'Resume clock' : 'Pause clock' }}</Button>
        </div>
    </WidgetFrame>
</template>

<style scoped>
.controls { width: 238px; display: grid; gap: 8px; }
label { font: 10px var(--font-display); color: var(--ink-dim); text-transform: uppercase; }
.pair, .times { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 5px; }
.pair :deep(.btn), .times :deep(.btn) { min-width: 0; width: 100%; }
.detail { margin: -2px 0 2px; font-size: 10px; color: var(--ink-dim); }
</style>
