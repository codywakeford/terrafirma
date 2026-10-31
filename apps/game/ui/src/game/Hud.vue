<script setup lang="ts">
import { computed, onMounted } from "vue"
import Corner from "../account/Corner.vue"
import { useBridge } from "../bridge/use.ts"
import Crosshair from "./Crosshair.vue"
import FlightStrip from "./flight/FlightStrip.vue"
import SpaceStrip from "./flight/SpaceStrip.vue"
import Hotbar from "./Hotbar.vue"
import LabStrip from "../lab/LabStrip.vue"
import ProbeCard from "./ProbeCard.vue"
import StatBar from "./StatBar.vue"
import ActionRail from "./ActionRail.vue"
import ShapeBar from "./ShapeBar.vue"
import ToolRail from "./ToolRail.vue"
import TravelScreen from "./TravelScreen.vue"
import { useStore } from "./store.ts"
import MapWidget from "./widgets/MapWidget.vue"
import { loadWidgetSettings } from "./widgets/config.ts"
import WorldControlsWidget from "./widgets/WorldControlsWidget.vue"

const store = useStore()
const bridge = useBridge()
onMounted(() => { void loadWidgetSettings(bridge) })
const hud = computed(() => store.state.hud)
const player = computed(() => store.state.player)
const flight = computed(() => store.ui.flight)
// pause keeps the world's HUD up, the mouse free over it; another screen covers it
const paused = computed(() => store.screen === "pause")
const covered = computed(() => store.screen !== null && !paused.value)
const tools = computed(() => store.state.tools)
const library = computed(() => store.state.library)
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
        <FlightStrip v-if="flight?.regime === 'near' && !covered" :flight="flight" />
        <SpaceStrip v-if="flight?.regime === 'space' && !covered" :flight="flight" />
        <LabStrip />
        <ProbeCard v-if="!covered" />
        <ToolRail v-if="tools?.out && !covered" :tools="tools" :free="tools.cursor || paused" />
        <ActionRail v-if="tools?.out && library && !covered" :tools="tools" :library="library" :free="tools.cursor || paused" />
        <div v-if="!covered" class="corner-icons">
            <Corner />
            <WorldControlsWidget />
        </div>
        <div class="bottom">
            <div v-if="player" class="stats">
                <StatBar v-if="hud.health" kind="heart" :value="player.hp" :max="player.hpMax" />
                <StatBar v-if="hud.breath && player.breath < player.breathMax" kind="bubble"
                    :value="player.breath" :max="player.breathMax" reverse />
            </div>
            <ShapeBar v-if="tools?.out && !covered" :tools="tools" :free="tools.cursor || paused" />
            <Hotbar v-if="hud.hotbar && hotbar" :stacks="hotbar.stacks" :selected="hotbar.selected"
                :free="!!tools?.out && tools.cursor || paused" />
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

.corner-icons > * { pointer-events: auto; }

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
