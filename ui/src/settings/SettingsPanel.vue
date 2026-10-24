<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { BridgeError } from "../bridge"
import { useBridge } from "../bridge/use"
import { Choice, Field, Notice, Slider, Toggle } from "../kit"
import { CATEGORIES, GRAPHICS_PRESETS } from "./options"

const bridge = useBridge()
const values = ref<Record<string, number | boolean> | null>(null)
const categoryId = ref("graphics")
const pageId = ref("presets")
const query = ref("")
const selectedKey = ref<string | null>(null)
const problem = ref<string | null>(null)
const busy = ref(false)

const category = computed(() => CATEGORIES.find((c) => c.id === categoryId.value)!)
const page = computed(() => category.value.pages.find((p) => p.id === pageId.value) ?? category.value.pages[0]!)
const matches = computed(() => {
    const words = query.value.trim().toLowerCase().split(/\s+/).filter(Boolean)
    if (!words.length) return []
    return CATEGORIES.flatMap((c) => c.pages.flatMap((p) => p.options
        .filter((o) => words.every((word) => `${c.title} ${p.title} ${o.label} ${o.note} ${o.terms ?? ""}`.toLowerCase().includes(word)))
        .map((o) => ({ category: c, page: p, option: o }))))
})
const searchedOption = computed(() => matches.value.find((m) => m.option.key === selectedKey.value))
const shownOptions = computed(() => query.value.trim()
    ? searchedOption.value ? [searchedOption.value.option] : []
    : page.value.options)
const shownTitle = computed(() => query.value.trim()
    ? searchedOption.value?.option.label ?? "Search settings" : page.value.title)
const shownNote = computed(() => query.value.trim()
    ? searchedOption.value?.option.note ?? "Select a result to change it." : page.value.note)
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

watch(categoryId, () => { pageId.value = category.value.pages[0]!.id; selectedKey.value = null }, { flush: "sync" })
watch(query, () => { selectedKey.value = null })

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
        try { values.value = await bridge.send("settings.get", {}) }
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
        try { values.value = await bridge.send("settings.get", {}) }
        catch (err) { report(err) }
        busy.value = false
    }
}
function showResult(key: string) { selectedKey.value = key }
function openResult(key: string) {
    const found = matches.value.find((m) => m.option.key === key)
    if (!found) return
    query.value = ""
    categoryId.value = found.category.id
    pageId.value = found.page.id
    selectedKey.value = null
}
onMounted(async () => {
    try { values.value = await bridge.send("settings.get", {}) }
    catch (err) { report(err) }
})
</script>

<template>
    <div class="settings-panel">
        <div class="search">
            <Field v-model="query" label="Search settings" placeholder="Find a setting…" />
            <span class="search-hint">{{ query ? `${matches.length} result${matches.length === 1 ? "" : "s"}` : "PLAYER PREFERENCES" }}</span>
        </div>
        <div class="columns">
            <nav class="categories" aria-label="Settings categories">
                <span class="rail-label">SETTINGS</span>
                <button v-for="c in CATEGORIES" :key="c.id" :class="{ on: categoryId === c.id }"
                    :aria-current="categoryId === c.id ? 'page' : undefined" @click="categoryId = c.id">
                    <span class="mark">{{ c.mark }}</span><span>{{ c.title }}</span>
                </button>
            </nav>
            <nav class="pages" :aria-label="query ? 'Search results' : category.title + ' pages'">
                <div class="pages-heading">
                    <span class="eyebrow">{{ query ? "SEARCH" : "SETTINGS / " + category.title.toUpperCase() }}</span>
                    <h2>{{ query ? "Results" : category.title }}</h2>
                    <p>{{ query ? "Across all settings" : "Choose a page" }}</p>
                </div>
                <template v-if="query">
                    <button v-for="m in matches" :key="m.option.key" class="page-row" :class="{ on: selectedKey === m.option.key }"
                        @click="showResult(m.option.key)" @dblclick="openResult(m.option.key)">
                        <strong>{{ m.option.label }}</strong>
                        <small>{{ m.category.title }} » {{ m.page.title }}</small>
                    </button>
                    <p v-if="!matches.length" class="empty">No settings match “{{ query }}”.</p>
                </template>
                <template v-else>
                    <button v-for="p in category.pages" :key="p.id" class="page-row" :class="{ on: pageId === p.id }"
                        :aria-current="pageId === p.id ? 'page' : undefined" @click="pageId = p.id">
                        <strong>{{ p.title }}</strong>
                        <small>{{ p.note }}</small>
                    </button>
                </template>
            </nav>
            <main class="detail">
                <div class="detail-head">
                    <span class="eyebrow">{{ query && searchedOption
                        ? searchedOption.category.title + " / " + searchedOption.page.title
                        : category.title + " / " + page.title }}</span>
                    <h2>{{ shownTitle }}</h2>
                    <p>{{ shownNote }}</p>
                </div>
                <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
                <p v-if="!values && !problem" class="loading">Reading settings…</p>
                <template v-if="values && !query && page.preset">
                    <div class="preset-intro">
                        <strong>Graphics profile</strong>
                        <span>{{ preset ? GRAPHICS_PRESETS.find((p) => p.id === preset)?.label + " selected" : "Custom settings" }}</span>
                    </div>
                    <Choice v-model="chosenPreset" :options="presetOptions" :disabled="busy" />
                    <p class="footnote">A preset changes render distance, distant terrain, shadows, bloom, fog, and clouds together. Adjusting any of them afterward makes a custom profile.</p>
                </template>
                <div v-if="values && shownOptions.length" class="option-list">
                    <div v-for="o in shownOptions" :key="o.key" class="option">
                        <Slider v-if="o.kind === 'slider'" :model-value="numberValue(o.key)" :label="o.label"
                            :min="o.min" :max="o.max" :step="o.step" :format="o.format"
                            @update:model-value="stage(o.key, $event)" @change="save(o.key, $event)" />
                        <Toggle v-else :model-value="flagValue(o.key)" :label="o.label"
                            @update:model-value="save(o.key, $event)" />
                        <p>{{ o.note }}</p>
                    </div>
                </div>
                <p v-if="query && !searchedOption" class="search-empty">{{ matches.length ? "Select a result to inspect it." : "Try another word or search by category." }}</p>
            </main>
        </div>
    </div>
</template>

<style scoped>
.settings-panel { height: 100%; min-height: 0; display: flex; flex-direction: column; background: var(--stone-1); }
.search { display: flex; align-items: end; gap: 20px; flex: none; padding: 12px 18px 14px; border-bottom: var(--b) solid var(--edge); background: var(--stone-2); }
.search :deep(.field) { width: min(520px, 100%); }
.search-hint { margin-left: auto; padding-bottom: 10px; color: var(--ink-faint); font: 10px var(--font-display); white-space: nowrap; }
.columns { display: grid; grid-template-columns: 178px 270px minmax(0, 1fr); min-height: 0; flex: 1; }
.categories, .pages, .detail { min-width: 0; min-height: 0; overflow-y: auto; }
.categories { display: flex; flex-direction: column; gap: 3px; padding: 18px 8px; border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.rail-label, .eyebrow { font: 10px var(--font-display); color: var(--accent-hi); letter-spacing: .04em; }
.rail-label { padding: 0 10px 12px; }
.categories button { display: flex; align-items: center; gap: 10px; width: 100%; min-width: 0; padding: 10px 8px; border: var(--b) solid transparent; background: transparent; color: var(--ink-dim); text-align: left; font: 11px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.categories button:hover, .page-row:hover { color: var(--ink); background: var(--stone-2); }
.categories button.on { border-color: var(--edge); background: var(--stone-3); color: var(--ink); box-shadow: inset 4px 0 0 var(--accent), var(--raised); }
.mark { width: 22px; flex: none; text-align: center; color: var(--accent-hi); font: 17px var(--font); }
.pages { border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.pages-heading { display: grid; gap: 7px; padding: 24px 18px 18px; border-bottom: 1px solid var(--stone-4); }
.pages-heading h2, .detail-head h2 { margin: 0; font: 22px var(--font-display); text-transform: uppercase; line-height: 1.25; overflow-wrap: anywhere; }
.pages-heading p, .detail-head p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.page-row { display: grid; gap: 5px; width: 100%; padding: 14px 18px; border: 0; border-bottom: 1px solid var(--stone-3); background: transparent; text-align: left; color: var(--ink-dim); }
.page-row.on { background: var(--stone-3); box-shadow: inset 4px 0 0 var(--accent), var(--raised); color: var(--ink); }
.page-row strong { font: 12px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.page-row small { color: var(--ink-faint); font-size: 10px; line-height: 1.35; }
.empty { padding: 18px; color: var(--ink-dim); font-size: 12px; }
.detail { display: flex; flex-direction: column; gap: 22px; padding: 28px 34px; background: var(--stone-2); }
.detail-head { display: grid; gap: 8px; padding-bottom: 20px; border-bottom: 1px solid var(--stone-4); }
.detail-head p { max-width: 58ch; }
.option-list { display: grid; max-width: 660px; }
.option { padding: 18px 0; border-bottom: 1px solid var(--stone-4); }
.option:first-child { padding-top: 0; }
.option > p, .footnote { margin: 8px 0 0; color: var(--ink-faint); font-size: 11px; line-height: 1.5; }
.option :deep(.slider) { max-width: 540px; }
.preset-intro { display: flex; justify-content: space-between; gap: 12px; max-width: 660px; }
.preset-intro strong { font: 12px var(--font-display); text-transform: uppercase; }
.preset-intro span { color: var(--accent-hi); font-size: 11px; }
.detail :deep(.choice) { max-width: 660px; }
.footnote { max-width: 62ch; margin-top: -8px; }
.loading, .search-empty { color: var(--ink-dim); font-size: 12px; }
@media (max-width: 900px) {
    .columns { grid-template-columns: 150px 220px minmax(0, 1fr); }
    .detail { padding: 22px; }
    .categories button { font-size: 10px; }
}
</style>
