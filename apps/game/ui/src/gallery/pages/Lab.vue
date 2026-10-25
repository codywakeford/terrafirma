<script setup lang="ts">
import { onMounted } from "vue"
import { useStore } from "../../game/store.ts"
import { useLab } from "../../lab/flow.ts"
import LabView from "../../lab/LabView.vue"

/**
 * The Lab page against the mock engine: pick a scenario and it "loads" its
 * world (mode → game, tf_lab's state arrives); its controls answer; Stop
 * returns to the list. The live view is sky here — in the game, it's the world.
 */
const store = useStore()
const lab = useLab()

onMounted(() => {
    store.mode = "menu"
    lab.open()
})
</script>

<template>
    <div class="frame">
        <div class="sky" />
        <LabView v-if="lab.state.open" />
        <button v-else class="reopen" @click="lab.open()">open the Lab</button>
    </div>
</template>

<style scoped>
.frame { position: relative; height: 100%; min-height: 640px; overflow: hidden; border: var(--b) solid var(--edge); transform: translateZ(0); }
.sky { position: absolute; inset: 0; background: linear-gradient(180deg, #8ab6f0, #c4dcf5 60%, #5c6268 60%, #4a5056); }
.reopen { position: absolute; left: 16px; top: 16px; border: 0; background: none; color: var(--accent-hi); font-size: 12px; }
</style>
