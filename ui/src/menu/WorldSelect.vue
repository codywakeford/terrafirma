<script setup lang="ts">
import { Button, Panel } from "../kit"
import { computed, onMounted, ref } from "vue"
import { BridgeError, type World } from "../bridge"
import { useBridge } from "../bridge/use"
import type { Screen } from "./screens"

defineEmits<{ go: [screen: Screen] }>()

const bridge = useBridge()
const worlds = ref<World[] | null>(null)
const selected = ref<string | null>(null)
const confirming = ref(false)
const problem = ref<string | null>(null)

const current = computed(() => worlds.value?.find((w) => w.path === selected.value) ?? null)

/** Refusals are for the player; anything else is a bug and is reported as one. */
function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}

async function refresh() {
    worlds.value = await bridge.send("worlds.list", {})
    if (!current.value) selected.value = worlds.value[0]?.path ?? null
}

async function play(world: World) {
    problem.value = null
    await bridge.send("play", { path: world.path }).catch(report)
}

async function remove(world: World) {
    problem.value = null
    confirming.value = false
    try {
        await bridge.send("worlds.delete", { path: world.path })
        selected.value = null
        await refresh()
    } catch (err) {
        report(err)
    }
}

onMounted(() => refresh().catch(report))
</script>

<template>
    <Panel title="Worlds" class="worlds">
        <template #actions>
            <Button size="sm" @click="$emit('go', 'home')">Back</Button>
            <Button size="sm" variant="primary" @click="$emit('go', 'create')">+ New world</Button>
        </template>

        <p v-if="problem" class="problem">{{ problem }}</p>

        <ul v-if="worlds?.length" class="list">
            <li v-for="world in worlds" :key="world.path" :class="{ selected: world.path === selected }"
                @click="selected = world.path; confirming = false" @dblclick="play(world)">
                <span class="glyph">{{ world.name.slice(0, 1).toUpperCase() }}</span>
                <span class="meta">
                    <span class="name">{{ world.name }}</span>
                    <span class="path">{{ world.path.split('/').pop() }}</span>
                </span>
            </li>
        </ul>
        <div v-else-if="worlds" class="empty">
            <p>No worlds yet.</p>
            <Button variant="primary" @click="$emit('go', 'create')">Create your first world</Button>
        </div>

        <template v-if="current" #footer>
            <template v-if="confirming">
                <span class="warn">Delete “{{ current.name }}” forever?</span>
                <Button size="sm" @click="confirming = false">Keep it</Button>
                <Button size="sm" variant="danger" @click="remove(current)">Delete</Button>
            </template>
            <template v-else>
                <Button size="sm" variant="danger" @click="confirming = true">Delete</Button>
                <Button variant="primary" @click="play(current)">Play »</Button>
            </template>
        </template>
    </Panel>
</template>

<style scoped>
.worlds { width: min(680px, 92vw); height: min(560px, 80vh); }

.list { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 6px; }

.list li {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 10px 12px;
    border: var(--b) solid transparent;
    background: rgb(255 255 255 / 0.02);
    cursor: pointer;
}

.list li:hover { background: rgb(255 255 255 / 0.05); }
.list li.selected { border-color: var(--accent); background: rgb(0 148 210 / 0.12); }

.glyph {
    width: 40px;
    height: 40px;
    display: grid;
    place-items: center;
    border: var(--b) solid var(--edge);
    background: linear-gradient(180deg, #3c8a3e 0 30%, #6b4a2b 30%);
    box-shadow: var(--raised);
    font: 16px var(--font-display);
    text-shadow: var(--b) var(--b) 0 rgb(0 0 0 / 0.6);
}

.meta { display: flex; flex-direction: column; }
.name { font-size: 15px; }
.path { font-size: 11px; color: var(--ink-faint); }

.empty { display: grid; place-items: center; gap: 16px; padding: 60px 0; color: var(--ink-dim); }
.empty p { margin: 0; }
.problem { margin: 0 0 12px; color: var(--danger); }
.warn { margin-right: auto; color: var(--danger); }
</style>
