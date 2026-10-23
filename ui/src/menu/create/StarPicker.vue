<script setup lang="ts">
import { computed } from "vue"
import { IconTile } from "../../kit"
import BodyIcon from "../../worldgen/BodyIcon.vue"
import { STARS, star, type StarId } from "../../worldgen/config"

/** The star types as a row of swatches; the chosen one is described below. */
const model = defineModel<StarId>({ required: true })
const chosen = computed(() => star(model.value))
</script>

<template>
    <div class="stars">
        <div class="row">
            <IconTile v-for="s in STARS" :key="s.id" :label="s.label" :on="model === s.id" :size="40" @click="model = s.id">
                <BodyIcon kind="star" :color="s.color" />
            </IconTile>
        </div>
        <p><strong :style="{ color: `rgb(${chosen.color.join(' ')})` }">{{ chosen.label }}</strong> — {{ chosen.note }}</p>
    </div>
</template>

<style scoped>
.stars { display: flex; flex-direction: column; gap: 8px; }
.row { display: flex; gap: 6px; }
p { margin: 0; font-size: 12px; color: var(--ink-dim); }
strong { font: 400 12px var(--font-display); text-transform: uppercase; }
</style>
