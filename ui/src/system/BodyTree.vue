<script setup lang="ts">
import { star } from "../worldgen/config"
import BodyIcon from "../worldgen/BodyIcon.vue"
import type { StarId } from "../worldgen/config"
import { addMoon, addPlanet, body as bodyOf, moonsOf, planets, removeBody, STAR, type SystemHandle } from "./model"
import { useSystemPreview } from "./preview"

/**
 * Every body in the system, as a tree: the star, its planets, their moons.
 * Choosing one is the main thing you do in the World step, so it has a
 * column of its own. `image` gives each body's look (the playable world's
 * terrain, the others' sketches).
 */
const props = defineProps<{ system: SystemHandle; star: StarId; image: (id: string) => ImageData | null }>()
const cards = useSystemPreview()

function pick(id: string) {
    props.system.selected = id
}
function kindOf(id: string) {
    return cards.card(id)?.kind ?? "…"
}
</script>

<template>
    <nav class="tree" aria-label="Bodies in this system">
        <button class="row star" :class="{ on: system.selected === STAR }" @click="pick(STAR)">
            <BodyIcon kind="star" :color="star(props.star).color" />
            <span class="name">{{ star(props.star).label }}</span>
            <span class="kind">star</span>
        </button>
        <template v-for="planet in planets(system)" :key="planet.id">
            <button class="row" :class="{ on: system.selected === planet.id }" @click="pick(planet.id)">
                <BodyIcon kind="planet" :image="image(planet.id)" />
                <span class="name">{{ planet.name }}<em v-if="planet.playable"> · play</em></span>
                <span class="kind">{{ kindOf(planet.id) }}</span>
            </button>
            <button v-for="moon in moonsOf(system, planet.id)" :key="moon.id" class="row moon"
                :class="{ on: system.selected === moon.id }" @click="pick(moon.id)">
                <BodyIcon kind="planet" :image="image(moon.id)" />
                <span class="name">{{ moon.name }}</span>
                <span class="kind">{{ kindOf(moon.id) }}</span>
            </button>
        </template>

        <div class="actions">
            <button @click="addPlanet(system)">+ planet</button>
            <button v-if="system.selected !== STAR && bodyOf(system, system.selected).parent === null"
                @click="addMoon(system, system.selected)">+ moon</button>
            <button v-if="system.selected !== STAR && !bodyOf(system, system.selected).playable" class="danger"
                @click="removeBody(system, system.selected)">remove</button>
        </div>
    </nav>
</template>

<style scoped>
.tree { display: flex; flex-direction: column; gap: 3px; min-height: 0; overflow-y: auto; padding-right: 4px; }
.row {
    display: grid;
    grid-template-columns: 32px 1fr;
    grid-template-rows: auto auto;
    column-gap: 8px;
    align-items: center;
    padding: 4px 6px;
    border: var(--b) solid transparent;
    background: none;
    text-align: left;
    color: var(--ink-dim);
}
.row :deep(canvas) { grid-row: 1 / 3; }
.row:hover { color: var(--ink); background: var(--stone-1); }
.row.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.row.moon { margin-left: 18px; }
.name { font: 12px var(--font-display); text-transform: uppercase; }
.name em { font-style: normal; color: var(--accent-hi); }
.kind { font-size: 10px; color: var(--ink-faint); }
.actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 8px; padding-top: 8px; border-top: var(--b) solid var(--edge); }
.actions button { border: 0; background: none; padding: 0; font: 11px var(--font-display); text-transform: uppercase; color: var(--accent-hi); }
.actions .danger { color: var(--danger); }
</style>
