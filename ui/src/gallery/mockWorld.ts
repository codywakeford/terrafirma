// The JavaScript stand-in for terragen, for the gallery's mock engine. Same
// inputs and world shape as the engine's generator, not the same numbers.

import { star, type WorldConfig } from "../worldgen/config"
import type { PreviewWire } from "../worldgen/fields"
import { declination, insolation } from "../worldgen/sun"
import { seedNumber, torusFbm } from "./noise"

const BIOMES = ["deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland", "forest", "jungle", "taiga", "tundra", "rock", "snow"] as const
type Biome = (typeof BIOMES)[number]

const POLAR_EDGE = 0.035

// in chunks: spreading a whole field into one call can exceed the argument limit
function b64(bytes: Uint8Array) {
    let text = ""
    for (let i = 0; i < bytes.length; i += 8192) text += String.fromCharCode(...bytes.subarray(i, i + 8192))
    return btoa(text)
}

export function mockPreview(config: WorldConfig, date: number | null, resolution: number): PreviewWire {
    const seed = seedNumber(config.seed)
    const octaves = 3 + Math.round(config.roughness * 3)
    const persistence = 0.35 + config.roughness * 0.25
    // no date: the engine shows the yearly mean; the mock approximates with an equinox
    const decl = declination(config.tilt, date ?? 0)
    const warmth = star(config.star).warmth
    const n = resolution * resolution
    const elevation = new Int16Array(n), heats = new Int8Array(n), wets = new Int8Array(n), biomes = new Uint8Array(n)
    const q = (v: number, s: number, lo: number, hi: number) => Math.max(lo, Math.min(hi, Math.trunc(v * s)))

    for (let y = 0; y < resolution; y++) {
        const v = (y + 0.5) / resolution
        const latitude = (0.5 - v) * Math.PI // north positive
        // sunlight on this date, plus a share of the yearly mean (land and sea store heat)
        const sun = insolation(latitude, decl) * 0.6 + insolation(latitude, 0) * 0.4
        const polar = v < POLAR_EDGE || v > 1 - POLAR_EDGE
        for (let x = 0; x < resolution; x++) {
            const u = x / resolution
            const continent = torusFbm(u, v, seed, config.continents * 1.3, octaves, persistence)
                + torusFbm(u, v, seed + 3, config.continents * 9, 3, 0.5) * config.coast * 0.22
            const ridge = 1 - Math.abs(torusFbm(u, v, seed + 7, config.continents * 4, octaves, 0.5))
            const base = continent + (config.land - 0.5) * 0.9
            const inland = Math.max(0, Math.min(1, base / 0.25))
            const e = base < 0 ? base : base * 0.6 + ridge * ridge * config.mountains * inland

            const heat = (sun - 0.55) * 2.2 + warmth + config.temperature * 0.6
                + torusFbm(u, v, seed + 13, 3, 3, 0.5) * 0.2 - Math.max(0, e) * 0.9
            const wet = torusFbm(u, v, seed + 29, 3, 3, 0.5) + config.humidity * 0.6

            const i = y * resolution + x
            elevation[i] = q(e, 10000, -32767, 32767)
            heats[i] = q(heat, 100, -127, 127)
            wets[i] = q(wet, 100, -127, 127)
            biomes[i] = BIOMES.indexOf(polar ? (e < 0 ? "seaice" : "snow") : classify(e, heat, wet))
        }
    }
    return {
        resolution: resolution,
        elevation: b64(new Uint8Array(elevation.buffer)),
        heat: b64(new Uint8Array(heats.buffer)),
        wet: b64(new Uint8Array(wets.buffer)),
        biome: b64(biomes),
        biomes: [...BIOMES],
        // the JS stand-in has no water simulation: no lakes, no rivers
        lake: b64(new Uint8Array(n)),
        grid: 256,
        rivers: "",
        // nor any geology: one plate, all shield, no volcanoes
        province: b64(new Uint8Array(n)),
        plate: b64(new Uint8Array(n)),
        boundary: b64(new Uint8Array(n)),
        volcanoes: "",
    }
}


function classify(e: number, heat: number, wet: number): Biome {
    if (e < 0) return heat < -0.75 ? "seaice" : e < -0.18 ? "deep" : "ocean"
    if (e < 0.03) return heat < -0.6 ? "snow" : "beach"
    if (heat < -0.6) return "snow"
    if (e > 0.5) return heat < -0.1 ? "snow" : "rock"
    if (heat < -0.3) return wet > 0 ? "taiga" : "tundra"
    if (heat > 0.35) return wet > 0.15 ? "jungle" : wet < -0.15 ? "desert" : "savanna"
    return wet > 0.05 ? "forest" : "grassland"
}

