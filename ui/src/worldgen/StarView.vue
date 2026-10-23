<script setup lang="ts">
import { computed } from "vue"
import { star, type StarId } from "./config"

/** The world's sun: a glowing disc in its own colour, sized and lit by type. */
const props = defineProps<{ star: StarId }>()

const info = computed(() => star(props.star))
const rgb = computed(() => info.value.color.join(" "))
// hotter stars read a little larger and brighter
const scale = computed(() => 0.62 + (info.value.warmth + 0.55) * 0.18)
</script>

<template>
    <div class="star-view" :style="{ '--star': rgb, '--scale': scale }">
        <div class="corona" />
        <div class="disc" />
        <div class="caption">
            <strong>{{ info.label }}</strong>
            <span>{{ info.note }}</span>
        </div>
    </div>
</template>

<style scoped>
.star-view { position: relative; display: grid; place-items: center; overflow: hidden; }

.disc, .corona {
    position: absolute;
    left: 50%;
    top: 46%;
    width: calc(56% * var(--scale));
    aspect-ratio: 1;
    border-radius: 50%; /* the one round thing: a star */
    transform: translate(-50%, -50%);
}

.disc {
    background: radial-gradient(circle at 42% 40%, #fff 0%, rgb(var(--star)) 38%, rgb(var(--star) / 0.85) 70%, rgb(var(--star) / 0.55) 100%);
    box-shadow: 0 0 40px 8px rgb(var(--star) / 0.55), 0 0 120px 30px rgb(var(--star) / 0.25);
}

.corona {
    width: calc(80% * var(--scale));
    background: radial-gradient(circle, rgb(var(--star) / 0.28) 0%, transparent 65%);
    animation: pulse 6s steps(6) infinite;
}

.caption {
    position: absolute;
    bottom: 6%;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    text-align: center;
}
.caption strong { font: 400 14px var(--font-display); text-transform: uppercase; color: rgb(var(--star)); }
.caption span { font-size: 12px; color: var(--ink-dim); }

@keyframes pulse { 50% { transform: translate(-50%, -50%) scale(1.06); opacity: 0.8; } }
</style>
