<script setup lang="ts">
import { computed } from "vue"
import { useStore } from "../game/store"
import { useLab } from "./flow"

/** Stepped into a Lab scenario, top left of the HUD: which one, the time speed, and the key back to the Lab view. */
const store = useStore()
const lab = useLab()
const here = computed(() => (store.ui.lab ? lab.find(store.ui.lab.id) : null))
</script>

<template>
    <div v-if="store.ui.lab" class="strip">
        <span class="tag">» Lab</span>
        <span class="title">{{ here ? here.title : store.ui.lab.id }}</span>
        <span class="speed">{{ store.ui.lab.speed === 0 ? "paused" : `×${store.ui.lab.speed}` }}</span>
        <span class="key">L · Lab view</span>
    </div>
</template>

<style scoped>
.strip {
    position: absolute;
    top: 14px;
    left: 14px;
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 12px;
    border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.82);
    box-shadow: var(--raised);
    font: 12px var(--font-display);
    text-transform: uppercase;
    color: var(--ink);
}
.tag { color: var(--accent); }
.speed, .key { color: var(--ink-faint); }
</style>
