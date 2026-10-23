<script setup lang="ts">
import { defineAsyncComponent, ref } from "vue"
import CreateWorld from "./create/CreateWorld.vue"
import Home from "./Home.vue"
import Settings from "./Settings.vue"
import type { Screen } from "./screens"
import WorldSelect from "./WorldSelect.vue"

// the wiki brings three.js: loaded when first opened
const WikiScreen = defineAsyncComponent(() => import("../wiki/WikiScreen.vue"))

defineProps<{ error: string | null }>()
defineEmits<{ dismiss: [] }>()

const screen = ref<Screen>("home")
</script>

<template>
    <main class="menu">
        <div v-if="error" class="error" role="alert">
            <span><strong>Session ended:</strong> {{ error }}</span>
            <button @click="$emit('dismiss')">✕</button>
        </div>

        <Transition name="screen" mode="out-in">
            <Home v-if="screen === 'home'" @go="screen = $event" />
            <WorldSelect v-else-if="screen === 'worlds'" @go="screen = $event" />
            <CreateWorld v-else-if="screen === 'create'" @go="screen = $event" />
            <Settings v-else-if="screen === 'settings'" @go="screen = $event" />
            <WikiScreen v-else-if="screen === 'wiki'" place="menu" @close="screen = 'home'" />
        </Transition>
    </main>
</template>

<style scoped>
.menu {
    position: relative;
    height: 100%;
    display: grid;
    place-items: center;
    overflow: hidden;
    background: transparent;
}
.menu::before {
    content: "";
    position: absolute;
    inset: 0;
    pointer-events: none;
    background:
        radial-gradient(ellipse 27% 48% at 50% 52%, rgb(3 8 15 / 0.68), transparent 100%),
        linear-gradient(90deg, rgb(3 8 15 / 0.18), transparent 35%, transparent 70%, rgb(3 8 15 / 0.08));
}
.menu > :deep(section), .menu > :deep(.home) { position: relative; z-index: 1; }

.error {
    position: absolute;
    top: 20px;
    z-index: 2;
    display: flex;
    gap: 16px;
    align-items: center;
    padding: 10px 14px;
    border: var(--b) solid var(--edge);
    background: var(--danger-lo);
    box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35);
}
.error button { border: 0; background: none; padding: 0 4px; }

.screen-enter-active, .screen-leave-active { transition: opacity 140ms steps(4), transform 140ms steps(4); }
.screen-enter-from { opacity: 0; transform: translateY(12px); }
.screen-leave-to { opacity: 0; transform: translateY(-12px); }
</style>
