<script setup lang="ts">
import { computed } from "vue"
import Reactor from "../matter/Reactor.vue"
import { PRESETS } from "../matter/presets"
import { useTable, useWiki } from "./nav"
import WikiLayout from "./WikiLayout.vue"

/** The Reactor: fill a vessel, set what it sits in, and watch the engine run the mixture. Set-ups to start from on the left. */
const wiki = useWiki()
const table = useTable()
const preset = computed(() => wiki.state.chosen.reactor ?? PRESETS[0]!.id)
</script>

<template>
    <WikiLayout wide>
        <template #list>
            <section>
                <h3>Set-ups</h3>
                <button v-for="p in PRESETS" :key="p.id" class="item plain" :class="{ on: preset === p.id }" @click="wiki.go('reactor', p.id)">
                    {{ p.label }}
                </button>
            </section>
        </template>
        <template #view>
            <Reactor :substances="table.substances" :reactions="table.reactions" :preset="preset" class="reactor" />
        </template>
    </WikiLayout>
</template>

<style scoped>
.reactor { flex: 1; min-height: 0; }
</style>
