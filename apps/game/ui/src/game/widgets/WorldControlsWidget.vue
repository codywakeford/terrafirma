<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import { useBridge } from "../../bridge/use"
import { Button, ListGroup, ListItem, Notice, Panel, Section, Segmented, Slider, Toggle, WorkbenchColumns } from "../../kit"
import { useStore } from "../store"
import { widgetConfig } from "./config"

const props = defineProps<{ initialOpen?: boolean; initialPage?: "mode" | "time" }>()
const bridge = useBridge()
const store = useStore()
const controls = computed(() => store.ui.world_controls)
const root = ref<HTMLElement | null>(null)
const trigger = ref<HTMLButtonElement | null>(null)
const expanded = ref(!!props.initialOpen)
const page = ref<"mode" | "time">(props.initialPage ?? "mode")
const problem = ref<string | null>(null)
const busy = ref(false)
const stagedTime = ref(0.5)
const editingTime = ref(false)
const modes = [{ value: "survival", label: "Survival" }, { value: "creative", label: "Creative" }] as const
const times = [
    { id: "dawn", label: "Dawn", note: "05:30" }, { id: "noon", label: "Noon", note: "12:00" },
    { id: "dusk", label: "Dusk", note: "18:30" }, { id: "midnight", label: "Midnight", note: "00:00" },
] as const
const mode = computed<"creative" | "survival" | null>({
    get: () => controls.value?.mode ?? null,
    set: (value) => { if (value) void send("world.mode", { mode: value }) },
})
const timeLabel = computed(() => formatTime(controls.value?.time ?? 0))

function formatTime(value: number) {
    const minutes = Math.round(value * 24 * 60) % (24 * 60)
    return `${String(Math.floor(minutes / 60)).padStart(2, "0")}:${String(minutes % 60).padStart(2, "0")}`
}
function outside(event: PointerEvent) {
    if (expanded.value && root.value && !root.value.contains(event.target as Node)) expanded.value = false
}
function keydown(event: KeyboardEvent) {
    if (expanded.value && event.key === "Escape") {
        event.preventDefault()
        event.stopPropagation()
        expanded.value = false
        trigger.value?.focus()
    }
}
onMounted(() => {
    document.addEventListener("pointerdown", outside)
    document.addEventListener("keydown", keydown)
})
onUnmounted(() => {
    document.removeEventListener("pointerdown", outside)
    document.removeEventListener("keydown", keydown)
})
watch(() => controls.value?.time, (value) => {
    if (value !== undefined && !editingTime.value) stagedTime.value = value
}, { immediate: true })
watch(controls, (value) => { if (!value) expanded.value = false })

async function send(name: "world.mode", payload: { mode: "creative" | "survival" }): Promise<void>
async function send(name: "world.time", payload: { time: number | "dawn" | "noon" | "dusk" | "midnight" }): Promise<void>
async function send(name: "world.clock", payload: { paused: boolean }): Promise<void>
async function send(name: "world.mode" | "world.time" | "world.clock", payload: { mode: "creative" | "survival" } | { time: number | "dawn" | "noon" | "dusk" | "midnight" } | { paused: boolean }) {
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
function setTime(value: number) {
    editingTime.value = false
    void send("world.time", { time: value })
}
</script>

<template>
    <div v-if="widgetConfig.ready && widgetConfig.worldControls && controls" ref="root" class="world-controls"
        :class="`corner-${widgetConfig.worldControlsCorner}`">
        <button ref="trigger" class="world-trigger" :class="{ active: expanded }" aria-label="World controls"
            aria-controls="world-controls-panel" :aria-expanded="expanded" @click="expanded = !expanded">
            <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M1 3h14v2H1zm0 4h14v2H1zm0 4h14v2H1z" />
                <path fill="var(--accent-hi)" d="M4 2h3v4H4zm6 4h3v4h-3zm-5 4h3v4H5z" />
            </svg>
            <span class="mode-badge">{{ controls.mode === 'creative' ? 'C' : 'S' }}</span>
        </button>

        <Panel v-if="expanded" id="world-controls-panel" title="World controls" class="world-panel">
            <template #actions><Button size="sm" @click="expanded = false">Done</Button></template>
            <WorkbenchColumns :list-width="184" :facts-width="236">
                <template #list>
                    <ListGroup title="World">
                        <ListItem :on="page === 'mode'" @click="page = 'mode'">Build mode</ListItem>
                        <ListItem :on="page === 'time'" @click="page = 'time'">Time & clock</ListItem>
                    </ListGroup>
                </template>
                <template #content>
                    <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
                    <template v-if="page === 'mode'">
                        <Section title="Build mode" hint="For this player">
                            <fieldset :disabled="busy"><Segmented v-model="mode" :options="modes" /></fieldset>
                            <p class="copy">Switch between survival play and free building. Your selected mode is saved for this world.</p>
                        </Section>
                        <Section title="Current rules">
                            <div class="status-row"><span>World damage</span><strong>{{ controls.damage ? 'Enabled' : 'Disabled' }}</strong></div>
                            <div class="status-row"><span>Player protection</span><strong>{{ controls.mode === 'creative' ? 'On' : 'World rules' }}</strong></div>
                        </Section>
                    </template>
                    <template v-else>
                        <Section title="Time of day" hint="World-wide">
                            <div class="preset-grid">
                                <Button v-for="time in times" :key="time.id" size="sm" :disabled="busy"
                                    @click="send('world.time', { time: time.id })">{{ time.label }} <small>{{ time.note }}</small></Button>
                            </div>
                            <Slider :model-value="stagedTime" label="Set time" :min="0" :max="0.99" :step="0.01" :format="formatTime"
                                @update:model-value="stagedTime = $event; editingTime = true" @change="setTime" />
                        </Section>
                        <Section title="Clock">
                            <fieldset :disabled="busy"><Toggle :model-value="!controls.paused" label="Time passes"
                                @update:model-value="send('world.clock', { paused: !$event })" /></fieldset>
                            <p class="copy">Pausing the clock holds the current time for everyone in this world.</p>
                        </Section>
                    </template>
                </template>
                <template #facts>
                    <h4>{{ page === 'mode' ? 'Build mode' : 'World clock' }}</h4>
                    <p class="dim">World controls · {{ page === 'mode' ? 'Player' : 'Time' }}</p>
                    <p class="copy">{{ page === 'mode'
                        ? 'Creative gives you building tools and protects your player. Survival restores the world’s normal rules.'
                        : 'Choose a moment directly or set the exact time. These changes affect the whole world.' }}</p>
                    <dl>
                        <div><dt>Mode</dt><dd>{{ controls.mode }}</dd></div>
                        <div><dt>Time</dt><dd>{{ timeLabel }}</dd></div>
                        <div><dt>Clock</dt><dd>{{ controls.paused ? 'Paused' : 'Running' }}</dd></div>
                        <div><dt>Damage</dt><dd>{{ controls.damage ? 'On' : 'Off' }}</dd></div>
                    </dl>
                </template>
            </WorkbenchColumns>
        </Panel>
    </div>
</template>

<style scoped>
.world-controls { position: fixed; z-index: 6; max-width: calc(100vw - 40px); }
.corner-0 { top: 20px; left: 20px; }
.corner-1 { top: 20px; right: 80px; }
.corner-2 { bottom: 20px; left: 20px; }
.corner-3 { bottom: 20px; right: 20px; }
.world-trigger { position: relative; display: grid; place-items: center; width: 48px; height: 48px; padding: 0; color: var(--ink); background: var(--stone-2); border: var(--b) solid var(--edge); box-shadow: var(--raised), var(--ledge); }
.world-trigger:hover, .world-trigger.active { background: var(--stone-3); outline: var(--b) solid var(--accent); }
.world-trigger:focus-visible { outline: var(--b) solid var(--accent-hi); }
.mode-badge { position: absolute; right: 2px; bottom: 2px; width: 15px; height: 15px; background: var(--accent-lo); border: 1px solid var(--edge); color: var(--ink); font: 10px/13px var(--font-display); text-align: center; }
.world-panel { position: absolute; top: 56px; left: 0; width: min(850px, calc(100vw - 40px)); height: min(570px, calc(100vh - 104px)); }
.corner-1 .world-panel { left: auto; right: 0; width: min(850px, calc(100vw - 100px)); }
.corner-2 .world-panel, .corner-3 .world-panel { top: auto; bottom: 56px; }
.corner-3 .world-panel { left: auto; right: 0; }
.world-panel :deep(.body) { padding: 0; overflow: hidden; }
.world-panel :deep(.workbench-columns) { height: 100%; }
fieldset { min-width: 0; margin: 0; padding: 0; border: 0; }
fieldset:disabled { opacity: .6; }
.preset-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px; }
.preset-grid :deep(.btn) { display: flex; justify-content: space-between; align-items: center; gap: 8px; min-width: 0; }
.preset-grid small { font: 10px var(--font-body); color: var(--ink-dim); }
.status-row { display: flex; justify-content: space-between; gap: 12px; padding: 9px 0; border-bottom: 1px solid var(--stone-3); font-size: 12px; }
.status-row strong { font: 11px var(--font-display); text-transform: uppercase; }
.world-panel :deep(.section) { max-width: 580px; }
.world-panel :deep(.workbench-content) { gap: 26px; }
</style>
