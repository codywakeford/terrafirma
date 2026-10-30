<script setup lang="ts">
import { computed, ref } from "vue"
import { BRUSH_IDS, TOOL_IDS, type BrushId, type JointId, type StateSections, type ToolId } from "../bridge"
import { useBridge } from "../bridge/use.ts"
import IconButton from "../kit/IconButton.vue"
import PixelIcon from "./PixelIcon.vue"
import { ACTION_ICONS, BRUSH_ICONS, INK, TOOL_ICONS } from "./toolIcons.ts"

/**
 * The tool, low on the left while the tools are out (sprints 53–55): Select, or a brush (add, cut, paint, pull a
 * face, a new part: welded or hinged; Wheel's a hinged cylinder part) and the shape it draws (one button, its
 * list beside it). A shape: press on a face, drag its footprint, let go and pull it out, click. R moves through the
 * shapes; with the mouse free (an object held, or paused) they're clicked.
 */
// free: the mouse is (an object held, or paused)
const props = defineProps<{ tools: Extract<StateSections["tools"], { out: true }>; free: boolean }>()
const bridge = useBridge()
const pick = (id: ToolId) => {
    if (props.free) void bridge.send("tools.pick", { tool: id })
}
// a brush is drawn with; Select's the other thing a click does
const brush = (id: BrushId) => {
    if (!props.free) return
    void bridge.send("tools.brush", { brush: id })
    if (props.tools.mode.click !== "draw") void bridge.send("tools.mode", { click: "draw" })
}
const select = () => {
    if (props.free) void bridge.send("tools.mode", props.tools.mode.click === "select" ? { click: "draw" } : { click: "select" })
}
const shapes = ref(false)
const shape = (id: ToolId) => {
    pick(id)
    shapes.value = false
}
const joint = (j: JointId) => {
    if (props.free) void bridge.send("tools.brush", { brush: "part", joint: j })
}
// a wheel: a cylinder, a part of its own, hinged about the face it's drawn out of
const wheel = () => {
    if (!props.free) return
    void bridge.send("tools.pick", { tool: "cylinder" })
    void bridge.send("tools.brush", { brush: "part", joint: "hinge" })
}

const SIXTEENTHS = 16
const size = computed(() =>
    props.tools.size % SIXTEENTHS === 0 ? `${props.tools.size / SIXTEENTHS}m` : `${props.tools.size}/16`)
</script>

<template>
    <nav class="tool-rail rail" :class="{ free: free }" aria-label="Tool">
        <IconButton :label="ACTION_ICONS.select.label" :on="tools.mode.click === 'select'" side="right"
            :tabindex="free ? 0 : -1" @click="select">
            <PixelIcon :rows="ACTION_ICONS.select.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton v-for="id in BRUSH_IDS" :key="id" :label="BRUSH_ICONS[id].label"
            :on="tools.mode.click === 'draw' && tools.brush === id" side="right" :tabindex="free ? 0 : -1" @click="brush(id)">
            <PixelIcon :rows="BRUSH_ICONS[id].rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <div v-if="tools.brush === 'part'" class="joints">
            <button :class="{ on: tools.partJoint === 'fixed' }" @click="joint('fixed')">Welded</button>
            <button :class="{ on: tools.partJoint === 'hinge' }" @click="joint('hinge')">Hinged</button>
        </div>
        <IconButton :label="ACTION_ICONS.wheel.label" side="right" :tabindex="free ? 0 : -1"
            :on="tools.brush === 'part' && tools.partJoint === 'hinge' && tools.tool === 'cylinder'" @click="wheel">
            <PixelIcon :rows="ACTION_ICONS.wheel.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <div class="gap" />
        <!-- the shape, one button: its list opens beside it -->
        <div class="shape">
            <IconButton :label="`Shape: ${TOOL_ICONS[tools.tool].label}`" :on="shapes" side="right"
                :tabindex="free ? 0 : -1" @click="shapes = !shapes">
                <PixelIcon :rows="TOOL_ICONS[tools.tool].rows" :palette="INK" style="--u: 2px" />
                <span class="size">{{ size }}</span>
            </IconButton>
            <div v-if="shapes" class="shapes">
                <IconButton v-for="id in TOOL_IDS" :key="id" :label="TOOL_ICONS[id].label" :on="tools.tool === id"
                    side="none" @click="shape(id)">
                    <PixelIcon :rows="TOOL_ICONS[id].rows" :palette="INK" style="--u: 2px" />
                </IconButton>
            </div>
        </div>
    </nav>
</template>

<style scoped>
/* low on the left, a third of the screen at most, wrapping into columns to the right */
.tool-rail {
    position: absolute; left: 20px; bottom: 110px;
    display: flex; flex-direction: column; flex-wrap: wrap; align-content: flex-start;
    gap: 10px; max-height: 33vh;
}
/* the mouse is free only holding an object, or paused: elsewhere the rail is shown, not touched */
.tool-rail.free { pointer-events: auto; }
.gap { height: 6px; width: 48px; }
.shape { position: relative; }
.shapes { position: absolute; left: calc(100% + 10px); bottom: 0; display: flex; gap: 6px; padding: 6px;
    border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.94); }
.joints { display: flex; flex-direction: column; gap: 2px; width: 48px; }
.joints button {
    padding: 3px 0; font: 9px var(--font-display); text-transform: uppercase; color: var(--ink-faint);
    background: var(--stone-1); border: var(--b) solid var(--edge);
}
.joints button.on { color: var(--ink); background: var(--accent-lo); }
.size {
    position: absolute; right: -6px; bottom: -8px; padding: 0 3px; background: var(--accent); border: 1px solid var(--edge);
    font: 10px/14px var(--font-display); color: var(--ink);
}
</style>
