<script setup lang="ts">
import { computed, nextTick, onMounted, provide, ref, watch } from "vue"
import { BridgeError } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Button, ListItem, WorkbenchPanel } from "../../kit/index.ts"
import { body as bodyOf, STAR } from "../../system/model.ts"
import { SYSTEM_PREVIEW, SystemPreview } from "../../system/preview.ts"
import { decodeWorldCode, encodeWorldCode } from "../../worldgen/config.ts"
import { worldName } from "../../worldgen/names.ts"
import type { Screen } from "../screens.ts"
import BodyNavigator from "./BodyNavigator.vue"
import { WorldDraft } from "./draft.ts"
import DynamicsTab from "./DynamicsTab.vue"
import GameTab from "./GameTab.vue"
import Inspector from "./Inspector.vue"
import { LOOKS, Looks } from "./looks.ts"
import { worldSettings } from "./rules.ts"
import SystemControls from "./SystemControls.vue"
import Viewer from "./Viewer.vue"

type Group = "start" | "system" | "body" | "rules"
type Page = "quick" | "system-overview" | "star" |
    "overview" | "orbit" | "makeup" | "spin" | "land" | "climate" | "underground" | "history" |
    "survival" | "danger" | "time" | "knowledge"
type PageSpec = { id: Page; label: string; note?: string; status?: "preview" | "planned" }
type PageGroup = { label: string; pages: PageSpec[] }
type BodyPage = "overview" | "orbit" | "makeup" | "spin" | "land" | "climate" | "underground" | "history"
type RulesPage = "survival" | "danger" | "time" | "knowledge"

const emit = defineEmits<{ go: [screen: Screen] }>()
const props = defineProps<{ initialCode?: string | null }>()
const bridge = useBridge()
const draft = WorldDraft()
if (props.initialCode) draft.config = decodeWorldCode(props.initialCode)
const system = SystemPreview({ bridge })
provide(SYSTEM_PREVIEW, system)
watch(() => [draft.system, draft.config.star], () => system.request(draft.system, draft.config.star), { deep: true, immediate: true })
const looks = Looks({ bridge, draft, cards: system })
provide(LOOKS, looks)

const group = ref<Group>("start")
const page = ref<Page>("quick")
const problem = ref<string | null>(null)
const busy = ref(false)
const view = ref<"globe" | "map" | "system">("globe")
const code = computed(() => encodeWorldCode(draft.config))
const chosen = computed(() => draft.system.selected === STAR ? null : bodyOf(draft.system, draft.system.selected))
const currentPages = computed(() => PAGE_GROUPS[group.value])
const pageSpec = computed(() => currentPages.value.flatMap((item) => item.pages).find((item) => item.id === page.value)!)
const title = computed(() => RAIL.find((item) => item.id === group.value)!.label)
const bodyPage = computed(() => page.value as BodyPage)
const rulesPage = computed(() => page.value as RulesPage)
const RAIL = [
    { id: "start", label: "Start", rows: ["...AAA...", "..ABBBA..", ".ABBBBBA.", "ABBBBBBBA", "ABBBBBBBA", ".ABBBBBA.", "..ABBBA..", "...AAA...", "........."], palette: { A: "#c9d4dc", B: "#40b8ec" } },
    { id: "system", label: "System", rows: ["....A....", "....A....", ".B..A..B.", "..BAAAB..", "AAABABAAA", "..BAAAB..", ".B..A..B.", "....A....", "....A...."], palette: { A: "#c9d4dc", B: "#d9ae69" } },
    { id: "body", label: "Body", rows: ["..AAAAA..", ".ABBBBBA.", "ABBBBBAAA", "ABBBBAAAA", "ABBBBBAAA", "ABBBBBBAA", ".ABBBBBA.", "..AAAAA..", "........."], palette: { A: "#c9d4dc", B: "#46a6d5" } },
    { id: "rules", label: "Rules", rows: ["..AAAAA..", ".ABBBBBA.", ".AB.A.BA.", ".ABBBBBA.", ".AB.A.BA.", ".ABBBBBA.", "..AAAAA..", "...AAA...", "....A...."], palette: { A: "#c9d4dc", B: "#72b18a" } },
] as const
const PAGE_GROUPS: Record<Group, PageGroup[]> = {
    start: [{ label: "Create", pages: [{ id: "quick", label: "Quick start" }] }],
    system: [
        { label: "Solar system", pages: [{ id: "system-overview", label: "Overview", status: "preview" }, { id: "star", label: "Star" }] },
    ],
    body: [
        { label: "Selected body", pages: [{ id: "overview", label: "Overview", status: "preview" }, { id: "orbit", label: "Orbit", status: "preview" }, { id: "makeup", label: "Composition", status: "preview" }, { id: "spin", label: "Rotation & life", status: "preview" }] },
        { label: "Surface", pages: [{ id: "land", label: "Land & water" }, { id: "climate", label: "Climate & seasons" }, { id: "underground", label: "Underground", status: "planned" }] },
        { label: "Story", pages: [{ id: "history", label: "History", status: "preview" }] },
    ],
    rules: [
        { label: "Survival", pages: [{ id: "survival", label: "The body" }, { id: "danger", label: "Danger" }] },
        { label: "World rhythm", pages: [{ id: "time", label: "Time & weather" }, { id: "knowledge", label: "Knowledge" }] },
    ],
}
const DEFAULT_PAGE: Record<Group, Page> = { start: "quick", system: "system-overview", body: "overview", rules: "survival" }
watch(group, (next) => { page.value = DEFAULT_PAGE[next] })

async function openDetail(nextGroup: Group, nextPage: Page) {
    group.value = nextGroup
    await nextTick()
    page.value = nextPage
}

onMounted(async () => {
    try {
        draft.taken = (await bridge.send("worlds.list", {})).map((world) => world.name)
        if (draft.taken.some((name) => name.toLowerCase() === draft.name.toLowerCase())) draft.name = worldName(draft.taken)
    } catch (err) { problem.value = err instanceof Error ? err.message : String(err) }
})

async function create() {
    problem.value = null
    busy.value = true
    try {
        const world = await bridge.send("worlds.create", { name: draft.name, code: code.value, settings: worldSettings(draft.mode, draft.rules, draft.config) })
        await bridge.send("play", { path: world.path })
    } catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
        busy.value = false
    }
}
</script>

<template>
    <WorkbenchPanel v-model:group="group" class="create-workbench" title="Create world" :section="title" :groups="RAIL" rail-label="World creator sections">
        <template #actions>
            <Button size="sm" @click="emit('go', 'worlds')">Back</Button>
        </template>
        <template #workspace>
            <div class="creator-stage">
                <nav v-if="group !== 'start'" class="pages" aria-label="Editor pages">
                    <section v-for="set in currentPages" :key="set.label">
                        <h3>{{ set.label }}</h3>
                        <ListItem v-for="item in set.pages" :key="item.id" :on="page === item.id" @click="page = item.id">
                            {{ item.label }}
                            <template v-if="item.status" #trail>{{ item.status }}</template>
                        </ListItem>
                    </section>
                </nav>

                <div class="controls" :class="{ 'start-controls': group === 'start' }">
                    <header v-if="group !== 'start'" class="page-head">
                        <span>{{ title }} / {{ pageSpec.label }}</span>
                        <h2>{{ pageSpec.label }}</h2>
                        <p v-if="group === 'body' && !chosen?.playable && ['land', 'climate', 'underground'].includes(page)">This body is a visual preview. Its surface is not playable yet.</p>
                        <p v-else-if="pageSpec.status === 'preview'">System preview · these changes are not saved into the playable world yet.</p>
                        <p v-else-if="pageSpec.status === 'planned'">Saved in Earth's world code; the detailed underground model and preview are still being built.</p>
                    </header>
                    <GameTab v-if="page === 'quick'" :draft="draft"
                        @fine-tune-land="openDetail('body', 'land')"
                        @fine-tune-rules="openDetail('rules', 'survival')" />
                    <SystemControls v-else-if="group === 'system'" :draft="draft" :page="page === 'star' ? 'star' : 'overview'" />
                    <Inspector v-else-if="group === 'body'" :draft="draft" :page="bodyPage" />
                    <DynamicsTab v-else-if="group === 'rules'" :draft="draft" :page="rulesPage" />
                </div>

                <div class="visual">
                    <BodyNavigator v-model:view="view" :system="draft.system" :star="draft.config.star" />
                    <Viewer v-model:view="view" :draft="draft" />
                </div>
            </div>
        </template>
        <template #footer>
            <span class="save-status"><strong>Saved:</strong> Earth · rules <span> / </span> <strong>Preview:</strong> other bodies</span>
            <span v-if="problem" class="problem" role="alert">{{ problem }}</span>
            <Button variant="primary" :disabled="busy || !draft.name.trim()" @click="create">Create & play »</Button>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.create-workbench { width: min(1400px, 98vw); height: min(800px, 91vh); }
.creator-stage { display: grid; grid-template-columns: 190px 355px minmax(0, 1fr); width: 100%; height: 100%; min-width: 0; min-height: 0; }
.pages, .controls { min-width: 0; min-height: 0; overflow-y: auto; }
.pages { padding: 18px 8px; border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.pages section + section { margin-top: 18px; }
.pages h3 { margin: 0 0 6px; padding: 0 7px; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
.controls { display: flex; flex-direction: column; gap: 18px; padding: 20px 18px 28px; border-right: var(--b) solid var(--edge); background: var(--stone-0); }
.start-controls { grid-column: 1 / 3; }
.page-head { display: grid; gap: 8px; padding-bottom: 15px; border-bottom: 1px solid var(--stone-4); }
.page-head span { color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.page-head h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.page-head p { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.visual { display: flex; flex-direction: column; min-width: 0; min-height: 0; overflow: hidden; background: var(--stone-0); }
.visual :deep(.viewer) { flex: 1; min-height: 0; padding: 12px 16px; }
.save-status { margin-right: auto; color: var(--ink-faint); font-size: 10px; }
.save-status strong { color: var(--ink-dim); font-weight: 400; }
.save-status span { padding: 0 4px; }
.problem { max-width: 360px; color: var(--danger); font-size: 11px; }
@media (max-width: 1100px) { .creator-stage { grid-template-columns: 150px 300px minmax(0, 1fr); } }
</style>
