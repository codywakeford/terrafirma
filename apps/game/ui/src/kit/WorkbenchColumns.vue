<script setup lang="ts">
import { computed, useSlots } from "vue"

/**
 * The same list → content → facts hierarchy on every workspace screen.
 * The facts column shows when there's a facts slot; the columns own the
 * type inside them, so every screen's facts read alike.
 */
const props = withDefaults(defineProps<{ listWidth?: number; factsWidth?: number }>(), { listWidth: 240, factsWidth: 380 })
const slots = useSlots()
const columns = computed(() => `${props.listWidth}px minmax(0, 1fr)${slots.facts ? ` ${props.factsWidth}px` : ""}`)
</script>

<template>
    <div class="workbench-columns" :style="{ gridTemplateColumns: columns }">
        <nav class="workbench-list"><slot name="list" /></nav>
        <main class="workbench-content"><slot name="content" /></main>
        <aside v-if="$slots.facts" class="workbench-facts"><slot name="facts" /></aside>
    </div>
</template>

<style scoped>
.workbench-columns { display: grid; width: 100%; height: 100%; min-width: 0; min-height: 0; }
.workbench-list, .workbench-content, .workbench-facts { min-width: 0; min-height: 0; overflow-y: auto; }
.workbench-list { padding: 18px 10px; border-right: var(--b) solid var(--edge); background: var(--stone-1); }
.workbench-content, .workbench-facts { display: flex; flex-direction: column; gap: 12px; padding: 18px 22px; background: var(--stone-0); }
.workbench-facts { border-left: var(--b) solid var(--edge); }
.workbench-facts :deep(h4) { margin: 0; font: 400 18px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.workbench-facts :deep(h5) { margin: 6px 0 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.workbench-columns :deep(.dim) { color: var(--ink-faint); font-size: 11px; margin: 0; }
.workbench-columns :deep(.copy) { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.workbench-facts :deep(dl) { display: grid; gap: 4px; margin: 0; }
.workbench-facts :deep(dl div) { display: flex; justify-content: space-between; gap: 10px; font-size: 11px; }
.workbench-facts :deep(dt) { color: var(--ink-dim); }
.workbench-facts :deep(dd) { margin: 0; font-family: var(--font-display); font-size: 11px; text-align: right; }
.workbench-columns :deep(.link) { border: 0; background: none; padding: 0; text-align: left; color: var(--accent-hi); font-size: 12px; cursor: pointer; }
.workbench-facts :deep(.links) { display: flex; flex-wrap: wrap; gap: 4px 12px; }
@media (max-width: 900px) {
    .workbench-columns { grid-template-columns: 200px minmax(0, 1fr) !important; }
    .workbench-facts { display: none; }
}
</style>
