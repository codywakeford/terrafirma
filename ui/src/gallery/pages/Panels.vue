<script setup lang="ts">
import { Button, Notice, Panel } from "../../kit"
import Specimen from "../Specimen.vue"
</script>

<template>
    <div class="grid">
        <Specimen name="Panel" note="title, actions, body, footer">
            <Panel title="Worlds" class="demo">
                <template #actions><Button size="sm">Back</Button></template>
                <p>Panel body. Scrolls when it overflows.</p>
                <template #footer><Button variant="primary">Play »</Button></template>
            </Panel>
        </Specimen>
        <Specimen name="Panel" note="body only">
            <Panel class="demo"><Notice>A bare panel with no title bar.</Notice></Panel>
        </Specimen>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(420px, 1fr)); gap: 28px; }
.demo { width: 100%; height: 260px; }
p { margin: 0; color: var(--ink-dim); }
</style>
