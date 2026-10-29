<script setup lang="ts">
import { computed, onMounted } from "vue"
import { useBridge } from "../bridge/use.ts"
import Crosshair from "./Crosshair.vue"
import FlightStrip from "./flight/FlightStrip.vue"
import SpaceStrip from "./flight/SpaceStrip.vue"
import Hotbar from "./Hotbar.vue"
import LabStrip from "../lab/LabStrip.vue"
import WeatherStrip from "./WeatherStrip.vue"
import ProbeCard from "./ProbeCard.vue"
import StatBar from "./StatBar.vue"
import TravelScreen from "./TravelScreen.vue"
import { useStore } from "./store.ts"
import MapWidget from "./widgets/MapWidget.vue"
import { loadWidgetSettings } from "./widgets/config.ts"

const store = useStore()
const bridge = useBridge()
onMounted(() => { void loadWidgetSettings(bridge) })
const hud = computed(() => store.state.hud)
const player = computed(() => store.state.player)
const flight = computed(() => store.ui.flight)
const hotbar = computed(() => {
    const bar = store.state.hotbar
    const main = store.state.inventory?.main
    if (!bar || !main) return null
    return { stacks: main.stacks.slice(0, bar.size), selected: bar.selected }
})
</script>

<template>
    <!-- the Lab browser shows the world, not the player's HUD -->
    <div v-if="hud?.visible && store.screen !== 'lab'" class="hud">
        <Crosshair v-if="hud.crosshair" />
        <FlightStrip v-if="flight?.regime === 'near' && !store.screen" :flight="flight" />
        <SpaceStrip v-if="flight?.regime === 'space' && !store.screen" :flight="flight" />
        <LabStrip />
        <!-- a panel over the world (the wiki, the inventory) hides the strip; it covered the panel's Close -->
        <WeatherStrip v-if="!store.screen" />
        <ProbeCard v-if="!store.screen" />
        <div class="bottom">
            <div v-if="player" class="stats">
                <StatBar v-if="hud.health" kind="heart" :value="player.hp" :max="player.hpMax" />
                <StatBar v-if="hud.breath && player.breath < player.breathMax" kind="bubble"
                    :value="player.breath" :max="player.breathMax" reverse />
            </div>
            <Hotbar v-if="hud.hotbar && hotbar" :stacks="hotbar.stacks" :selected="hotbar.selected" />
        </div>
    </div>
    <MapWidget />
    <TravelScreen />
</template>

<style scoped>
.hud {
    /* One GUI pixel, like Minecraft's GUI scale: whole device pixels only, so art stays crisp. */
    --u: clamp(2px, round(down, 100vh / 240, 1px), 4px);
    position: fixed;
    inset: 0;
    pointer-events: none;
}

.bottom {
    position: absolute;
    left: 50%;
    bottom: 0;
    transform: translateX(-50%);
    display: flex;
    flex-direction: column;
    gap: var(--u);
}

.stats {
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    padding: 0 var(--u);
}
</style>
