<script setup lang="ts">
import { Button, Panel } from "../kit"
import SettingsPanel from "../settings/SettingsPanel.vue"
import type { Screen } from "./screens"

defineEmits<{ go: [screen: Screen] }>()
</script>

<template>
    <Panel title="Settings" class="settings">
        <template #actions>
            <Button size="sm" @click="$emit('go', 'home')">Done</Button>
        </template>
        <SettingsPanel />
    </Panel>
</template>

<style scoped>
.settings { width: min(1200px, 96vw); height: min(760px, 92vh); }
.settings :deep(.body) { padding: 0; overflow: hidden; }
</style>
