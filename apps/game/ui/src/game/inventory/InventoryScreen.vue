<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import ItemSlot from "../ItemSlot.vue"
import { useStore } from "../store.ts"
import { Carry } from "./carry.ts"
import CreativePanel from "./CreativePanel.vue"
import SurvivalPanel from "./SurvivalPanel.vue"

/**
 * The inventory, laid out as Minecraft's: in survival, the player's own
 * inventory; in creative, the catalogue in tabs (icons along the top, the
 * player's inventory along the bottom), the search box always there. One
 * frame whose size never changes with the tab. E or Esc closes it (unless
 * you're typing in the search). What's carried, and every click, is in
 * carry.ts; the engine owns every stack.
 */
const bridge = useBridge()
const store = useStore()
const carry = Carry({ bridge: bridge, store: store })

const creative = computed(() => store.ui.creative === true)
const catalog = shallowRef<CreativeItem[] | null>(null)
const tab = ref("building")
const query = ref("")
const search = ref<HTMLInputElement | null>(null)
const over = ref<{ slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | null>(null)

// the catalogue's tabs, each drawn as an item from it (as Minecraft's)
const TABS = [
    { id: "building", label: "Building blocks", icon: "tf_world:stone_bricks" },
    { id: "nature", label: "Nature", icon: "tf_world:grass" },
    { id: "matter", label: "Matter: every substance", icon: "tf_matter:copper" },
    { id: "functional", label: "Tools for finding out", icon: "tf_matter:heat_gun" },
    { id: "tools", label: "Tools", icon: "tf_matter:fire_drill" },
    { id: "items", label: "Items", icon: "" },
]
const tabs = computed(() => TABS.flatMap((t) => {
    const inTab = (catalog.value ?? []).filter((i) => i.category === t.id)
    if (!inTab.length) return []
    const face = inTab.find((i) => i.item === t.icon) ?? inTab[0]!
    return [{ ...t, face: face }]
}))

watch(creative, async (on) => {
    if (!on || catalog.value) return
    try {
        catalog.value = await bridge.send("creative.list", {})
        for (const item of catalog.value) store.items[item.item] = { description: item.description, icon: item.icon }
    } catch (err) {
        carry.problem.value = err instanceof Error ? err.message : String(err)
    }
}, { immediate: true })

function choose(id: string) {
    tab.value = id
    query.value = ""
}

function close() {
    carry.drop()
    bridge.send("screen.close", {})
}

function keydown(e: KeyboardEvent) {
    const typing = document.activeElement === search.value
    if (e.key === "Escape") { e.preventDefault(); close(); return }
    if (typing) return
    if (e.key === "e" || e.key === "E") { e.preventDefault(); close(); return }
    if (/^[1-9]$/.test(e.key)) { e.preventDefault(); carry.key(Number(e.key), over.value); return }
    // any other letter starts a search, as Minecraft's creative tabs do
    if (creative.value && tab.value !== "inventory" && e.key.length === 1 && /\S/.test(e.key) && !e.ctrlKey && !e.metaKey) {
        query.value += e.key
        nextTick(() => search.value?.focus())
        e.preventDefault()
    }
}
function move(e: MouseEvent) { carry.cursor.value = { x: e.clientX, y: e.clientY } }
function up(e: MouseEvent) { carry.release(e, document.elementFromPoint(e.clientX, e.clientY)) }

// craftresult is where a craft lands before the hand places it; it isn't shown,
// so anything left there goes back to the inventory rather than vanish
function returnCraftResult() {
    const stack = store.state.inventory?.craftresult?.stacks[0]
    if (stack) bridge.send("inventory.move", { from: { list: "craftresult", index: 0 }, to: { list: "main" }, count: stack.count }).catch(console.error)
}

onMounted(() => {
    window.addEventListener("keydown", keydown, true) // capture: before an input or button can keep it
    window.addEventListener("mousemove", move)
    window.addEventListener("mouseup", up)
    returnCraftResult()
})
onBeforeUnmount(() => {
    window.removeEventListener("keydown", keydown, true)
    window.removeEventListener("mousemove", move)
    window.removeEventListener("mouseup", up)
    returnCraftResult()
})

const tooltip = computed(() => {
    if (carry.busy.value || !over.value) return null
    if ("item" in over.value) return over.value.item.description
    const s = over.value.stack
    return s ? store.items[s.item]?.description ?? s.item : null
})
</script>

<template>
    <div class="screen" @mousedown.self="carry.away()" @contextmenu.prevent>
        <div class="frame" :class="{ creative: creative }" data-panel>
            <nav v-if="creative" class="tabs top" aria-label="Categories">
                <button v-for="t in tabs" :key="t.id" class="tab" :class="{ on: tab === t.id && !query }" :title="t.label" @click="choose(t.id)">
                    <img v-if="t.face.icon" :src="t.face.icon" :alt="t.label" draggable="false">
                </button>
                <button class="tab" :class="{ on: !!query }" title="Search everything" @click="search?.focus()">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M2 0h3v1h-3zM1 1h1v1h-1zM5 1h1v1h-1zM0 2h1v3h-1zM6 2h1v3h-1zM1 5h1v1h-1zM5 5h1v1h-1zM2 6h3v1h-3zM6 6h1v1h-1zM7 7h1v1h-1zM8 8h1v1h-1z" />
                    </svg>
                </button>
            </nav>

            <div class="body">
                <header>
                    <h2>{{ !creative ? "Inventory" : tab === "inventory" ? "Inventory" : query ? "Search" : tabs.find((t) => t.id === tab)?.label ?? "" }}</h2>
                    <button class="close" title="Close (E or Esc)" @click="close">✕</button>
                </header>
                <label v-if="creative && tab !== 'inventory'" class="search">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M2 0h3v1h-3zM1 1h1v1h-1zM5 1h1v1h-1zM0 2h1v3h-1zM6 2h1v3h-1zM1 5h1v1h-1zM5 5h1v1h-1zM2 6h3v1h-3zM6 6h1v1h-1zM7 7h1v1h-1zM8 8h1v1h-1z" />
                    </svg>
                    <input ref="search" v-model="query" type="search" placeholder="Search everything" aria-label="Search every item">
                </label>
                <CreativePanel v-if="creative && tab !== 'inventory'" :carry="carry" :items="catalog ?? []" :category="tab" :query="query"
                    @hover="over = $event" />
                <SurvivalPanel v-else :carry="carry" @hover="over = $event" />
                <p v-if="carry.problem.value" class="problem">{{ carry.problem.value }}</p>
            </div>

            <nav v-if="creative" class="tabs bottom" aria-label="Your inventory">
                <button class="tab" :class="{ on: tab === 'inventory' }" title="Your inventory" @click="choose('inventory')">
                    <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                        <path fill="currentColor" d="M0 1h9v1h-9zM0 2h1v6h-1zM8 2h1v6h-1zM0 8h9v1h-9zM1 4h7v1h-7zM4 3h1v3h-1z" />
                    </svg>
                </button>
            </nav>
        </div>

        <div v-if="carry.carried.value" class="hand" :style="{ left: `${carry.cursor.value.x}px`, top: `${carry.cursor.value.y}px` }">
            <ItemSlot :stack="carry.carried.value" />
        </div>
        <div v-else-if="tooltip" class="tooltip" :style="{ left: `${carry.cursor.value.x + 16}px`, top: `${carry.cursor.value.y + 16}px` }">
            {{ tooltip }}
        </div>
    </div>
</template>

<style scoped>
.screen {
    --u: clamp(2px, round(down, 100vh / 300, 1px), 3px);
    position: fixed;
    inset: 0;
    display: grid;
    place-items: center;
    background: rgb(4 8 12 / 0.45);
}

/* one size, whatever the tab: nine slots across, the catalogue's six rows plus the hotbar */
.frame { position: relative; display: flex; flex-direction: column; }
.body {
    width: calc(var(--u) * (18 * 9 + 8 + 12));
    min-height: calc(var(--u) * 180);
    display: flex;
    flex-direction: column;
    gap: calc(var(--u) * 3);
    padding: calc(var(--u) * 4) calc(var(--u) * 6) calc(var(--u) * 6);
    border: var(--b) solid var(--edge);
    background: var(--stone-1);
    box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35);
}
header { display: flex; align-items: center; justify-content: space-between; }
h2 { margin: 0; font: 400 calc(var(--u) * 5) var(--font-display); text-transform: uppercase; color: var(--ink); }
.close { border: 0; background: none; padding: 2px 6px; color: var(--ink-faint); font-size: 14px; }
.close:hover { color: var(--ink); }

/* tabs sit on the frame's edge, the chosen one joined to it */
.tabs { display: flex; gap: var(--u); padding: 0 calc(var(--u) * 3); }
.tab { width: calc(var(--u) * 22); height: calc(var(--u) * 22); display: grid; place-items: center; padding: 0;
    border: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised); color: var(--ink-dim); }
.tabs.top .tab { border-bottom: 0; margin-bottom: calc(var(--b) * -1); }
.tabs.bottom .tab { border-top: 0; margin-top: calc(var(--b) * -1); }
.tab.on { background: var(--stone-1); color: var(--accent-hi); z-index: 1; }
.tab:hover { color: var(--ink); }
.tab img { width: calc(var(--u) * 16); height: calc(var(--u) * 16); image-rendering: pixelated; }
.tab svg { width: calc(var(--u) * 9); height: calc(var(--u) * 9); }

.search { display: flex; align-items: center; gap: 8px; padding: 3px 8px; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); color: var(--accent-hi); }
.search:focus-within { border-color: var(--accent); }
.search svg { width: 12px; height: 12px; flex: none; }
.search input { flex: 1; min-width: 0; padding: 4px 0; border: 0; background: transparent; outline: none; font-size: 12px; }
.problem { margin: 0; color: var(--danger); font-size: 11px; }

.hand { position: fixed; pointer-events: none; transform: translate(-50%, -50%); filter: drop-shadow(var(--b) var(--b) 0 rgb(0 0 0 / 0.5)); }
.tooltip { position: fixed; pointer-events: none; padding: 6px 10px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.96);
    box-shadow: var(--raised); font: 12px var(--font-display); white-space: pre-line; }
</style>
