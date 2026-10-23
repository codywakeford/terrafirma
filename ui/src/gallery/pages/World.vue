<script setup lang="ts">
import { WorldDraft } from "../../menu/create/draft"
import GameTab from "../../menu/create/GameTab.vue"
import DynamicsTab from "../../menu/create/DynamicsTab.vue"
import Viewer from "../../menu/create/Viewer.vue"
import WorldTab from "../../menu/create/WorldTab.vue"
import Specimen from "../Specimen.vue"

const draft = WorldDraft()
</script>

<template>
    <div class="grid">
        <Specimen name="Viewer" note="interactive 3D system · body list · globe ↔ map ↔ system">
            <div class="box"><Viewer :draft="draft" /></div>
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
.box { width: 100%; height: 680px; }
.col { display: flex; flex-direction: column; gap: 22px; width: 100%; }
</style>
