<script setup lang="ts">
import type { Stack } from "../bridge/index.ts"
import ItemSlot from "./ItemSlot.vue"

defineProps<{ stacks: (Stack | null)[]; selected: number }>()
</script>

<template>
    <div class="hotbar">
        <div v-for="(stack, i) in stacks" :key="i" class="cell" :class="{ selected: i === selected }">
            <ItemSlot :stack="stack" />
        </div>
    </div>
</template>

<style scoped>
.hotbar {
    display: flex;
    padding: var(--u);
    background: rgb(20 20 20 / 0.55);
    border: var(--u) solid #1b1b1b;
    box-shadow: inset 0 0 0 var(--u) #5a5a5a;
}

.cell {
    position: relative;
    display: grid;
    place-items: center;
    width: calc(var(--u) * 20);
    height: calc(var(--u) * 20);
    box-shadow: inset 0 0 0 var(--u) rgb(139 139 139 / 0.35);
}

.cell.selected::after {
    content: "";
    position: absolute;
    inset: calc(var(--u) * -2);
    border: calc(var(--u) * 2) solid #f2f2f2;
    box-shadow: 0 0 0 var(--u) #1b1b1b, inset 0 0 0 var(--u) #1b1b1b;
    z-index: 1;
}
</style>
