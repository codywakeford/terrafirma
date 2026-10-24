<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { BridgeError, type CommunityCatalog, type CommunityMod, type CommunityTexture, type CommunityWorld } from "../bridge"
import { useBridge } from "../bridge/use"
import PixelIcon from "../game/PixelIcon.vue"
import { Button, Field, Notice, Panel, Tabs } from "../kit"
import { Copier } from "./create/copy"
import CommunityMap from "./CommunityMap.vue"
import { decodeWorldCode } from "../worldgen/config"
import type { Screen } from "./screens"

type Group = "mods" | "textures" | "worlds" | "seeds"
type Source = "library" | "discover"
type Entry = { id: string; name: string; note: string; status?: string }
const emit = defineEmits<{ go: [screen: Screen]; createCode: [code: string] }>()
const bridge = useBridge()
const catalog = ref<CommunityCatalog | null>(null)
const group = ref<Group>("mods")
const source = ref<Source>("library")
const query = ref("")
const chosen = ref<string | null>(null)
const problem = ref<string | null>(null)
const busy = ref(false)
const copier = Copier()

const RAIL: { id: Group; label: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "mods", label: "Mods", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["...AAA...", "...ABA...", ".AAABAAA.", ".ABBBBBA.", "AABBBBBAA", ".ABBBBBA.", ".AAABAAA.", "...ABA...", "...AAA..."] },
    { id: "textures", label: "Textures", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["AAAAAAAAA", "ABBBABBBB", "ABBBABBBB", "ABBBABBBB", "AAAAAAAAA", "ABBBABBBB", "ABBBABBBB", "ABBBABBBB", "AAAAAAAAA"] },
    { id: "worlds", label: "Worlds", palette: { A: "#c9d4dc", B: "#4a9b64", C: "#40b8ec" },
        rows: ["...AAA...", ".AABBAA..", "AABBBBAA.", "ABBBCCBBA", "ABBBCCBBA", "AABBBBBA.", ".AABBAA..", "...AAA...", "........."] },
    { id: "seeds", label: "Seeds", palette: { A: "#c9d4dc", B: "#4a9b64" },
        rows: ["....A....", "...AAA...", "..AABAA..", ".AABBBAA.", "AAABBBAAA", "....B....", "...BBB...", "..BBBBB..", "........."] },
]
const TABS = [{ id: "library", label: "Library" }, { id: "discover", label: "Discover" }] as const
const groupLabel = computed(() => RAIL.find((item) => item.id === group.value)!.label)
const allEntries = computed<Entry[]>(() => {
    if (!catalog.value || source.value === "discover") return []
    switch (group.value) {
        case "mods": return catalog.value.mods.map((mod) => ({
            id: mod.id, name: mod.title, note: mod.source === "builtIn" ? "Terra Firma built-in" : mod.author || "Installed locally",
            status: mod.source === "builtIn" ? "Core" : mod.enabledWorlds.length ? `${mod.enabledWorlds.length} worlds` : undefined,
        }))
        case "textures": return catalog.value.textures.map((pack) => ({
            id: pack.id, name: pack.name, note: pack.author || "Installed locally", status: pack.active ? "Active" : undefined,
        }))
        case "worlds": return catalog.value.worlds.map((world) => ({
            id: world.path, name: world.name, note: world.code ? "Terra world" : "Local world",
        }))
        case "seeds": return catalog.value.worlds.filter((world) => world.code).map((world) => ({
            id: world.path, name: world.name, note: "From your world",
        }))
    }
})
const entries = computed(() => allEntries.value.filter((item) =>
    (item.name + " " + item.note).toLowerCase().includes(query.value.trim().toLowerCase())))
watch([group, source], () => { query.value = ""; chosen.value = null })
watch(entries, (items) => {
    if (!items.some((item) => item.id === chosen.value)) chosen.value = items[0]?.id ?? null
}, { immediate: true })
const selectedMod = computed<CommunityMod | null>(() => group.value === "mods" ? catalog.value?.mods.find((mod) => mod.id === chosen.value) ?? null : null)
const selectedTexture = computed<CommunityTexture | null>(() => group.value === "textures" ? catalog.value?.textures.find((pack) => pack.id === chosen.value) ?? null : null)
const selectedWorld = computed<CommunityWorld | null>(() => (group.value === "worlds" || group.value === "seeds")
    ? catalog.value?.worlds.find((world) => world.path === chosen.value) ?? null : null)
const codeIssue = computed(() => {
    if (!selectedWorld.value?.code) return null
    try { decodeWorldCode(selectedWorld.value.code); return null }
    catch (err) { return err instanceof Error ? err.message : String(err) }
})

function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function refresh() {
    catalog.value = await bridge.send("community.list", {})
}
async function setMod(mod: CommunityMod, world: CommunityWorld, enabled: boolean) {
    problem.value = null
    busy.value = true
    try {
        await bridge.send("community.mod.set", { id: mod.id, world: world.path, enabled })
        await refresh()
    } catch (err) { report(err) }
    finally { busy.value = false }
}
async function setTexture(id: string) {
    problem.value = null
    busy.value = true
    try {
        await bridge.send("community.texture.set", { id })
        await refresh()
    } catch (err) { report(err) }
    finally { busy.value = false }
}
async function play(world: CommunityWorld) {
    problem.value = null
    try { await bridge.send("play", { path: world.path }) }
    catch (err) { report(err) }
}
onMounted(() => refresh().catch(report))
</script>

<template>
    <Panel title="Community" class="community">
        <template #actions><Button size="sm" @click="emit('go', 'home')">Back</Button></template>
        <div class="frame">
            <nav class="rail" aria-label="Content types">
                <button v-for="item in RAIL" :key="item.id" :class="{ on: group === item.id }" :title="item.label"
                    :aria-label="item.label" :aria-current="group === item.id ? 'page' : undefined" @click="group = item.id">
                    <PixelIcon :rows="item.rows" :palette="item.palette" style="--u: 3px" />
                    <span>{{ item.label }}</span>
                </button>
            </nav>

            <section class="browser" :aria-label="`${groupLabel} list`">
                <div class="browser-head">
                    <span class="eyebrow">Community / {{ groupLabel }}</span>
                    <h2>{{ groupLabel }}</h2>
                    <Tabs v-model="source" :tabs="TABS" />
                    <Field v-if="source === 'library'" v-model="query" label="Search your library" :placeholder="`Find ${groupLabel.toLowerCase()}`" />
                </div>
                <div v-if="source === 'discover'" class="empty-list">
                    <strong>Discovery is on its way.</strong>
                    <p>Browse and install community {{ groupLabel.toLowerCase() }} here when the catalog is connected.</p>
                </div>
                <p v-else-if="!catalog && !problem" class="empty-list">Reading local content…</p>
                <div v-else-if="entries.length" class="entries">
                    <button v-for="entry in entries" :key="entry.id" class="entry" :class="{ on: chosen === entry.id }" @click="chosen = entry.id">
                        <span class="entry-icon">{{ entry.name.slice(0, 1).toUpperCase() }}</span>
                        <span class="entry-text"><strong>{{ entry.name }}</strong><small>{{ entry.note }}</small></span>
                        <span v-if="entry.status" class="entry-status">{{ entry.status }}</span>
                    </button>
                </div>
                <div v-else class="empty-list">
                    <strong>{{ query ? "No matches" : `No local ${groupLabel.toLowerCase()} yet` }}</strong>
                    <p v-if="!query && group === 'seeds'">Create a world to keep its code here.</p>
                    <p v-else-if="!query">Installed content will appear here.</p>
                </div>
            </section>

            <main class="detail">
                <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
                <template v-if="source === 'discover'">
                    <div class="detail-empty"><span class="big-chevron">»</span><h2>Discover {{ groupLabel }}</h2>
                        <p>The local library is ready. Online publishing, search, and installation will appear here once the community catalog is wired.</p>
                    </div>
                </template>
                <template v-else-if="selectedMod">
                    <div class="detail-title"><span class="eyebrow">{{ selectedMod.source === "builtIn" ? "Built-in mod" : "Installed mod" }}</span>
                        <h2>{{ selectedMod.title }}</h2><span class="byline">{{ selectedMod.name }}<template v-if="selectedMod.author"> · by {{ selectedMod.author }}</template></span></div>
                    <p class="description">{{ selectedMod.description || "This mod has no description in its mod.conf." }}</p>
                    <section v-if="selectedMod.depends.length" class="detail-section">
                        <h3>Requires</h3><div class="tags"><span v-for="dep in selectedMod.depends" :key="dep">{{ dep }}</span></div>
                    </section>
                    <Notice v-if="selectedMod.source === 'builtIn'">Required by this build. Built-in gameplay packs will become configurable as their platform dependencies are separated.</Notice>
                    <Notice v-else-if="!selectedMod.configurable" tone="warn">{{ selectedMod.issue }}</Notice>
                    <section v-else class="detail-section">
                        <h3>Use in worlds</h3>
                        <p class="section-copy">Install once, enable for each world. Changes take effect when that world next starts.</p>
                        <p v-if="!catalog?.worlds.length" class="dim">Create a world before enabling mods.</p>
                        <div v-for="world in catalog?.worlds" :key="world.path" class="world-row">
                            <span><strong>{{ world.name }}</strong><small>{{ selectedMod.enabledWorlds.includes(world.path) ? "Enabled" : "Off" }}</small></span>
                            <Button size="sm" :variant="selectedMod.enabledWorlds.includes(world.path) ? 'plain' : 'primary'" :disabled="busy"
                                @click="setMod(selectedMod, world, !selectedMod.enabledWorlds.includes(world.path))">
                                {{ selectedMod.enabledWorlds.includes(world.path) ? "Disable" : "Enable" }}
                            </Button>
                        </div>
                    </section>
                </template>
                <template v-else-if="selectedTexture">
                    <div class="detail-title"><span class="eyebrow">Texture pack</span><h2>{{ selectedTexture.name }}</h2>
                        <span v-if="selectedTexture.author" class="byline">by {{ selectedTexture.author }}</span></div>
                    <p class="description">{{ selectedTexture.description || "This pack has no description in its texture_pack.conf." }}</p>
                    <section class="detail-section"><h3>Appearance</h3>
                        <p class="section-copy">Texture packs change the look of every world on this installation. Restart Terra Firma to apply a change.</p>
                        <div class="actions">
                            <Button v-if="selectedTexture.active" size="sm" :disabled="busy" @click="setTexture('')">Use default textures</Button>
                            <Button v-else variant="primary" :disabled="busy" @click="setTexture(selectedTexture.id)">Use this pack »</Button>
                        </div>
                    </section>
                </template>
                <template v-else-if="selectedWorld">
                    <div class="detail-title"><span class="eyebrow">{{ group === "seeds" ? "World code" : "Your world" }}</span>
                        <h2>{{ selectedWorld.name }}</h2><span class="byline">Stored on this device</span></div>
                    <template v-if="selectedWorld.code">
                        <section class="detail-section"><h3>Planet code</h3>
                            <p class="section-copy">This code recreates the land, climate, and water. World rules and buildings stay with the saved world.</p>
                            <CommunityMap v-if="!codeIssue" :code="selectedWorld.code" />
                            <code class="world-code">{{ selectedWorld.code }}</code>
                            <div class="actions">
                                <Button size="sm" @click="copier.copy(selectedWorld.code!)">{{ copier.state.value === "copied" ? "Copied" : "Copy code" }}</Button>
                                <Button v-if="group === 'seeds'" variant="primary" :disabled="codeIssue !== null"
                                    @click="emit('createCode', selectedWorld.code)">Create from code »</Button>
                            </div>
                            <Notice v-if="codeIssue" tone="warn">{{ codeIssue }}</Notice>
                            <Notice v-if="copier.state.value === 'failed'" tone="warn">Clipboard access failed. Select the code to copy it.</Notice>
                        </section>
                    </template>
                    <Notice v-else>This world has no Terra world code. It cannot be shared as a seed.</Notice>
                    <div v-if="group === 'worlds'" class="actions"><Button variant="primary" @click="play(selectedWorld)">Play »</Button></div>
                </template>
                <div v-else class="detail-empty"><span class="big-chevron">»</span><h2>{{ groupLabel }}</h2>
                    <p>Select something from your library to inspect it here.</p>
                    <Button v-if="group === 'worlds' || group === 'seeds'" size="sm" @click="emit('go', 'create')">Create a world »</Button>
                </div>
            </main>
        </div>
    </Panel>
</template>

<style scoped>
.community { width: min(1340px, 97vw); height: min(840px, 94vh); }
.community :deep(.body) { overflow: hidden; padding: 0; }
.frame { display: grid; grid-template-columns: 76px 320px minmax(0, 1fr); height: 100%; min-height: 0; }
.rail { display: flex; flex-direction: column; gap: 6px; padding: 16px 8px; border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.rail button { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 9px 2px; border: var(--b) solid transparent; background: none; color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
.rail button:hover { color: var(--ink); background: var(--stone-2); }
.rail button.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.browser { min-width: 0; min-height: 0; display: flex; flex-direction: column; border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.browser-head { flex: none; display: grid; gap: 12px; padding: 20px 16px; border-bottom: 1px solid var(--stone-4); }
.eyebrow { color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; letter-spacing: .04em; }
h2 { margin: 0; font: 22px var(--font-display); text-transform: uppercase; line-height: 1.25; overflow-wrap: anywhere; }
.browser-head :deep(.tabs) { width: 100%; }
.browser-head :deep(.tabs button) { min-width: 0; padding: 0 8px; }
.entries { min-height: 0; overflow-y: auto; }
.entry { display: flex; gap: 10px; align-items: center; width: 100%; min-width: 0; padding: 10px 12px; border: 0; border-bottom: 1px solid var(--stone-3); background: transparent; text-align: left; }
.entry:hover { background: var(--stone-2); }
.entry.on { background: var(--stone-3); box-shadow: inset 4px 0 0 var(--accent), var(--raised); }
.entry-icon { flex: none; display: grid; place-items: center; width: 34px; height: 34px; border: var(--b) solid var(--edge); background: var(--stone-3); box-shadow: var(--raised); font: 13px var(--font-display); color: var(--accent-hi); }
.entry-text { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.entry-text strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 12px; }
.entry-text small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--ink-dim); font-size: 10px; }
.entry-status { flex: none; color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.empty-list { padding: 24px 16px; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.empty-list strong { color: var(--ink); font: 12px var(--font-display); text-transform: uppercase; }
.empty-list p { margin: 8px 0 0; }
.detail { min-width: 0; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: 22px; padding: 28px 34px; background: var(--stone-2); }
.detail-title { display: grid; gap: 7px; padding-bottom: 18px; border-bottom: 1px solid var(--stone-4); }
.byline, .dim { color: var(--ink-faint); font-size: 11px; }
.description { max-width: 60ch; margin: 0; color: var(--ink-dim); font-size: 13px; line-height: 1.6; }
.detail-section { display: grid; gap: 12px; }
h3 { margin: 0; font: 12px var(--font-display); text-transform: uppercase; }
.section-copy { max-width: 64ch; margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.tags { display: flex; flex-wrap: wrap; gap: 6px; }
.tags span { padding: 4px 8px; background: var(--stone-1); border: 1px solid var(--edge); color: var(--ink-dim); font-size: 11px; }
.world-row { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 10px 0; border-top: 1px solid var(--stone-4); }
.world-row > span { display: flex; flex-direction: column; min-width: 0; }
.world-row strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 12px; }
.world-row small { color: var(--ink-faint); font-size: 10px; }
.actions { display: flex; flex-wrap: wrap; gap: 8px; }
.world-code { display: block; overflow-wrap: anywhere; padding: 12px; background: var(--stone-0); border: var(--b) solid var(--edge); box-shadow: var(--sunk); color: var(--accent-hi); font-size: 12px; user-select: text; }
.detail-empty { display: grid; justify-items: start; align-content: center; gap: 12px; min-height: 100%; max-width: 440px; }
.detail-empty p { color: var(--ink-dim); line-height: 1.6; }
.big-chevron { color: var(--accent); font: 40px var(--font-display); }
@media (max-width: 850px) {
    .frame { grid-template-columns: 64px minmax(180px, 35%) minmax(0, 1fr); }
    .detail { padding: 20px; }
}
</style>
