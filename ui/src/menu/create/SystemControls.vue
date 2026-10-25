<script setup lang="ts">
import { Button, Section, Slider } from "../../kit"
import { addMoon, addPlanet, body, removeBody, STAR } from "../../system/model"
import StarPicker from "./StarPicker.vue"
import type { WorldDraftHandle } from "./draft"

defineProps<{ draft: WorldDraftHandle; page: "overview" | "star" }>()
</script>

<template>
    <div class="system-controls">
        <template v-if="page === 'overview'">
            <h2>One system, many worlds</h2>
            <p class="copy">Choose the star and arrange its planets and moons. Select any body above the viewer to examine it, then use Body to change its causes.</p>
            <div class="status-grid">
                <div><strong>Playable now</strong><span>Earth's land, climate, star light, tilt and year are saved in its world code.</span></div>
                <div><strong>Preview only</strong><span>The other bodies and the system's history are visual design previews; they are not saved into the playable world yet.</span></div>
            </div>
            <Section title="Arrange bodies" hint="system preview only">
                <p class="copy">Select a planet or moon above the viewer. The system view lets you inspect their orbits together.</p>
                <div class="body-actions">
                    <Button size="sm" @click="addPlanet(draft.system)">+ Planet</Button>
                    <Button v-if="draft.system.selected !== STAR && body(draft.system, draft.system.selected).parent === null"
                        size="sm" @click="addMoon(draft.system, draft.system.selected)">+ Moon</Button>
                    <Button v-if="draft.system.selected !== STAR && !body(draft.system, draft.system.selected).playable"
                        size="sm" variant="danger" @click="removeBody(draft.system, draft.system.selected)">Remove selected</Button>
                </div>
            </Section>
        </template>
        <template v-else-if="page === 'star'">
            <Section title="Star type" hint="changes the playable world's light and warmth">
                <StarPicker v-model="draft.config.star" />
            </Section>
            <Section title="System history" hint="preview only — not saved with the world">
                <Slider v-model="draft.system.age" label="Age" :min="0.5" :max="12" :step="0.1" :format="(v) => `${v.toFixed(1)} billion years`" />
                <Slider v-model="draft.system.activity" label="Stellar wind" :min="0" :max="1" :step="0.01" :format="(v) => `${Math.round(v * 100)}%`" />
            </Section>
            <p class="copy">Age gives every body time to cool and change. The star's wind can strip air from worlds without a strong magnetic field.</p>
        </template>
    </div>
</template>

<style scoped>
.system-controls { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.copy { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.status-grid { display: grid; gap: 8px; }
.status-grid > div { display: grid; gap: 6px; padding: 12px; border-left: 3px solid var(--accent); background: var(--stone-2); }
.status-grid > div + div { border-left-color: var(--ink-faint); }
.status-grid strong { font: 10px var(--font-display); text-transform: uppercase; }
.status-grid span { color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.body-actions { display: flex; flex-wrap: wrap; gap: 7px; }
</style>
