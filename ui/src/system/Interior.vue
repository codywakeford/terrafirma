<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { BodyCard } from "../bridge"

/**
 * A body cut open: its shells from the core out, to scale, as the body model
 * worked them out (card.layers). Molten shells glow; a liquid core is what
 * runs a magnetic field.
 */
const props = defineProps<{ card: BodyCard; name: string }>()
const canvas = ref<HTMLCanvasElement | null>(null)
let observer: ResizeObserver | null = null

const COLOURS: Record<BodyCard["layers"][number]["name"], [string, string]> = {
    // solid, molten
    core: ["#8a7f72", "#f2a03c"],
    mantle: ["#7a4a36", "#c4582c"],
    crust: ["#6e6a62", "#6e6a62"],
    ocean: ["#2a6aa8", "#2a6aa8"],
    ice: ["#cfe0e8", "#cfe0e8"],
    "metallic hydrogen": ["#8a8aa0", "#a8a0c8"],
    envelope: ["#c8a882", "#d8b892"],
}

function draw() {
    const el = canvas.value
    if (!el) return
    const w = el.clientWidth, h = el.clientHeight, dpr = window.devicePixelRatio || 1
    el.width = Math.round(w * dpr); el.height = Math.round(h * dpr)
    const ctx = el.getContext("2d")!
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    ctx.clearRect(0, 0, w, h)
    const r = Math.min(w, h) * 0.42, cx = w * 0.42, cy = h / 2
    // the whole body as a disc, then a quarter cut away to show the shells
    const outer = [...props.card.layers].reverse()
    for (const layer of outer) {
        const [solid, molten] = COLOURS[layer.name]
        ctx.fillStyle = layer.molten ? molten : solid
        ctx.beginPath()
        ctx.moveTo(cx, cy)
        ctx.arc(cx, cy, Math.max(2, layer.outer * r), -Math.PI / 2, Math.PI)
        ctx.closePath()
        ctx.fill()
    }
    // the uncut surface: the outermost shell, shaded
    const top = props.card.layers[props.card.layers.length - 1]!
    const g = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.3, r * 0.1, cx, cy, r)
    g.addColorStop(0, COLOURS[top.name][top.molten ? 1 : 0])
    g.addColorStop(1, "#0b0f12")
    ctx.fillStyle = g
    ctx.beginPath()
    ctx.moveTo(cx, cy)
    ctx.arc(cx, cy, r, Math.PI, Math.PI * 1.5)
    ctx.closePath()
    ctx.fill()

    // labels along the cut
    ctx.font = '11px "JetBrains Mono", monospace'
    ctx.textAlign = "left"
    let inner = 0
    for (const layer of props.card.layers) {
        const mid = (inner + layer.outer) / 2
        inner = layer.outer
        const y = cy - mid * r
        ctx.strokeStyle = "rgb(232 236 239 / 0.4)"
        ctx.beginPath(); ctx.moveTo(cx + 2, y); ctx.lineTo(cx + r + 16, y); ctx.stroke()
        ctx.fillStyle = "#e8ecef"
        ctx.fillText(`${layer.name}${layer.molten && (layer.name === "core" || layer.name === "mantle") ? " (molten)" : ""}`, cx + r + 20, y + 4)
    }
    ctx.fillStyle = "#8a959d"
    ctx.fillText(`${props.name} · ${props.card.radius.toFixed(2)} Earth radii`, 10, h - 10)
}

watch(() => props.card, draw, { deep: true })
onMounted(() => { observer = new ResizeObserver(draw); observer.observe(canvas.value!); draw() })
onBeforeUnmount(() => observer?.disconnect())
</script>

<template>
    <canvas ref="canvas" class="interior" :aria-label="`${name} cut open: its layers`" />
</template>

<style scoped>
.interior { display: block; width: 100%; height: 100%; }
</style>
