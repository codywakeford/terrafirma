<script setup lang="ts">
import { computed } from "vue"
import type { CreativeItem, SlotRef, Stack } from "../../bridge"
import { useStore } from "../store"
import type { CarryHandle } from "./carry"
import SlotGrid from "./SlotGrid.vue"

/**
 * The creative catalogue: every item by category, searchable, with the
 * hotbar below to drag into and a bin to throw things away. The search box
 * is always there: typing searches everything.
 */
const props = defineProps<{ carry: CarryHandle; items: CreativeItem[]; category: string; query: string }>()
const emit = defineEmits<{ hover: [over: { slot: SlotRef; stack: Stack | null } | { item: CreativeItem } | null] }>()
const store = useStore()
const row = computed(() => store.state.hotbar?.size ?? 9)
const main = computed(() => props.carry.lists.value?.main ?? null)

const shown = computed(() => {
    const q = props.query.trim().toLowerCase()
    return props.items.filter((item) => (q
        ? item.description.toLowerCase().includes(q) || item.item.toLowerCase().includes(q)
        : item.category === props.category))
})

function down(item: CreativeItem, e: MouseEvent) {
    if (e.button !== 0 && e.button !== 2) return
    props.carry.catalogue(item, e.button === 2 ? "right" : "left", e.shiftKey, e)
}
</script>

<template>
    <div class="creative">
        <div class="catalogue" @mousedown.self="carry.held.value && carry.bin()">
            <button v-for="item in shown" :key="item.item" class="cell" :aria-label="item.description"
                @mousedown.prevent="down(item, $event)" @contextmenu.prevent
                @mouseenter="emit('hover', { item: item })" @mouseleave="emit('hover', null)">
                <img v-if="item.icon" :src="item.icon" :alt="item.description" draggable="false">
                <span v-else>{{ item.description.slice(0, 2) }}</span>
            </button>
            <p v-if="!shown.length" class="empty">{{ query ? "Nothing matches." : "Nothing here yet." }}</p>
        </div>
        <div v-if="main" class="bottom">
            <SlotGrid list="main" :stacks="main.stacks" :columns="row" :start="0" :end="row" :held="carry.held.value"
                @press="carry.slot" @hover="emit('hover', $event)" />
            <button class="bin" data-bin title="Destroy what you carry" @mousedown.prevent="carry.bin()">
                <svg viewBox="0 0 9 9" shape-rendering="crispEdges" aria-hidden="true">
                    <path fill="currentColor" d="M1 1h7v1h-7zM3 0h3v1h-3zM2 2h5v7h-5zM3 3v5h1v-5zM5 3v5h1v-5z" />
                </svg>
            </button>
        </div>
    </div>
</template>

<style scoped>
.creative { display: flex; flex-direction: column; gap: calc(var(--u) * 3); height: 100%; }
/* nine across, six down, scrolling: the same size whatever the tab */
.catalogue { display: grid; grid-template-columns: repeat(9, calc(var(--u) * 18)); grid-auto-rows: calc(var(--u) * 18); gap: var(--u);
    height: calc(var(--u) * 18 * 6 + var(--u) * 5); overflow-y: auto; overflow-x: hidden; align-content: start; padding-right: 2px; }
.cell { display: grid; place-items: center; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); }
.cell:hover { background: #1b2730; box-shadow: inset 0 0 0 var(--b) var(--accent-lo), var(--sunk); }
.cell img { width: calc(var(--u) * 16); height: calc(var(--u) * 16); image-rendering: pixelated; }
.cell span { font: 12px var(--font-display); }
.empty { grid-column: 1 / -1; color: var(--ink-faint); padding: 16px; margin: 0; }
.bottom { display: flex; align-items: center; gap: calc(var(--u) * 3); }
.bin { width: calc(var(--u) * 18); height: calc(var(--u) * 18); display: grid; place-items: center; padding: 0;
    border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--raised); color: var(--ink-dim); }
.bin:hover { color: var(--danger); }
.bin svg { width: calc(var(--u) * 9); height: calc(var(--u) * 9); }
</style>
