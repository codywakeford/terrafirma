<script setup lang="ts">
import { moonsOf, planets, STAR, type SystemHandle } from "../../system/model"
import { useLooks } from "./looks"
import { star, type StarId } from "../../worldgen/config"
import BodyIcon from "../../worldgen/BodyIcon.vue"

const props = defineProps<{ system: SystemHandle; star: StarId }>()
const view = defineModel<"globe" | "map" | "system">("view", { required: true })
const looks = useLooks()
function pick(id: string) {
    props.system.selected = id
    view.value = "globe"
}
</script>

<template>
    <nav class="navigator" aria-label="Choose a celestial body">
        <div class="bodies">
            <div class="body-block">
                <button class="body-button" :class="{ on: system.selected === STAR && view !== 'system' }"
                    :aria-current="system.selected === STAR ? 'true' : undefined" @click="pick(STAR)">
                    <BodyIcon kind="star" :color="star(props.star).color" />
                    <span>{{ star(props.star).label }}</span>
                </button>
            </div>
            <div v-for="planet in planets(system)" :key="planet.id" class="body-block">
                <button class="body-button" :class="{ on: system.selected === planet.id && view !== 'system' }"
                    :aria-current="system.selected === planet.id ? 'true' : undefined" @click="pick(planet.id)">
                    <BodyIcon kind="planet" :image="looks.image(planet.id)" />
                    <span>{{ planet.name }}</span>
                    <small v-if="planet.playable">play</small>
                </button>
                <div v-if="moonsOf(system, planet.id).length" class="moon-row" :aria-label="`Moons of ${planet.name}`">
                    <button v-for="moon in moonsOf(system, planet.id)" :key="moon.id" :title="moon.name" :aria-label="moon.name"
                        :class="{ on: system.selected === moon.id && view !== 'system' }" @click="pick(moon.id)">
                        <BodyIcon kind="moon" />
                    </button>
                </div>
            </div>
        </div>
        <button class="system-button" :class="{ on: view === 'system' }" aria-label="View solar system" title="View solar system"
            @click="view = 'system'">
            <span class="system-symbol">✣</span><span>System</span>
        </button>
    </nav>
</template>

<style scoped>
.navigator { display: flex; align-items: stretch; gap: 8px; min-width: 0; min-height: 88px; padding: 5px 12px; background: var(--stone-0); }
.bodies { display: flex; align-items: start; gap: 5px; flex: 1; min-width: 0; overflow-x: auto; scrollbar-width: thin; }
.body-block { flex: none; width: 86px; display: flex; flex-direction: column; align-items: center; gap: 2px; }
.body-button, .system-button { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 2px; width: 100%; min-width: 0; padding: 2px; border: var(--b) solid transparent; background: none; color: var(--ink-dim); cursor: pointer; }
.body-button { min-height: 53px; }
.body-button:hover, .system-button:hover, .moon-row button:hover { background: var(--stone-2); color: var(--ink); }
.body-button.on, .system-button.on, .moon-row button.on { border-color: var(--edge); background: var(--accent-lo); box-shadow: var(--sunk); color: var(--ink); }
.body-button span:not(.icon), .system-button span:last-child { display: block; max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font: 9px var(--font-display); text-transform: uppercase; }
.body-button small { color: var(--accent-hi); font: 7px var(--font-display); text-transform: uppercase; }
.body-button :deep(.icon) { width: 24px; height: 24px; }
.moon-row { display: flex; justify-content: center; gap: 2px; max-width: 86px; overflow-x: auto; }
.moon-row button { display: grid; place-items: center; flex: none; width: 19px; height: 19px; padding: 0; border: 1px solid transparent; background: none; cursor: pointer; }
.moon-row button :deep(.icon) { width: 12px; height: 12px; }
.system-button { flex: none; width: 64px; align-self: start; min-height: 53px; }
.system-symbol { color: var(--accent-hi); font: 22px var(--font-display); line-height: 1; }
</style>
