<script setup lang="ts">
import { onMounted, provide, ref, watch } from "vue"
import { BridgeError } from "../../bridge"
import { useBridge } from "../../bridge/use"
import { Button, Notice, Panel, Tabs } from "../../kit"
import { encodeWorldCode } from "../../worldgen/config"
import { worldName } from "../../worldgen/names"
import { SYSTEM_PREVIEW, SystemPreview } from "../../system/preview"
import type { Screen } from "../screens"
import BodyTree from "../../system/BodyTree.vue"
import DynamicsTab from "./DynamicsTab.vue"
import { WorldDraft } from "./draft"
import GameTab from "./GameTab.vue"
import Inspector from "./Inspector.vue"
import { LOOKS, Looks } from "./looks"
import { worldSettings } from "./rules"
import { TABS, type TabId } from "./tabs"
import Viewer from "./Viewer.vue"

const emit = defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()

const draft = WorldDraft()
// every body's card from the engine, for the tabs and the viewer alike
const system = SystemPreview({ bridge: bridge })
provide(SYSTEM_PREVIEW, system)
watch(() => [draft.system, draft.config.star], () => system.request(draft.system, draft.config.star), { deep: true, immediate: true })
const looks = Looks({ bridge: bridge, draft: draft, cards: system })
provide(LOOKS, looks)
const tab = ref<TabId>("start")
const problem = ref<string | null>(null)
const busy = ref(false)

// Know the existing names, so the suggested one never collides.
onMounted(async () => {
    draft.taken = (await bridge.send("worlds.list", {})).map((w) => w.name)
    if (draft.taken.some((n) => n.toLowerCase() === draft.name.toLowerCase())) draft.name = worldName(draft.taken)
})

async function create() {
    problem.value = null
    busy.value = true
    try {
        const world = await bridge.send("worlds.create", {
            name: draft.name,
            code: encodeWorldCode(draft.config),
            settings: worldSettings(draft.mode, draft.rules, draft.config),
        })
        await bridge.send("play", { path: world.path })
    } catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
        busy.value = false
    }
}
</script>

<template>
    <Panel title="Create world" class="create">
        <template #actions>
            <Button size="sm" @click="emit('go', 'worlds')">Back</Button>
        </template>

        <Tabs v-model="tab" :tabs="TABS" class="steps" />

        <!-- World: the system editor — bodies, the view, the chosen body -->
        <div v-if="tab === 'world'" class="editor">
            <BodyTree class="tree" :system="draft.system" :star="draft.config.star" :image="looks.image" />
            <Viewer class="view" :draft="draft" />
            <Inspector class="inspector" :draft="draft" />
        </div>

        <!-- Start and Rules: settings beside the world they make -->
        <div v-else class="layout">
            <div class="content">
                <GameTab v-if="tab === 'start'" :draft="draft" />
                <DynamicsTab v-else :draft="draft" />
            </div>
            <Viewer class="viewer" :draft="draft" />
        </div>

        <template #footer>
            <Notice class="notice">
                <strong>Built from this:</strong> Earth's land, climate, sun, rivers and lakes — the preview is the world.
                <strong>Not saved yet:</strong> the rest of the system; only Earth is playable.
            </Notice>
            <span v-if="problem" class="problem">{{ problem }}</span>
            <Button variant="primary" size="lg" :disabled="busy || !draft.name.trim()" @click="create">Create & play »</Button>
        </template>
    </Panel>
</template>

<style scoped>
.create { width: min(1200px, 96vw); height: min(780px, 92vh); }
.create :deep(.body) { overflow: hidden; }

.create :deep(.body) { display: flex; flex-direction: column; gap: 16px; }
.steps { flex: none; width: 420px; }

.layout { flex: 1; min-height: 0; display: grid; grid-template-columns: 440px minmax(0, 1fr); }
.content { display: flex; flex-direction: column; gap: 22px; overflow-y: auto; padding-right: 8px; margin-right: 28px; }
/* Settings on the left, the world on the right: a clear seam between them. */
.viewer, .view, .inspector { padding-left: 22px; border-left: var(--b) solid var(--edge); box-shadow: inset var(--b) 0 0 var(--bevel-hi); }

.editor { flex: 1; min-height: 0; display: grid; grid-template-columns: 210px minmax(0, 1fr) 360px; gap: 0 22px; }
.tree { min-height: 0; }
.view { min-width: 0; }
.inspector { min-height: 0; }

.notice { margin-right: auto; max-width: 520px; }
.problem { color: var(--danger); }
</style>
