<script setup lang="ts">
import { ref } from "vue"
import type { BridgeHandle, Mode } from "./bridge"

const props = defineProps<{ bridge: BridgeHandle }>()
const mode = ref<Mode | null>(null)

props.bridge.on((message) => {
    if (message.kind === "mode") mode.value = message.mode
})
</script>

<template>
    <div class="probe">
        <h1>Terra Firma</h1>
        <p>{{ bridge.connected ? `engine: ${mode ?? "waiting"}` : "not inside the engine" }}</p>
        <input placeholder="type here" @focus="bridge.send({ kind: 'capture', keyboard: true })"
            @blur="bridge.send({ kind: 'capture', keyboard: false })">
    </div>
</template>

<style>
html, body { margin: 0; background: transparent; font-family: system-ui, sans-serif; }
.probe {
    position: fixed; top: 24px; right: 24px; padding: 20px 24px;
    color: #f4f1e8; background: rgb(20 24 18 / 0.7);
    backdrop-filter: blur(8px); border-radius: 12px;
    border: 1px solid rgb(255 255 255 / 0.12);
}
.probe h1 { margin: 0 0 8px; font-size: 20px; letter-spacing: 0.04em; }
.probe p { margin: 0 0 12px; opacity: 0.8; }
.probe input { padding: 6px 10px; border-radius: 6px; border: 1px solid #555; background: #111; color: inherit; }
</style>
