<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import * as THREE from "three"
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js"
import type { MatterElement, MatterSubstance, SandboxFrame } from "../bridge"
import { colorOf, glow, heatColor } from "./colors"

/**
 * A sandbox in 3D: each block drawn as what fills it (a solid cube sized by
 * how much there is, a liquid as a pool), glowing when hot, with flames
 * where a reaction gives off heat and a haze where its gas differs from the
 * air. Drag to orbit, scroll to zoom; hover a block to read it. `heat`
 * colours every block by temperature instead.
 */
const props = defineProps<{ frame: SandboxFrame | null; substances: MatterSubstance[]; elements: Map<string, MatterElement>; heat: boolean }>()
const host = ref<HTMLDivElement | null>(null)
const hover = ref<string | null>(null)

let renderer: THREE.WebGLRenderer, scene: THREE.Scene, camera: THREE.PerspectiveCamera, controls: OrbitControls
let blocks = new THREE.Group(), flames: THREE.Points | null = null, frameId = 0, resize: ResizeObserver
const ray = new THREE.Raycaster(), mouse = new THREE.Vector2()
const cube = new THREE.BoxGeometry(1, 1, 1)
let meshes: { mesh: THREE.Mesh; index: number }[] = []
let sized = ""

function index(f: SandboxFrame, x: number, y: number, z: number) {
    return (y * f.size[2] + z) * f.size[0] + x
}

function rebuild(f: SandboxFrame) {
    scene.remove(blocks)
    blocks.traverse((o) => { if (o instanceof THREE.Mesh) (o.material as THREE.Material).dispose() })
    blocks = new THREE.Group()
    meshes = []
    const [sx, sy, sz] = f.size
    const offset = new THREE.Vector3(-sx / 2 + 0.5, 0.5, -sz / 2 + 0.5)
    const flamePoints: number[] = []
    for (let y = 0; y < sy; y++)
        for (let z = 0; z < sz; z++)
            for (let x = 0; x < sx; x++) {
                const i = index(f, x, y, z)
                const at = new THREE.Vector3(x, y, z).add(offset)
                const main = f.main[i]!, t = f.temperature[i]!
                if (props.heat) {
                    const [r, g, b] = heatColor(t)
                    const m = new THREE.Mesh(cube, new THREE.MeshBasicMaterial({ color: new THREE.Color(r, g, b), transparent: true,
                        opacity: main >= 0 ? 0.85 : 0.18, depthWrite: main >= 0 }))
                    m.position.copy(at)
                    m.scale.setScalar(main >= 0 ? 0.96 : 0.9)
                    blocks.add(m)
                    meshes.push({ mesh: m, index: i })
                    continue
                }
                if (main >= 0 && f.fill[i]! > 0.01) {
                    const s = props.substances[main]!
                    const hot = glow(t)
                    const liquid = f.state[i] === "liquid"
                    const material = new THREE.MeshStandardMaterial({ color: colorOf(s, props.elements), roughness: liquid ? 0.15 : 0.8,
                        metalness: 0, transparent: liquid || s.id === "glass", opacity: liquid ? 0.8 : s.id === "glass" ? 0.55 : 1,
                        emissive: new THREE.Color(...hot.color), emissiveIntensity: hot.strength * 1.6 })
                    const m = new THREE.Mesh(cube, material)
                    const fill = Math.min(1, f.fill[i]!)
                    if (liquid) { // a pool: full width, as deep as there is
                        m.scale.set(0.98, Math.max(0.04, fill), 0.98)
                        m.position.set(at.x, at.y - 0.5 + m.scale.y / 2, at.z)
                    } else { // a lump: a cube of its volume, resting on the floor of the block
                        const side = Math.max(0.15, Math.cbrt(fill)) * 0.98
                        m.scale.setScalar(side)
                        m.position.set(at.x, at.y - 0.5 + side / 2, at.z)
                    }
                    blocks.add(m)
                    meshes.push({ mesh: m, index: i })
                }
                if (f.vapour[i]! >= 0 && f.haze[i]! > 0.02) {
                    const v = props.substances[f.vapour[i]!]!
                    const steam = v.id === "water"
                    const m = new THREE.Mesh(cube, new THREE.MeshBasicMaterial({ color: steam ? "#f4f6f8" : colorOf(v, props.elements),
                        transparent: true, opacity: Math.min(0.45, f.haze[i]! * 0.5), depthWrite: false }))
                    m.position.copy(at)
                    m.scale.setScalar(0.96)
                    blocks.add(m)
                    if (main < 0) meshes.push({ mesh: m, index: i })
                }
                if (f.activity[i]! > 0.5 && t > 600)
                    for (let k = 0; k < Math.min(24, 4 + f.activity[i]! / 20); k++)
                        flamePoints.push(at.x + (Math.random() - 0.5) * 0.8, at.y + Math.random() * 0.9, at.z + (Math.random() - 0.5) * 0.8)
            }
    // the sandbox's walls (open at the top)
    const walls = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(sx, sy, sz)),
        new THREE.LineBasicMaterial({ color: "#3a4048" }))
    walls.position.set(0, sy / 2, 0)
    blocks.add(walls)
    scene.add(blocks)

    if (flames) { scene.remove(flames); flames.geometry.dispose() }
    flames = null
    if (flamePoints.length) {
        const g = new THREE.BufferGeometry()
        g.setAttribute("position", new THREE.Float32BufferAttribute(flamePoints, 3))
        flames = new THREE.Points(g, new THREE.PointsMaterial({ color: "#ffb040", size: 0.16, transparent: true, opacity: 0.85,
            blending: THREE.AdditiveBlending, depthWrite: false }))
        scene.add(flames)
    }

    const key = f.size.join("×")
    if (key !== sized) { // frame a new size once; then the viewer's own angle stays
        sized = key
        const reach = Math.max(sx, sy, sz)
        camera.position.set(reach * 1.4, reach * 1.25, reach * 1.6)
        controls.target.set(0, sy / 2, 0)
        controls.update()
    }
}

function describe(i: number): string {
    const f = props.frame
    if (!f) return ""
    const parts = [`${Math.round(f.temperature[i]!)} K (${Math.round(f.temperature[i]! - 273.15)} °C)`]
    if (f.main[i]! >= 0) parts.unshift(`${props.substances[f.main[i]!]!.name} · ${f.state[i]} · ${Math.round(f.fill[i]! * 100)}% full`)
    else parts.unshift("gas")
    if (f.vapour[i]! >= 0 && f.haze[i]! > 0.02) parts.push(`${props.substances[f.vapour[i]!]!.name} ${Math.round(f.haze[i]! * 100)}% of its gas`)
    if (Math.abs(f.activity[i]!) > 0.05) parts.push(`${f.activity[i]! > 0 ? "giving off" : "taking in"} ${Math.abs(f.activity[i]!).toFixed(1)} W`)
    return parts.join(" · ")
}

function pointer(e: PointerEvent) {
    if (!host.value) return
    const r = host.value.getBoundingClientRect()
    mouse.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1)
    ray.setFromCamera(mouse, camera)
    const hit = ray.intersectObjects(meshes.map((m) => m.mesh))[0]
    const found = hit ? meshes.find((m) => m.mesh === hit.object) : undefined
    hover.value = found ? describe(found.index) : null
}

onMounted(() => {
    const el = host.value!
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
    renderer.setPixelRatio(window.devicePixelRatio)
    el.appendChild(renderer.domElement)
    scene = new THREE.Scene()
    camera = new THREE.PerspectiveCamera(40, 1, 0.05, 200)
    controls = new OrbitControls(camera, renderer.domElement)
    controls.enableDamping = true
    scene.add(new THREE.HemisphereLight("#dfe8f0", "#2a2622", 1.6))
    const sun = new THREE.DirectionalLight("#ffffff", 1.4)
    sun.position.set(4, 8, 5)
    scene.add(sun)
    resize = new ResizeObserver(() => {
        const { width, height } = el.getBoundingClientRect()
        renderer.setSize(width, height)
        camera.aspect = width / Math.max(1, height)
        camera.updateProjectionMatrix()
    })
    resize.observe(el)
    const loop = () => {
        frameId = requestAnimationFrame(loop)
        if (flames) flames.rotation.y += 0.01 // a flicker
        controls.update()
        renderer.render(scene, camera)
    }
    loop()
    if (props.frame) rebuild(props.frame)
})

watch(() => [props.frame, props.heat] as const, ([f]) => { if (f && scene) rebuild(f) })

onBeforeUnmount(() => {
    cancelAnimationFrame(frameId)
    resize.disconnect()
    controls.dispose()
    renderer.dispose()
})
</script>

<template>
    <div class="sandbox">
        <div ref="host" class="gl" @pointermove="pointer" @pointerleave="hover = null" />
        <p v-if="hover" class="hover">{{ hover }}</p>
    </div>
</template>

<style scoped>
.sandbox { position: relative; min-height: 0; }
.gl { position: absolute; inset: 0; }
.hover { position: absolute; left: 10px; bottom: 10px; margin: 0; padding: 6px 10px; border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.86); font-size: 11px; color: var(--ink); pointer-events: none; }
</style>
