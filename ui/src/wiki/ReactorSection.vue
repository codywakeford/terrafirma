<script setup lang="ts">
import { ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit"
import { computed } from "vue"
import Reactor from "../matter/Reactor.vue"
import { PRESETS } from "../matter/presets"
import { useTable, useWiki } from "./nav"

/** The Reactor: fill a vessel, set what it sits in, and watch the engine run the mixture. Set-ups to start from on the left. */
const wiki = useWiki()
const table = useTable()
const preset = computed(() => wiki.state.chosen.reactor ?? PRESETS[0]!.id)
const visiblePresets = computed(() => PRESETS.filter((p) => p.label.toLowerCase().includes(wiki.search.value.trim().toLowerCase())))
</script>

<template>
    <WorkbenchColumns>
        <template #list>
            <ListGroup title="Set-ups">
                <ListItem v-for="p in visiblePresets" :key="p.id" :on="preset === p.id" @click="wiki.go('reactor', p.id)">
                    {{ p.label }}
                </ListItem>
                <ListEmpty v-if="wiki.search.value && !visiblePresets.length">No matches.</ListEmpty>
            </ListGroup>
        </template>
        <template #content>
            <Reactor :substances="table.substances" :reactions="table.reactions" :preset="preset" class="reactor" />
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
.reactor { flex: 1; min-height: 0; }
</style>
