import { shallowRef } from "vue"
import type { BridgeHandle, UiValues } from "../../bridge"
import { decodePreview, paint, type Fields, type OverlayId } from "../../worldgen/fields"

type Preview = { code: string; fields: Fields }
const preview = shallowRef<Preview | null>(null)
const problem = shallowRef<string | null>(null)
let pending: { code: string; promise: Promise<void> } | null = null
let wanted = ""

/** The create screen's terrain, requested once per world in the game session. */
export function worldMapData() {
    async function ensure(bridge: BridgeHandle, code: string) {
        wanted = code
        if (preview.value?.code === code) return
        if (pending?.code === code) return pending.promise
        problem.value = null
        const promise = bridge.send("worldgen.preview", { code, date: null, resolution: 512 })
            .then((wire) => {
                if (wanted === code) preview.value = { code, fields: decodePreview(wire) }
            })
            .catch((error: unknown) => {
                if (wanted === code) problem.value = error instanceof Error ? error.message : String(error)
            })
            .finally(() => { if (pending?.code === code) pending = null })
        pending = { code, promise }
        return promise
    }
    return { preview, problem, ensure }
}

export function mapImage(fields: Fields, map: UiValues["map"], overlay: OverlayId = "terrain"): ImageData {
    const image = paint(fields, overlay)
    if (map.mode === "creative") return image
    const bytes = Uint8Array.from(atob(map.explored), (character) => character.charCodeAt(0))
    if (bytes.length !== map.grid * map.grid / 8) throw new Error("Invalid exploration mask")
    const r = fields.resolution
    for (let y = 0; y < r; y++) {
        const row = Math.floor(y * map.grid / r)
        for (let x = 0; x < r; x++) {
            const col = Math.floor(x * map.grid / r)
            const bit = row * map.grid + col
            if (bytes[bit >> 3]! & (1 << (bit & 7))) continue
            const i = (y * r + x) * 4
            const shade = (x + y) % 9 === 0 ? 2 : 0
            image.data[i] = 10 + shade
            image.data[i + 1] = 18 + shade
            image.data[i + 2] = 24 + shade
            image.data[i + 3] = 255
        }
    }
    return image
}

/** World-model block coordinates → the create screen's equirectangular UV. */
export function mapUv(x: number, z: number, size: number) {
    const u = ((x + size / 2) / size % 1 + 1) % 1
    const v = Math.max(0, Math.min(1, (size / 2 - z) / size))
    return { u, v }
}
