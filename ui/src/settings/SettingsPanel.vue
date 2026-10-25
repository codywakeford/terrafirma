<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { BridgeError } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button, Choice, ListEmpty, ListGroup, ListItem, Notice, Slider, Toggle, WorkbenchPanel } from "../kit"
import { CATEGORIES, GRAPHICS_PRESETS, type Option, type Page, type Category } from "./options"
import { BINDINGS } from "./bindings"
import Keybindings from "./Keybindings.vue"
import { publishAudioLevels } from "./audio"
import { applyWidgetSettings } from "../game/widgets/config"

const bridge = useBridge()
defineProps<{ closeLabel?: string }>()
const emit = defineEmits<{ close: [] }>()
const values = ref<Record<string, number | boolean> | null>(null)
const categoryId = ref("graphics")
const pageId = ref("presets")
const query = defineModel<string>("query", { required: true })
const selectedKey = ref<string | null>(null)
const problem = ref<string | null>(null)
const busy = ref(false)

const category = computed(() => CATEGORIES.find((c) => c.id === categoryId.value)!)
const page = computed(() => category.value.pages.find((p) => p.id === pageId.value) ?? category.value.pages[0]!)
const panelSection = computed(() => query.value.trim() ? "Search" : category.value.title)
const railChoice = computed({
    get: () => categoryId.value,
    set: (id: string) => { query.value = ""; categoryId.value = id },
})
const RAIL = [
    { id: "display", label: "Display", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["AAAAAAAAA", "ABBBBBBBA", "AB.....BA", "AB.....BA", "AB.....BA", "ABBBBBBBA", "AAAAAAAAA", "...AAA...", "..AAAAA.."] },
    { id: "graphics", label: "Graphics", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["AAAAAAAAA", "ABBBABBBB", "ABBBABBBB", "ABBBABBBB", "AAAAAAAAA", "ABBBABBBB", "ABBBABBBB", "ABBBABBBB", "AAAAAAAAA"] },
    { id: "controls", label: "Controls", shortLabel: "Input", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["...AAA...", "...ABA...", "...ABA...", "AAABABAAA", "ABBBBBBBA", "AAABABAAA", "...ABA...", "...ABA...", "...AAA..."] },
    { id: "audio", label: "Audio", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["...AAA...", "..AABA...", ".A.BBA...", "A..BBA...", "A..BBA.A.", "A..BBA.A.", ".A.BBA...", "..AABA...", "...AAA..."] },
    { id: "interface", label: "Interface", shortLabel: "UI", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["AAAAAAAAA", "ABBBBBBBA", "AB.....BA", "ABAAAAABA", "AB.....BA", "ABAAAAABA", "AB.....BA", "ABBBBBBBA", "AAAAAAAAA"] },
    { id: "hud", label: "HUD", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["AAAAAAAAA", "A.......A", "A.AAA.A.A", "A.ABA.A.A", "A.AAA.A.A", "A.......A", "A.AAA.A.A", "A.......A", "AAAAAAAAA"] },
    { id: "accessibility", label: "Accessibility", shortLabel: "Access", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["...AAA...", "...ABA...", "...AAA...", "AAAAAAAAA", ".AAABAAA.", "..A.A.A..", ".AA...AA.", ".A.....A.", "A.......A"] },
]
const pageGroups = computed(() => {
    const groups: { title: string; pages: Page[] }[] = []
    for (const item of category.value.pages) {
        let group = groups.find((g) => g.title === item.group)
        if (!group) { group = { title: item.group, pages: [] }; groups.push(group) }
        group.pages.push(item)
    }
    return groups
})
type SearchMatch = { key: string; label: string; category: Category; page: Page }
const matches = computed(() => {
    const words = query.value.trim().toLowerCase().split(/\s+/).filter(Boolean)
    if (!words.length) return []
    return CATEGORIES.flatMap((c) => c.pages.flatMap((p): SearchMatch[] => [
        ...p.options.filter((o) => words.every((word) =>
            `${c.title} ${p.title} ${o.label} ${o.note} ${o.terms ?? ""}`.toLowerCase().includes(word)))
            .map((o) => ({ key: o.key, label: o.label, category: c, page: p })),
        ...(p.bindings ? BINDINGS.filter((b) => words.every((word) =>
            `${c.title} ${p.title} ${b.label} ${b.group}`.toLowerCase().includes(word)))
            .map((b) => ({ key: b.key, label: b.label, category: c, page: p })) : []),
    ]))
})
const selected = computed(() => page.value.options.find((o) => o.key === selectedKey.value) ?? null)
const focusBinding = computed(() => page.value.bindings ? selectedKey.value : null)
const preset = computed(() => {
    if (!values.value) return null
    return GRAPHICS_PRESETS.find((p) =>
        Object.entries(p.values).every(([key, value]) => values.value?.[key] === value))?.id ?? null
})
const presetOptions = GRAPHICS_PRESETS.map(({ id, label, note }) => ({ id, label, note }))
const chosenPreset = computed({
    get: () => preset.value,
    set: (id: string | null) => { if (id) applyPreset(id) },
})

watch(categoryId, () => { pageId.value = category.value.pages[0]!.id }, { flush: "sync" })
watch(pageId, () => { selectedKey.value = null }, { flush: "sync" })

function numberValue(key: string) {
    const value = values.value?.[key]
    if (typeof value !== "number") throw new Error(`setting ${key} is not a number`)
    return value
}
function flagValue(key: string) {
    const value = values.value?.[key]
    if (typeof value !== "boolean") throw new Error(`setting ${key} is not a flag`)
    return value
}
function stage(key: string, value: number) {
    values.value = { ...values.value, [key]: value }
}
function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function save(key: string, value: number | boolean) {
    if (busy.value) return
    busy.value = true
    problem.value = null
    try { await bridge.send("settings.set", { key, value }) }
    catch (err) { report(err) }
    finally {
        try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
        catch (err) { report(err) }
        busy.value = false
    }
}
async function applyPreset(id: string) {
    const chosen = GRAPHICS_PRESETS.find((p) => p.id === id)
    if (!chosen || busy.value) return
    busy.value = true
    problem.value = null
    try { await bridge.send("settings.apply", { values: { ...chosen.values } }) }
    catch (err) { report(err) }
    finally {
        try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
        catch (err) { report(err) }
        busy.value = false
    }
}
/** An option's current value as the player reads it. */
function shown(o: Option) {
    const value = values.value?.[o.key]
    if (value === undefined) return "—"
    if (o.kind === "toggle") return value ? "On" : "Off"
    if (typeof value !== "number") throw new Error(`setting ${o.key} is not a number`)
    return o.format ? o.format(value) : String(value)
}
function range(o: Option & { kind: "slider" }) {
    return `${o.format ? o.format(o.min) : o.min} – ${o.format ? o.format(o.max) : o.max}`
}
function open(m: SearchMatch) {
    categoryId.value = m.category.id
    pageId.value = m.page.id
    selectedKey.value = m.key
}
onMounted(async () => {
    try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
    catch (err) { report(err) }
})
</script>

<template>
    <WorkbenchPanel v-model:group="railChoice" v-model:query="query" class="settings"
        title="Settings" :section="panelSection" :groups="RAIL" rail-label="Settings categories"
        search-label="Search settings" search-placeholder="Search settings…">
        <template #actions><Button v-if="closeLabel" size="sm" @click="emit('close')">{{ closeLabel }}</Button></template>
        <template #list>
            <template v-if="query.trim()">
                <template v-for="c in CATEGORIES" :key="c.id">
                    <ListGroup v-if="matches.some((m) => m.category.id === c.id)" :title="c.title">
                        <ListItem v-for="m in matches.filter((item) => item.category.id === c.id)" :key="m.key"
                            :on="selectedKey === m.key" @click="open(m)">
                            {{ m.label }}<template #sub>{{ m.page.title }}</template>
                        </ListItem>
                    </ListGroup>
                </template>
                <ListEmpty v-if="!matches.length">No settings match “{{ query.trim() }}”.</ListEmpty>
            </template>
            <template v-else>
                <ListGroup v-for="group in pageGroups" :key="group.title" :title="group.title">
                    <ListItem v-for="p in group.pages" :key="p.id" :on="pageId === p.id" @click="pageId = p.id">{{ p.title }}</ListItem>
                </ListGroup>
            </template>
        </template>

        <template #content>
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <p v-if="!values && !problem" class="dim">Reading settings…</p>
            <template v-if="values && page.preset">
                <Choice v-model="chosenPreset" :options="presetOptions" :disabled="busy" />
                <p class="dim">{{ preset ? "" : "Custom: your settings match no preset." }}</p>
            </template>
            <Keybindings v-if="page.bindings" :focus-key="focusBinding" />
            <div v-if="values && page.options.length" class="options">
                <div v-for="o in page.options" :key="o.key" class="option" :class="{ on: selectedKey === o.key }"
                    @pointerdown="selectedKey = o.key" @focusin="selectedKey = o.key">
                    <Slider v-if="o.kind === 'slider'" :model-value="numberValue(o.key)" :label="o.label"
                        :min="o.min" :max="o.max" :step="o.step" :format="o.format"
                        @update:model-value="stage(o.key, $event)" @change="save(o.key, $event)" />
                    <Toggle v-else :model-value="flagValue(o.key)" :label="o.label"
                        @update:model-value="save(o.key, $event)" />
                </div>
            </div>
        </template>

        <template #facts>
            <template v-if="selected">
                <h4>{{ selected.label }}</h4>
                <p class="dim">{{ category.title }} · {{ page.title }}</p>
                <p class="copy">{{ selected.note }}</p>
                <dl>
                    <div><dt>Now</dt><dd>{{ shown(selected) }}</dd></div>
                    <div v-if="selected.kind === 'slider'"><dt>Range</dt><dd>{{ range(selected) }}</dd></div>
                </dl>
                <button class="link" @click="selectedKey = null">« All of {{ page.title }}</button>
            </template>
            <template v-else>
                <h4>{{ page.title }}</h4>
                <p class="dim">{{ category.title }} · {{ page.group }}</p>
                <p class="copy">{{ page.note }}</p>
                <template v-if="page.preset">
                    <p class="copy">A preset sets render distance, distant terrain, shadows, bloom, fog and clouds together. Change any of them afterwards and the profile becomes custom.</p>
                    <dl v-if="values">
                        <div v-for="o in CATEGORIES.find((c) => c.id === 'graphics')!.pages.flatMap((p) => p.options)" :key="o.key">
                            <dt>{{ o.label }}</dt><dd>{{ shown(o) }}</dd>
                        </div>
                    </dl>
                </template>
                <dl v-if="values && page.options.length">
                    <div v-for="o in page.options" :key="o.key"><dt>{{ o.label }}</dt><dd>{{ shown(o) }}</dd></div>
                </dl>
            </template>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.options { display: grid; max-width: 620px; }
.option { padding: 12px 10px; border: var(--b) solid transparent; border-bottom-color: var(--stone-3); }
.option.on { border-color: var(--edge); background: var(--stone-1); box-shadow: var(--sunk); }
.settings :deep(.choice) { max-width: 620px; }
</style>
