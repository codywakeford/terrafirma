<script setup lang="ts">
import { provide, ref, watch } from "vue"
import { useBridge } from "../../bridge/use"
import { WorldDraft } from "../../menu/create/draft"
import BodyNavigator from "../../menu/create/BodyNavigator.vue"
import GameTab from "../../menu/create/GameTab.vue"
import DynamicsTab from "../../menu/create/DynamicsTab.vue"
import Viewer from "../../menu/create/Viewer.vue"
import WorldTab from "../../menu/create/WorldTab.vue"
import { LOOKS, Looks } from "../../menu/create/looks"
import { SYSTEM_PREVIEW, SystemPreview } from "../../system/preview"
import Specimen from "../Specimen.vue"

const draft = WorldDraft()
const view = ref<"globe" | "map" | "system">("globe")
const bridge = useBridge()
const system = SystemPreview({ bridge })
provide(SYSTEM_PREVIEW, system)
watch(() => [draft.system, draft.config.star], () => system.request(draft.system, draft.config.star), { deep: true, immediate: true })
const looks = Looks({ bridge, draft, cards: system })
provide(LOOKS, looks)
</script>

<template>
    <div class="grid">
        <Specimen name="Viewer" note="interactive 3D system · body list · globe ↔ map ↔ system">
            <div class="box"><BodyNavigator v-model:view="view" :system="draft.system" :star="draft.config.star" /><Viewer v-model:view="view" :draft="draft" /></div>
        </Specimen>
        <div class="col">
            <Specimen name="Game tab"><div class="col"><GameTab :draft="draft" /></div></Specimen>
            <Specimen name="World tab"><div class="col"><WorldTab :draft="draft" /></div></Specimen>
            <Specimen name="Dynamics tab"><div class="col"><DynamicsTab :draft="draft" /></div></Specimen>
        </div>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: minmax(0, 1fr) 440px; gap: 28px; align-items: start; }
.box { display: flex; flex-direction: column; width: 100%; height: 680px; }
.box :deep(.viewer) { flex: 1; }
.col { display: flex; flex-direction: column; gap: 22px; width: 100%; }
</style>
