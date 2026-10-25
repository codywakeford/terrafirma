<script setup lang="ts">
import { computed, onMounted, shallowRef } from "vue"
import type { LabScenario } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Button, ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit/index.ts"
import { useLab } from "../lab/flow.ts"
import { LAB_DEMOS } from "./demos.ts"
import { useWiki } from "./nav.ts"
import SandboxStage from "./SandboxStage.vue"

/**
 * The Lab: how matter behaves, shown in sandboxes (heat moving, melting,
 * boiling, fire), and, from the menu, the scenarios that need a whole small
 * world (water flowing, falling blocks, the sun's path), each entered as
 * its own world.
 */
const wiki = useWiki()
const bridge = useBridge()
const lab = useLab()
const worlds = shallowRef<LabScenario[]>([])
onMounted(async () => {
    if (wiki.place === "menu") worlds.value = await bridge.send("lab.list", {}) // the scenario worlds are entered from the menu
})

const groups = computed(() => {
    const out = new Map<string, typeof LAB_DEMOS>()
    const term = wiki.search.value.trim().toLowerCase()
    for (const d of LAB_DEMOS) {
        if (!`${d.title} ${d.note} ${d.group}`.toLowerCase().includes(term)) continue
        out.set(d.group, [...(out.get(d.group) ?? []), d])
    }
    return [...out.entries()]
})
const visibleWorlds = computed(() => worlds.value.filter((w) =>
    `${w.title} ${w.description} ${w.mod}`.toLowerCase().includes(wiki.search.value.trim().toLowerCase())))
const chosen = computed(() => wiki.state.chosen.lab ?? LAB_DEMOS[0]!.id)
const demo = computed(() => LAB_DEMOS.find((d) => d.id === chosen.value) ?? null)
const world = computed(() => worlds.value.find((w) => `world:${w.id}` === chosen.value) ?? null)

async function enter(id: string) {
    await lab.open()
    lab.choose(id)
}
</script>

<template>
    <WorkbenchColumns>
        <template #list>
            <ListGroup v-for="[group, demos] in groups" :key="group" :title="group">
                <ListItem v-for="d in demos" :key="d.id" :on="chosen === d.id" @click="wiki.go('lab', d.id)">{{ d.title }}</ListItem>
            </ListGroup>
            <ListGroup v-if="visibleWorlds.length" title="Worlds">
                <ListItem v-for="w in visibleWorlds" :key="w.id" :on="chosen === `world:${w.id}`" @click="wiki.go('lab', `world:${w.id}`)">
                    {{ w.title }}
                </ListItem>
            </ListGroup>
            <ListEmpty v-if="wiki.search.value && !groups.length && !visibleWorlds.length">No matches.</ListEmpty>
        </template>

        <template #content>
            <SandboxStage v-if="demo" :spec="demo.spec" :speed="demo.speed" />
            <div v-else-if="world" class="world">
                <p class="copy">{{ world.description }}</p>
                <p class="dim">This one needs a small world of its own: water flowing, blocks falling, the sky turning.</p>
                <Button @click="enter(world.id)">Enter the scenario</Button>
            </div>
        </template>

        <template #facts>
            <template v-if="demo">
                <h4>{{ demo.title }}</h4>
                <p class="copy">{{ demo.note }}</p>
                <p class="dim">The game's own physics runs it: every block is a mixture of real substances; heat moves by each one's conductivity, gases mix and rise, liquids fall. Hover a block to read it; Heat shows temperatures.</p>
            </template>
            <template v-else-if="world">
                <h4>{{ world.title }}</h4>
                <p class="dim">from {{ world.mod }}</p>
            </template>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
.world { display: flex; flex-direction: column; gap: 12px; align-items: flex-start; }
</style>
