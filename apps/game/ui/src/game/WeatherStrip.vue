<script setup lang="ts">
import { computed } from "vue"
import { useStore } from "./store"

/** Top right of the HUD: the air where the player stands — temperature, what's falling, the wind, a barometer, and any event under way. */
const store = useStore()
const COMPASS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
const reading = computed(() => {
    const w = store.ui.weather
    if (!w) return null
    const speed = Math.hypot(w.wind.x, w.wind.z)
    // the wind is named for where it blows from
    const from = (Math.atan2(-w.wind.x, -w.wind.z) * 180 / Math.PI + 360) % 360
    const falling = w.precipitation < 0.05 ? null : `${w.snow ? "snow" : "rain"} ${w.precipitation.toFixed(1)} mm/h`
    return {
        temperature: `${Math.round(w.temperature)}°C`,
        falling: falling,
        wind: speed < 0.5 ? "calm" : `${COMPASS[Math.round(from / 45) % 8]} ${Math.round(speed)} m/s`,
        pressure: `${Math.round(w.pressure)} hPa`,
        events: w.events ?? [],
    }
})
</script>

<template>
    <div v-if="reading" class="strip">
        <span class="temp">{{ reading.temperature }}</span>
        <span v-if="reading.falling">{{ reading.falling }}</span>
        <span>{{ reading.wind }}</span>
        <span class="faint">{{ reading.pressure }}</span>
        <span v-for="e in reading.events" :key="e" class="event">{{ e }}</span>
    </div>
</template>

<style scoped>
.strip {
    position: absolute;
    top: 14px;
    right: 14px;
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
.temp { color: var(--accent); }
.faint { color: var(--ink-faint); }
.event { color: #f0b040; }
</style>
