<script setup lang="ts">
import type { FlightNear } from "../../bridge"

/** Over the blocks: how high, how fast, and how far below the ceiling, where the planet takes over. */
const props = defineProps<{ flight: FlightNear }>()
const metres = (m: number) => (Math.abs(m) >= 10000 ? `${(m / 1000).toFixed(1)} km` : `${Math.round(m)} m`)
</script>

<template>
    <div class="strip">
        <span><b>{{ metres(props.flight.altitude) }}</b> up</span>
        <span><b>{{ Math.round(props.flight.speed) }}</b> m/s</span>
        <span>{{ props.flight.climb >= 0 ? "▲" : "▼" }} {{ Math.abs(Math.round(props.flight.climb)) }} m/s</span>
        <span class="ceiling">
            <i :style="{ width: `${Math.max(0, Math.min(1, props.flight.altitude / props.flight.ceiling)) * 100}%` }" />
        </span>
        <span class="dim">{{ props.flight.resting ? "landed · Shift to unstrap" : `ceiling ${metres(props.flight.ceiling)}` }}</span>
    </div>
</template>

<style scoped>
.strip { position: absolute; top: 14px; left: 50%; transform: translateX(-50%); display: flex; align-items: center; gap: 14px;
    padding: 6px 12px; border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.86); box-shadow: var(--raised);
    font: 12px var(--font-display); color: var(--ink); white-space: nowrap; }
b { color: var(--accent-hi); font-weight: 400; }
.ceiling { width: 90px; height: 6px; border: 1px solid var(--edge); background: var(--stone-0); }
.ceiling i { display: block; height: 100%; background: var(--accent); }
.dim { color: var(--ink-dim); font-size: 11px; }
</style>
