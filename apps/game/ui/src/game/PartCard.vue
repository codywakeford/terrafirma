<script setup lang="ts">
import type { JointId, PartCard } from "../bridge"
import { useBridge } from "../bridge/use.ts"
import Button from "../kit/Button.vue"

/**
 * The part selected (sprints 53–56): what it's of, what it's joined to and how (each joint's kind set here), its
 * sockets; opened on a face, a socket made there (a hinge: a wheel's carried onto it); detach, delete.
 */
// menu: opened on a face (right-clicked): sockets can be made there
defineProps<{ part: PartCard; menu: boolean }>()
const bridge = useBridge()
const JOINTS: readonly JointId[] = ["fixed", "hinge"]
const isJoint = (v: string): v is JointId | "none" => v === "none" || JOINTS.some((k) => k === v)
const relink = (other: string, event: Event) => {
    const picked = event.target instanceof HTMLSelectElement ? event.target.value : ""
    if (!isJoint(picked)) throw new Error(`PartCard: no joint "${picked}"`)
    void bridge.send("tools.ask", { action: "relink", other: other, joint: picked })
}
</script>

<template>
    <div class="card">
        <h3>Part · {{ part.material || "empty" }}</h3>
        <p v-if="!part.joints.length" class="hint">Joined to nothing.</p>
        <ul v-else>
            <li v-for="j in part.joints" :key="j.other">
                <span>to {{ j.material || "a part" }}</span>
                <select :value="j.joint" @change="relink(j.other, $event)">
                    <option v-for="k in JOINTS" :key="k" :value="k">{{ k }}</option>
                    <option value="none">remove</option>
                </select>
            </li>
        </ul>
        <ul v-if="part.sockets.length">
            <li v-for="k in part.sockets" :key="k.id">
                <span>{{ k.joint }} socket</span>
                <button class="x" @click="bridge.send('tools.ask', { action: 'unsocket', socket: k.id })">remove</button>
            </li>
        </ul>
        <div v-if="menu" class="row">
            <Button size="sm" variant="primary" @click="bridge.send('tools.ask', { action: 'socket', joint: 'hinge' })">Add hinge</Button>
            <Button size="sm" @click="bridge.send('tools.ask', { action: 'socket', joint: 'fixed' })">Add weld</Button>
        </div>
        <p v-if="menu" class="hint">Here, on this face: carry a part onto it to join it so.</p>
        <div class="row">
            <Button size="sm" @click="bridge.send('tools.ask', { action: 'detach' })">Detach</Button>
            <Button size="sm" variant="danger" @click="bridge.send('tools.ask', { action: 'delete' })">Delete</Button>
        </div>
    </div>
</template>

<style scoped>
.card {
    display: flex; flex-direction: column; gap: 8px; width: 260px; padding: 12px;
    border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.94); box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}
h3 { margin: 0; font: 12px var(--font-display); text-transform: uppercase; color: var(--ink); }
ul { display: flex; flex-direction: column; gap: 4px; margin: 0; padding: 0; list-style: none; }
li { display: flex; align-items: center; justify-content: space-between; gap: 8px; font-size: 13px; color: var(--ink-dim); }
select { color: var(--ink); background: var(--stone-0); border: var(--b) solid var(--edge); font: inherit; }
.row { display: flex; gap: 6px; }
.x { padding: 1px 6px; color: var(--ink-dim); background: var(--stone-2); border: var(--b) solid var(--edge); font: 11px inherit; }
.hint { margin: 0; font-size: 12px; color: var(--ink-faint); }
</style>
