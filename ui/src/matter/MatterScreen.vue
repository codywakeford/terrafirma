<script setup lang="ts">
import { computed, onMounted, ref, shallowRef, watch } from "vue"
import type { MatterElement, MatterReaction, MatterRock, MatterState, MatterSubstance } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button, Panel, Segmented } from "../kit"
import type { Screen } from "../menu/screens"
import { ENVIRONMENTS } from "./environments"
import PeriodicTable from "./PeriodicTable.vue"
import PhaseBox from "./PhaseBox.vue"
import PhaseDiagram from "./PhaseDiagram.vue"
import Reactor from "./Reactor.vue"
import Structure from "./Structure.vue"

/**
 * Matter: the building blocks. Every substance the engine knows, its
 * structure in 3D, and its phase diagram: choose a temperature and pressure
 * (or a world) and the engine says whether it's solid, liquid, gas or
 * supercritical there. The periodic table shows every element, lit where a
 * substance is modelled.
 */
defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()

const elements = shallowRef<MatterElement[]>([])
const substances = shallowRef<MatterSubstance[]>([])
const reactions = shallowRef<MatterReaction[]>([])
const rocks = shallowRef<MatterRock[]>([])
const rock = ref<string | null>(null) // a rock picked in the list
const pickedRock = computed(() => rocks.value.find((r) => r.id === rock.value) ?? null)
const bySymbol = computed(() => new Map(elements.value.map((e) => [e.symbol, e])))
const modelled = computed(() => new Set(substances.value.filter((s) => s.kind === "element").map((s) => Object.keys(s.composition)[0]!)))

const view = ref<"structure" | "table" | "reactor">("structure")
const chosen = ref<string>("water")
const element = ref<string | null>(null) // an element picked in the table that has no substance yet
const substance = computed(() => substances.value.find((s) => s.id === chosen.value) ?? null)
const groups = computed(() => [
    { label: "Elements", items: substances.value.filter((s) => s.kind === "element") },
    { label: "Compounds", items: substances.value.filter((s) => s.kind === "compound") },
])

const marker = ref({ t: 288, p: 101325 })
const reading = ref<{ state: MatterState; boils_at: number | null; melts_at: number | null } | null>(null)
let asking = false, again = false

async function ask() {
    if (!substance.value) return
    if (asking) { again = true; return }
    asking = true
    reading.value = await bridge.send("matter.state", { id: substance.value.id, t: marker.value.t, p: marker.value.p })
    asking = false
    if (again) { again = false; ask() }
}
watch([chosen, marker], ask, { deep: true })

onMounted(async () => {
    const table = await bridge.send("matter.list", {})
    elements.value = table.elements
    substances.value = table.substances
    reactions.value = table.reactions
    rocks.value = table.rocks
    ask()
})

function pickElement(symbol: string) {
    const s = substances.value.find((x) => x.kind === "element" && x.composition[symbol])
    if (s) { chosen.value = s.id; element.value = null; view.value = "structure" }
    else element.value = symbol
}

const heat = computed(() => {
    const s = substance.value
    return s ? Math.max(0, Math.min(1, marker.value.t / (s.critical[0] * 1.2))) : 0.5
})
const color = computed(() => {
    const s = substance.value
    const first = s ? Object.keys(s.composition)[0] : undefined
    return (first && bySymbol.value.get(first)?.color) || "#b0b8c0"
})
function substanceName(id: string) {
    return substances.value.find((s) => s.id === id)?.name ?? id
}
function mineralColour(id: string) {
    let h = 0
    for (const c of id) h = (h * 31 + c.charCodeAt(0)) % 360
    return `hsl(${h} 45% 55%)`
}
const celsius = (k: number) => `${Math.round(k)} K · ${Math.round(k - 273.15)} °C`
const pressure = (p: number) => p >= 1e9 ? `${(p / 1e9).toPrecision(3)} GPa` : p >= 1e6 ? `${(p / 1e6).toPrecision(3)} MPa`
    : p >= 1e3 ? `${(p / 1e3).toPrecision(3)} kPa` : `${p.toPrecision(3)} Pa`
const views = [{ value: "structure", label: "Structure" }, { value: "table", label: "Periodic table" }, { value: "reactor", label: "Reactor" }] as const
</script>

<template>
    <Panel title="Matter" class="matter">
        <template #actions><Button size="sm" @click="$emit('go', 'home')">Back</Button></template>

        <div class="layout" :class="{ wide: view === 'reactor' }">
            <nav class="list">
                <section v-for="g in groups" :key="g.label">
                    <h3>{{ g.label }}</h3>
                    <button v-for="s in g.items" :key="s.id" class="item" :class="{ on: chosen === s.id && !element && !rock }"
                        @click="chosen = s.id; element = null; rock = null">
                        <span class="formula">{{ s.formula }}</span><span>{{ s.name }}</span>
                    </button>
                </section>
                <section v-if="rocks.length">
                    <h3>Rocks</h3>
                    <button v-for="r in rocks" :key="r.id" class="item" :class="{ on: rock === r.id }"
                        @click="rock = r.id; element = null; view = view === 'reactor' ? 'structure' : view">
                        <span class="formula">{{ r.origin.slice(0, 4) }}</span><span>{{ r.name }}</span>
                    </button>
                </section>
            </nav>

            <main class="centre">
                <Segmented v-model="view" :options="views" class="views" />
                <Reactor v-if="view === 'reactor'" :substances="substances" :reactions="reactions" class="reactor" />
                <template v-else-if="view === 'structure'">
                    <div v-if="substance" class="stage">
                        <Structure :structure="substance.structure" :elements="bySymbol" class="model" />
                        <div class="box-wrap">
                            <PhaseBox v-if="reading" :state="reading.state" :heat="heat" :color="color" class="box" />
                            <p v-if="reading" class="state" :class="reading.state">{{ reading.state }}</p>
                        </div>
                    </div>
                </template>
                <PeriodicTable v-else-if="view === 'table'" :elements="elements" :modelled="modelled"
                    :selected="element ?? (substance?.kind === 'element' ? Object.keys(substance.composition)[0]! : null)"
                    @pick="pickElement" />
            </main>

            <aside v-if="view !== 'reactor'" class="side">
                <template v-if="pickedRock">
                    <h4>{{ pickedRock.name }} <span class="dim">{{ pickedRock.origin }}</span></h4>
                    <p class="copy">{{ pickedRock.note }}</p>
                    <div class="bar">
                        <span v-for="(share, id) in pickedRock.minerals" :key="id" :style="{ flexGrow: share, background: mineralColour(String(id)) }"
                            :title="`${substanceName(String(id))} ${Math.round(share * 100)}%`" />
                    </div>
                    <ul class="minerals">
                        <li v-for="(share, id) in pickedRock.minerals" :key="id">
                            <span class="dot" :style="{ background: mineralColour(String(id)) }" />
                            <button class="link" @click="chosen = String(id); rock = null">{{ substanceName(String(id)) }}</button>
                            <span class="dim">{{ Math.round(share * 100) }}%</span>
                        </li>
                    </ul>
                    <p class="dim">A mixture of minerals by mass: what it melts into and what it gives when smelted come from these.</p>
                </template>
                <template v-else-if="element && bySymbol.get(element)">
                    <h4>{{ bySymbol.get(element)!.name }} <span class="dim">{{ bySymbol.get(element)!.symbol }} · {{ bySymbol.get(element)!.number }}</span></h4>
                    <p class="dim">{{ bySymbol.get(element)!.category }} · {{ bySymbol.get(element)!.mass }} g/mol</p>
                    <p class="copy">Not modelled yet: its substance has no phase data. Lit elements have one.</p>
                </template>
                <template v-else-if="substance">
                    <h4>{{ substance.name }} <span class="dim">{{ substance.formula }}</span></h4>
                    <p class="copy">{{ substance.note }}</p>
                    <dl>
                        <div><dt>Molar mass</dt><dd>{{ substance.molar_mass.toFixed(2) }} g/mol</dd></div>
                        <div><dt>Triple point</dt><dd>{{ Math.round(substance.triple[0]) }} K · {{ pressure(substance.triple[1]) }}</dd></div>
                        <div><dt>Critical point</dt><dd>{{ Math.round(substance.critical[0]) }} K · {{ pressure(substance.critical[1]) }}</dd></div>
                        <div v-if="substance.boil"><dt>Boils at 1 atm</dt><dd>{{ celsius(substance.boil) }}</dd></div>
                        <div v-else-if="substance.sublime"><dt>Sublimes at 1 atm</dt><dd>{{ celsius(substance.sublime) }}</dd></div>
                        <div><dt>Density</dt><dd>{{ substance.density.solid }} solid · {{ substance.density.liquid }} liquid kg/m³</dd></div>
                        <div v-if="substance.hardness"><dt>Hardness</dt><dd>{{ substance.hardness }} Mohs</dd></div>
                        <div><dt>Latent heat</dt><dd>{{ substance.fusion }} melt · {{ substance.vaporization }} boil kJ/mol</dd></div>
                    </dl>
                    <p v-if="substance.approximate" class="dim">Some of these numbers are estimates.</p>

                    <div class="here">
                        <span>{{ celsius(marker.t) }} · {{ pressure(marker.p) }}</span>
                        <strong v-if="reading" :class="reading.state">{{ reading.state }}</strong>
                        <span v-if="reading?.boils_at" class="dim">boils at {{ Math.round(reading.boils_at) }} K here</span>
                        <span v-if="reading?.melts_at" class="dim">melts at {{ Math.round(reading.melts_at) }} K here</span>
                    </div>
                    <div class="diagram"><PhaseDiagram v-model="marker" :substance="substance" /></div>
                    <div class="worlds">
                        <button v-for="env in ENVIRONMENTS" :key="env.label" @click="marker = { t: env.t, p: env.p }">{{ env.label }}</button>
                    </div>
                </template>
            </aside>
        </div>
    </Panel>
</template>

<style scoped>
.matter { width: min(1260px, 96vw); height: min(800px, 92vh); }
.matter :deep(.body) { overflow: hidden; }
.layout { display: grid; grid-template-columns: 220px minmax(0, 1fr) 400px; gap: 0 22px; height: 100%; }
/* the Reactor brings its own reactions column */
.layout.wide { grid-template-columns: 220px minmax(0, 1fr); }
.reactor { flex: 1; min-height: 0; }
.list { overflow-y: auto; min-height: 0; padding-right: 4px; }
.list section + section { margin-top: 14px; }
h3 { margin: 0 0 6px; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.item { display: grid; grid-template-columns: 52px 1fr; align-items: center; width: 100%; margin-bottom: 3px; padding: 6px 8px;
    border: var(--b) solid transparent; background: none; color: var(--ink-dim); font-size: 12px; text-align: left; }
.item:hover { color: var(--ink); background: var(--stone-1); }
.item.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.formula { font: 12px var(--font-display); color: var(--ink); }

.centre, .side { display: flex; flex-direction: column; gap: 12px; min-height: 0; padding-left: 22px; border-left: var(--b) solid var(--edge); }
.views { width: 420px; }
.stage { flex: 1; min-height: 0; display: grid; grid-template-rows: 1fr 170px; gap: 12px; }
.model { min-height: 0; }
.box-wrap { position: relative; }
.box { width: 100%; height: 100%; }
.state { position: absolute; top: 8px; left: 10px; margin: 0; font: 12px var(--font-display); text-transform: uppercase; }

.side { overflow-y: auto; }
h4 { margin: 0; font: 400 18px var(--font-display); text-transform: uppercase; }
.dim { color: var(--ink-faint); font-size: 11px; margin: 0; }
.copy { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
dl { display: grid; gap: 4px; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 10px; font-size: 11px; }
dt { color: var(--ink-dim); }
dd { margin: 0; font-family: var(--font-display); font-size: 11px; text-align: right; }
.here { display: flex; flex-wrap: wrap; gap: 6px 12px; align-items: baseline; padding: 8px 10px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); font-size: 11px; }
.here strong { font: 14px var(--font-display); text-transform: uppercase; }
.solid { color: #8fb6e8; } .liquid { color: #40b8ec; } .gas { color: #d9b07a; } .supercritical { color: #c99be0; }
.diagram { flex: none; height: 280px; }
.worlds { display: flex; flex-wrap: wrap; gap: 6px; }
.bar { display: flex; height: 18px; border: var(--b) solid var(--edge); overflow: hidden; }
.bar span { min-width: 2px; }
.minerals { list-style: none; margin: 0; padding: 0; display: grid; gap: 4px; }
.minerals li { display: grid; grid-template-columns: 10px 1fr auto; gap: 8px; align-items: center; font-size: 12px; }
.dot { width: 10px; height: 10px; }
.link { border: 0; background: none; padding: 0; text-align: left; color: var(--accent-hi); font-size: 12px; }
.worlds button { border: var(--b) solid var(--edge); background: var(--stone-2); padding: 3px 7px; font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-dim); }
.worlds button:hover { color: var(--ink); }
</style>
