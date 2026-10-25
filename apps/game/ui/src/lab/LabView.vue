<script setup lang="ts">
import { computed } from "vue"
import type { LabScenario, UiEvent } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import { Button, Segmented } from "../kit/index.ts"
import { useLab } from "./flow.ts"
import LabControls from "./LabControls.vue"

/**
 * The Lab page: scenarios down the left; on the right, the chosen scenario's
 * own world, live (the game draws through the clear middle), with its title
 * above and its controls below. The same page in the menu (choosing,
 * loading) and in a scenario's world (watching).
 */
const bridge = useBridge()
const store = useStore()
const lab = useLab()

const groups = computed(() => {
    const out = new Map<string, LabScenario[]>()
    for (const s of lab.state.scenarios) {
        const group = s.id.split(".")[0]!
        out.set(group, [...(out.get(group) ?? []), s])
    }
    return [...out.entries()]
})

const chosen = computed(() => lab.find(lab.state.chosen))
// the scenario's world is running and talking to us
const live = computed(() => store.mode === "game" && store.ui.lab !== null && store.ui.lab.id === lab.state.chosen)
const values = computed(() => (live.value && store.ui.lab ? store.ui.lab.values : {}))

const speeds = computed(() => (store.ui.lab?.speeds ?? [0, 1, 10, 100]).map((s) => ({ value: s, label: s === 0 ? "Pause" : `×${s}` })))
const speed = computed({
    get: () => store.ui.lab?.speed ?? 1,
    set: (s: number) => send({ name: "lab.speed", payload: { speed: s } }),
})

function send(event: UiEvent) {
    bridge.send("ui.send", event)
}
</script>

<template>
    <div class="lab" :class="{ live }">
        <aside class="list">
            <header>
                <h2><span class="chev">»</span> Lab</h2>
                <Button size="sm" @click="lab.back()">{{ store.mode === "game" ? "Stop" : "Back" }}</Button>
            </header>
            <p class="intro">Every mechanic, running in its own small world. Pick one to watch it.</p>
            <nav>
                <section v-for="[group, scenarios] in groups" :key="group">
                    <h3>{{ group }}</h3>
                    <button v-for="s in scenarios" :key="s.id" class="item" :class="{ on: lab.state.chosen === s.id }"
                        @click="lab.choose(s.id)">
                        {{ s.title }}
                    </button>
                </section>
            </nav>
        </aside>

        <main v-if="chosen" class="stage">
            <header class="about">
                <p class="kicker">{{ chosen.id.split(".")[0] }}</p>
                <h4>{{ chosen.title }}</h4>
                <p class="text">{{ chosen.description }}</p>
            </header>

            <div class="view">
                <p v-if="!live" class="waiting">{{ store.error ? store.error : `Building ${chosen.title}…` }}</p>
            </div>

            <footer class="deck">
                <LabControls v-if="chosen.controls?.length" :controls="chosen.controls" :values="values"
                    :disabled="!live" @set="(id, value) => send({ name: 'lab.control', payload: { id: id, value: value } })" />
                <div class="always" :class="{ disabled: !live }">
                    <Button @click="send({ name: 'lab.restart', payload: {} })">Restart</Button>
                    <Segmented v-model="speed" :options="speeds" class="speed" />
                    <Button variant="primary" @click="bridge.send('screen.close', {})">Step in</Button>
                </div>
            </footer>
        </main>
        <main v-else class="stage empty">
            <p class="waiting">Pick a scenario on the left.</p>
        </main>
    </div>
</template>

<style scoped>
.lab {
    position: fixed;
    inset: 0;
    display: grid;
    grid-template-columns: min(340px, 30vw) 1fr;
    background: #070b10;
}
/* in a scenario's world the game shows through the middle of the stage */
.lab.live { background: transparent; }

.list {
    display: flex;
    flex-direction: column;
    min-height: 0;
    border-right: var(--b) solid var(--edge);
    background: rgb(17 20 23 / 0.96);
    box-shadow: var(--raised);
}
.list header { display: flex; align-items: center; justify-content: space-between; padding: 18px 20px; border-bottom: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised); }
h2 { margin: 0; font: 400 22px var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; }
.chev { color: var(--accent); }
.intro { margin: 0; padding: 14px 20px 0; color: var(--ink-dim); font-size: 13px; line-height: 1.5; }
nav { flex: 1; min-height: 0; overflow: hidden auto; padding: 14px 20px; }
nav section + section { margin-top: 14px; }
h3 { margin: 0 0 6px; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }

.item {
    display: block;
    width: 100%;
    margin-bottom: 4px;
    padding: 9px 12px;
    border: var(--b) solid var(--edge);
    background: var(--stone-2);
    box-shadow: var(--raised);
    color: var(--ink-dim);
    font: 13px var(--font);
    text-align: left;
}
.item:hover { color: var(--ink); }
.item.on { background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }

.stage { display: grid; grid-template-rows: auto 1fr auto; min-width: 0; min-height: 0; }
.stage.empty { place-items: center; grid-template-rows: 1fr; }

.about, .deck { background: rgb(17 20 23 / 0.94); box-shadow: var(--raised); }
.about { padding: 16px 22px; border-bottom: var(--b) solid var(--edge); }
.kicker { margin: 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--accent); }
h4 { margin: 4px 0 6px; font: 400 20px var(--font-display); text-transform: uppercase; }
.text { margin: 0; max-width: 80ch; color: var(--ink-dim); font-size: 13px; line-height: 1.5; }

.view { display: grid; place-items: center; min-height: 0; }
.waiting { margin: 0; font: 12px var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; color: var(--ink-faint); }

.deck { display: grid; grid-template-columns: 1fr auto; gap: 22px; align-items: end; padding: 16px 22px; border-top: var(--b) solid var(--edge); }
.deck > :only-child { grid-column: 1 / -1; justify-self: end; }
.always { display: flex; gap: 10px; align-items: center; }
.always.disabled { opacity: 0.5; pointer-events: none; }
.speed { width: 280px; }
</style>
