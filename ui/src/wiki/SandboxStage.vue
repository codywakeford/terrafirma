<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from "vue"
import type { SandboxSpec } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button, Segmented } from "../kit"
import { useTable } from "./nav"
import { SandboxRun, SPEEDS } from "./sandbox"
import SandboxView from "./SandboxView.vue"

/** A demo running live: the 3D view, and play, pause, speed, restart and the heat overlay. */
const props = defineProps<{ spec: SandboxSpec; speed?: number }>()
const table = useTable()
const run = SandboxRun({ bridge: useBridge() })
const heat = ref(false)
const speeds = SPEEDS.map((s) => ({ value: s, label: `×${s}` }))

watch(() => props.spec, (spec) => {
    run.speed.value = props.speed ?? SPEEDS[0]
    run.running.value = true
    run.start(spec)
}, { immediate: true })
onBeforeUnmount(() => { run.dispose() })

const clock = (s: number) => s < 120 ? `${s.toFixed(1)} s` : s < 7200 ? `${(s / 60).toFixed(1)} min` : `${(s / 3600).toFixed(1)} h`
</script>

<template>
    <div class="stage">
        <SandboxView class="view" :frame="run.frame.value" :substances="table.substances" :elements="table.bySymbol" :heat="heat" />
        <div class="controls">
            <Button size="sm" @click="run.running.value = !run.running.value">{{ run.running.value ? "Pause" : "Play" }}</Button>
            <Button size="sm" @click="run.restart()">Restart</Button>
            <Segmented v-model="run.speed.value" :options="speeds" />
            <Button size="sm" :class="{ on: heat }" @click="heat = !heat">Heat</Button>
            <span class="time">{{ run.frame.value ? clock(run.frame.value.time) : "making…" }}</span>
        </div>
        <p v-if="run.problem.value" class="problem">{{ run.problem.value }}</p>
    </div>
</template>

<style scoped>
.stage { flex: 1; min-height: 0; display: grid; grid-template-rows: 1fr auto auto; gap: 10px; }
.view { min-height: 260px; border: var(--b) solid var(--edge); background: radial-gradient(circle at 50% 40%, #1d2228, #121418); }
.controls { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.time { margin-left: auto; font: 12px var(--font-display); color: var(--ink-dim); }
.on { box-shadow: var(--sunk); color: var(--accent-hi); }
.problem { margin: 0; color: #e07060; font-size: 12px; }
</style>
