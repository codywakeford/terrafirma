<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { BridgeError, type SlotRef, type Stack } from "../../bridge"
import { useBridge } from "../../bridge/use"
import { Panel } from "../../kit"
import ItemSlot from "../ItemSlot.vue"
import { useStore } from "../store"
import { click, outside, type Held, type Outgoing, type Step } from "./hand"
import Slot from "./Slot.vue"
import SlotGrid from "./SlotGrid.vue"

/**
 * The player's inventory: crafting, storage and hotbar. The engine owns every
 * stack; this screen only points at them and sends intents (see hand.ts).
 */
const bridge = useBridge()
const store = useStore()

const held = ref<Held | null>(null)
const hovered = ref<Stack | null>(null)
const cursor = ref({ x: 0, y: 0 })
const problem = ref<string | null>(null)

const lists = computed(() => store.state.inventory)
const row = computed(() => store.state.hotbar?.size ?? null) // the hotbar is main's first row
const main = computed(() => lists.value?.main ?? null)
const craft = computed(() => lists.value?.craft ?? null)

function live(slot: SlotRef) {
    return lists.value?.[slot.list]?.stacks[slot.index]?.count ?? 0
}

const heldStack = computed<Stack | null>(() => {
    const h = held.value
    const stack = h ? lists.value?.[h.from.list]?.stacks[h.from.index] : null
    if (!h || !stack) return null
    return { ...stack, count: h.count ?? stack.count }
})

// If the server empties the slot the hand points at (it was moved, crafted or
// dropped elsewhere), the hand has nothing to hold.
watch(heldStack, (stack) => {
    if (held.value && !stack) held.value = null
})

function send(intent: Outgoing) {
    switch (intent.kind) {
        case "inventory.move": return bridge.send(intent.kind, intent.request)
        case "inventory.craft": return bridge.send(intent.kind, intent.request)
        case "inventory.drop": return bridge.send(intent.kind, intent.request)
    }
}

async function apply(step: Step) {
    problem.value = null
    switch (step.kind) {
        case "hold": held.value = step.held; break
        case "release": held.value = null; break
        case "none": break
        case "send":
            held.value = step.held
            try {
                await send(step.intent)
            } catch (err) {
                if (!(err instanceof BridgeError && err.refused)) console.error(err)
                problem.value = err instanceof Error ? err.message : String(err)
                held.value = null
            }
    }
}

function press(slot: SlotRef, stack: Stack | null, button: "left" | "right", shift: boolean) {
    apply(click(held.value, { slot: slot, stack: stack, button: button, shift: shift }, live))
}

function track(e: MouseEvent) {
    cursor.value = { x: e.clientX, y: e.clientY }
}

// craftresult is where a craft lands before the hand places it; it is not
// shown, so anything left there goes back to the inventory rather than vanish.
function returnCraftResult() {
    const stack = lists.value?.craftresult?.stacks[0]
    if (!stack) return
    bridge.send("inventory.move", { from: { list: "craftresult", index: 0 }, to: { list: "main" }, count: stack.count }).catch((err) => console.error(err))
}

onMounted(() => {
    window.addEventListener("mousemove", track)
    returnCraftResult()
})
onBeforeUnmount(() => {
    window.removeEventListener("mousemove", track)
    returnCraftResult()
})

const tooltip = computed(() => (hovered.value && !held.value ? store.items[hovered.value.item]?.description ?? hovered.value.item : null))
</script>

<template>
    <div class="screen" @mousedown.self="apply(outside(held, live))" @contextmenu.prevent>
        <Panel title="Inventory" class="inventory">
            <div v-if="main && craft && row" class="layout">
                <section class="crafting">
                    <span class="label">Crafting</span>
                    <div class="craft-row">
                        <SlotGrid list="craft" :stacks="craft.stacks" :columns="craft.width" :held="held"
                            @press="press" @hover="hovered = $event" />
                        <svg class="arrow" viewBox="0 0 11 7" shape-rendering="crispEdges" aria-hidden="true">
                            <path fill="currentColor" d="M0 3h7v-3h1v1h1v1h1v1h1v1h-1v1h-1v1h-1v1h-1v-3h-7z" />
                        </svg>
                        <Slot result :stack="lists?.craftpreview?.stacks[0] ?? null"
                            @press="(b, s) => press({ list: 'craftpreview', index: 0 }, lists?.craftpreview?.stacks[0] ?? null, b, s)"
                            @hover="hovered = $event ? lists?.craftpreview?.stacks[0] ?? null : null" />
                    </div>
                </section>

                <section>
                    <span class="label">Storage</span>
                    <SlotGrid list="main" :stacks="main.stacks" :columns="row" :start="row" :held="held"
                        @press="press" @hover="hovered = $event" />
                </section>

                <section>
                    <span class="label">Hotbar</span>
                    <SlotGrid list="main" :stacks="main.stacks" :columns="row" :start="0" :end="row" :held="held"
                        @press="press" @hover="hovered = $event" />
                </section>
            </div>

            <template #footer>
                <span v-if="problem" class="problem">{{ problem }}</span>
                <span class="hint">right-click: half · shift-click: quick move · click outside: drop · esc / i: close</span>
            </template>
        </Panel>

        <div v-if="heldStack" class="hand" :style="{ left: `${cursor.x}px`, top: `${cursor.y}px` }">
            <ItemSlot :stack="heldStack" />
        </div>
        <div v-else-if="tooltip" class="tooltip" :style="{ left: `${cursor.x + 16}px`, top: `${cursor.y + 16}px` }">
            {{ tooltip }}
        </div>
    </div>
</template>

<style scoped>
.screen {
    --u: 3px; /* inventory pixel scale: 48px icons */
    position: fixed;
    inset: 0;
    display: grid;
    place-items: center;
    background: rgb(4 8 12 / 0.45);
}

.inventory { width: max-content; max-width: 95vw; }
.layout { display: flex; flex-direction: column; gap: calc(var(--u) * 6); }
section { display: flex; flex-direction: column; gap: 8px; }

.label { font: 11px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.craft-row { display: flex; align-items: center; justify-content: center; gap: calc(var(--u) * 8); padding: calc(var(--u) * 2) 0; }
.arrow { width: calc(var(--u) * 11); height: calc(var(--u) * 7); color: var(--stone-4); }

/* Footer text wraps to the slots' width instead of widening the panel. */
.problem, .hint { contain: inline-size; flex: 1; min-width: 0; }
.problem { color: var(--danger); }
.hint { font-size: 11px; color: var(--ink-faint); text-align: right; }

.hand {
    position: fixed;
    pointer-events: none;
    transform: translate(-50%, -50%);
    filter: drop-shadow(var(--b) var(--b) 0 rgb(0 0 0 / 0.5));
}

.tooltip {
    position: fixed;
    pointer-events: none;
    padding: 6px 10px;
    border: var(--b) solid var(--edge);
    background: rgb(12 14 16 / 0.96);
    box-shadow: var(--raised);
    font: 12px var(--font-display);
    white-space: nowrap;
}
</style>
