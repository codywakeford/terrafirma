<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { MatterState } from "../bridge"
import { useBridge } from "../bridge/use"
import { Segmented } from "../kit"
import { ENVIRONMENTS } from "../matter/environments"
import PeriodicTable from "../matter/PeriodicTable.vue"
import PhaseBox from "../matter/PhaseBox.vue"
import PhaseDiagram from "../matter/PhaseDiagram.vue"
import Structure from "../matter/Structure.vue"
import { useTable, useWiki } from "./nav"
import WikiLayout from "./WikiLayout.vue"

/**
 * Matter: every substance the engine knows, its structure in 3D, its phase
 * diagram (choose a temperature and pressure, or a world, and the engine
 * says what state it's in), the rocks made of them, the periodic table,
 * and the reactions each one takes part in.
 */
const bridge = useBridge()
const wiki = useWiki()
const table = useTable()

const view = ref<"structure" | "table">("structure")
const chosen = computed<string>({ get: () => wiki.state.chosen.matter ?? "water", set: (id) => { wiki.state.chosen.matter = id } })
const rock = ref<string | null>(null)
const element = ref<string | null>(null)
const substance = computed(() => table.byId.get(chosen.value) ?? null)
const pickedRock = computed(() => table.rocks.find((r) => r.id === rock.value) ?? null)
const modelled = computed(() => new Set(table.substances.filter((s) => s.kind === "element").map((s) => Object.keys(s.composition)[0]!)))
const groups = computed(() => [
    { label: "Elements", items: table.substances.filter((s) => s.kind === "element") },
    { label: "Compounds", items: table.substances.filter((s) => s.kind === "compound") },
])
// the reactions it's made by, and the ones that use it
const madeBy = computed(() => table.reactions.filter((r) => chosen.value in r.products))
const usedIn = computed(() => table.reactions.filter((r) => chosen.value in r.reactants))

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
watch([chosen, marker], ask, { deep: true, immediate: true })

function pickElement(symbol: string) {
    const s = table.substances.find((x) => x.kind === "element" && x.composition[symbol])
    if (s) { chosen.value = s.id; element.value = null; view.value = "structure" }
    else element.value = symbol
}
function pick(id: string) { chosen.value = id; element.value = null; rock.value = null }

const heat = computed(() => {
    const s = substance.value
    return s ? Math.max(0, Math.min(1, marker.value.t / (s.critical[0] * 1.2))) : 0.5
})
const color = computed(() => {
    const first = substance.value ? Object.keys(substance.value.composition)[0] : undefined
    return (first && table.bySymbol.get(first)?.color) || "#b0b8c0"
})
const substanceName = (id: string) => table.byId.get(id)?.name ?? id
function mineralColour(id: string) {
    let h = 0
    for (const c of id) h = (h * 31 + c.charCodeAt(0)) % 360
    return `hsl(${h} 45% 55%)`
}
const celsius = (k: number) => `${Math.round(k)} K · ${Math.round(k - 273.15)} °C`
const pressure = (p: number) => p >= 1e9 ? `${(p / 1e9).toPrecision(3)} GPa` : p >= 1e6 ? `${(p / 1e6).toPrecision(3)} MPa`
    : p >= 1e3 ? `${(p / 1e3).toPrecision(3)} kPa` : `${p.toPrecision(3)} Pa`
const views = [{ value: "structure", label: "Structure" }, { value: "table", label: "Periodic table" }] as const
</script>

<template>
    <WikiLayout>
        <template #list>
            <section v-for="g in groups" :key="g.label">
                <h3>{{ g.label }}</h3>
                <button v-for="s in g.items" :key="s.id" class="item" :class="{ on: chosen === s.id && !element && !rock }" @click="pick(s.id)">
                    <span class="formula">{{ s.formula }}</span><span>{{ s.name }}</span>
                </button>
            </section>
            <section v-if="table.rocks.length">
                <h3>Rocks</h3>
                <button v-for="r in table.rocks" :key="r.id" class="item" :class="{ on: rock === r.id }" @click="rock = r.id; element = null">
                    <span class="formula">{{ r.origin.slice(0, 4) }}</span><span>{{ r.name }}</span>
                </button>
            </section>
        </template>

        <template #view>
            <Segmented v-model="view" :options="views" class="views" />
            <div v-if="view === 'structure' && substance" class="stage">
                <Structure :structure="substance.structure" :elements="table.bySymbol" class="model" />
                <div class="box-wrap">
                    <PhaseBox v-if="reading" :state="reading.state" :heat="heat" :color="color" class="box" />
                    <p v-if="reading" class="state" :class="reading.state">{{ reading.state }}</p>
                </div>
            </div>
            <PeriodicTable v-else-if="view === 'table'" :elements="table.elements" :modelled="modelled"
                :selected="element ?? (substance?.kind === 'element' ? Object.keys(substance.composition)[0]! : null)" @pick="pickElement" />
        </template>

        <template #facts>
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
                        <button class="link" @click="pick(String(id))">{{ substanceName(String(id)) }}</button>
                        <span class="dim">{{ Math.round(share * 100) }}%</span>
                    </li>
                </ul>
                <p class="dim">A mixture of minerals by mass: what it melts into and what it gives when smelted come from these.</p>
            </template>
            <template v-else-if="element && table.bySymbol.get(element)">
                <h4>{{ table.bySymbol.get(element)!.name }} <span class="dim">{{ table.bySymbol.get(element)!.symbol }} · {{ table.bySymbol.get(element)!.number }}</span></h4>
                <p class="dim">{{ table.bySymbol.get(element)!.category }} · {{ table.bySymbol.get(element)!.mass }} g/mol</p>
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
                    <div><dt>Conducts heat</dt><dd>{{ substance.conductivity.solid }} solid · {{ substance.conductivity.liquid }} liquid W/(m·K)</dd></div>
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

                <template v-if="madeBy.length">
                    <h5>Made by</h5>
                    <div class="links"><button v-for="r in madeBy" :key="r.id" class="link" @click="wiki.go('reactions', r.id)">{{ r.name }}</button></div>
                </template>
                <template v-if="usedIn.length">
                    <h5>Used in</h5>
                    <div class="links"><button v-for="r in usedIn" :key="r.id" class="link" @click="wiki.go('reactions', r.id)">{{ r.name }}</button></div>
                </template>
            </template>
        </template>
    </WikiLayout>
</template>

<style scoped>
.views { width: 300px; }
.stage { flex: 1; min-height: 0; display: grid; grid-template-rows: 1fr 170px; gap: 12px; }
.model { min-height: 0; }
.box-wrap { position: relative; }
.box { width: 100%; height: 100%; }
.state { position: absolute; top: 8px; left: 10px; margin: 0; font: 12px var(--font-display); text-transform: uppercase; }
.here { display: flex; flex-wrap: wrap; gap: 6px 12px; align-items: baseline; padding: 8px 10px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); font-size: 11px; }
.here strong { font: 14px var(--font-display); text-transform: uppercase; }
.solid { color: #8fb6e8; } .liquid { color: #40b8ec; } .gas { color: #d9b07a; } .supercritical { color: #c99be0; }
.diagram { flex: none; height: 280px; }
.worlds { display: flex; flex-wrap: wrap; gap: 6px; }
.worlds button { border: var(--b) solid var(--edge); background: var(--stone-2); padding: 3px 7px; font: 10px var(--font-display); text-transform: uppercase; color: var(--ink-dim); }
.worlds button:hover { color: var(--ink); }
.bar { display: flex; height: 18px; border: var(--b) solid var(--edge); overflow: hidden; }
.bar span { min-width: 2px; }
.minerals { list-style: none; margin: 0; padding: 0; display: grid; gap: 4px; }
.minerals li { display: grid; grid-template-columns: 10px 1fr auto; gap: 8px; align-items: center; font-size: 12px; }
.dot { width: 10px; height: 10px; }
</style>
