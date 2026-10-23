<script setup lang="ts">
/** A wiki section's three columns: the list, the view, the facts. */
defineProps<{ wide?: boolean }>()
</script>

<template>
    <div class="layout" :class="{ wide: wide }">
        <nav class="list"><slot name="list" /></nav>
        <main class="view"><slot name="view" /></main>
        <aside v-if="!wide" class="facts"><slot name="facts" /></aside>
    </div>
</template>

<style scoped>
.layout { display: grid; grid-template-columns: 220px minmax(0, 1fr) 380px; gap: 0 22px; height: 100%; min-height: 0; }
.layout.wide { grid-template-columns: 220px minmax(0, 1fr); }
.list { overflow-y: auto; min-height: 0; padding-right: 4px; }
.view, .facts { display: flex; flex-direction: column; gap: 12px; min-height: 0; padding-left: 22px; border-left: var(--b) solid var(--edge); }
.facts { overflow-y: auto; }
.list :deep(section + section) { margin-top: 14px; }
.list :deep(h3) { margin: 0 0 6px; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.list :deep(.item) { display: grid; grid-template-columns: 52px 1fr; align-items: center; width: 100%; margin-bottom: 3px; padding: 6px 8px;
    border: var(--b) solid transparent; background: none; color: var(--ink-dim); font-size: 12px; text-align: left; }
.list :deep(.item.plain) { grid-template-columns: 1fr; }
.list :deep(.item:hover) { color: var(--ink); background: var(--stone-1); }
.list :deep(.item.on) { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.list :deep(.formula) { font: 12px var(--font-display); color: var(--ink); }
.facts :deep(h4) { margin: 0; font: 400 18px var(--font-display); text-transform: uppercase; }
.facts :deep(.dim) { color: var(--ink-faint); font-size: 11px; margin: 0; }
.facts :deep(.copy) { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.facts :deep(dl) { display: grid; gap: 4px; margin: 0; }
.facts :deep(dl div) { display: flex; justify-content: space-between; gap: 10px; font-size: 11px; }
.facts :deep(dt) { color: var(--ink-dim); }
.facts :deep(dd) { margin: 0; font-family: var(--font-display); font-size: 11px; text-align: right; }
.facts :deep(h5) { margin: 6px 0 0; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.facts :deep(.link), .view :deep(.link) { border: 0; background: none; padding: 0; text-align: left; color: var(--accent-hi); font-size: 12px; cursor: pointer; }
.facts :deep(.links) { display: flex; flex-wrap: wrap; gap: 4px 12px; }
</style>
