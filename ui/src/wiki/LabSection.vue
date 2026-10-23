<script setup lang="ts">
import { computed, onMounted, shallowRef } from "vue"
import type { LabScenario } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button } from "../kit"
import { useLab } from "../lab/flow"
import { LAB_DEMOS } from "./demos"
import { useWiki } from "./nav"
import SandboxStage from "./SandboxStage.vue"
import WikiLayout from "./WikiLayout.vue"

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
    for (const d of LAB_DEMOS) out.set(d.group, [...(out.get(d.group) ?? []), d])
    return [...out.entries()]
})
const chosen = computed(() => wiki.state.chosen.lab ?? LAB_DEMOS[0]!.id)
const demo = computed(() => LAB_DEMOS.find((d) => d.id === chosen.value) ?? null)
const world = computed(() => worlds.value.find((w) => `world:${w.id}` === chosen.value) ?? null)

async function enter(id: string) {
    await lab.open()
    lab.choose(id)
}
</script>

<template>
    <WikiLayout>
        <template #list>
            <section v-for="[group, demos] in groups" :key="group">
                <h3>{{ group }}</h3>
                <button v-for="d in demos" :key="d.id" class="item plain" :class="{ on: chosen === d.id }" @click="wiki.go('lab', d.id)">{{ d.title }}</button>
            </section>
            <section v-if="worlds.length">
                <h3>Worlds</h3>
                <button v-for="w in worlds" :key="w.id" class="item plain" :class="{ on: chosen === `world:${w.id}` }" @click="wiki.go('lab', `world:${w.id}`)">
                    {{ w.title }}
                </button>
            </section>
        </template>

        <template #view>
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
    </WikiLayout>
</template>

<style scoped>
.world { display: flex; flex-direction: column; gap: 12px; align-items: flex-start; }
.copy { margin: 0; color: var(--ink-dim); font-size: 13px; line-height: 1.5; }
.dim { margin: 0; color: var(--ink-faint); font-size: 11px; }
</style>
