import type { BodyCausesWire } from "../bridge"

/**
 * A planetary system as the player designs it: causes only. What each body
 * becomes — its air, temperature, field, seas, kind and history — comes from
 * terragen's body model (`system.preview`, see ./preview.ts), never from here.
 * planning/systems-and-bodies.md has the model.
 */
export type Body = {
    id: string
    name: string
    parent: string | null // null orbits the star; else a planet
    color: string         // its swatch in lists
    phase: number         // 0..1: where it starts on its orbit, for the system view
    /** The playable world: its land, tilt and year are the world code (its Surface, in the World step). */
    playable: boolean
    orbit: number         // AU around the star; parent radii around a planet
    eccentricity: number
    inclination: number   // degrees
    mass: number          // log₁₀ Earth masses (the slider is logarithmic)
    formed: number        // 0..1: hot and close (0) to beyond the frost line (1)
    volatiles: number     // 0..1: the water and gases it formed with
    day: number           // rotation period, hours
    tilt: number          // degrees (the playable world's is in the world code)
    life: boolean         // whether life arises where it can
}

export const STAR = "star" // what's selected when the star is

type Starter = Omit<Body, "playable" | "life" | "inclination"> & Partial<Pick<Body, "playable" | "life" | "inclination">>

// A system like ours, so the model's worlds are recognisable: a Mercury, an
// Earth and Moon, a small Mars-like world, a Jupiter with four moons.
const STARTER: Starter[] = [
    { id: "cinder", name: "Cinder", parent: null, color: "#a76c55", phase: 0.18, orbit: 0.42, eccentricity: 0.2, mass: -1.22, formed: 0.1, volatiles: 0.05, day: 1400, tilt: 1 },
    { id: "earth", name: "Earth", parent: null, color: "#50a4c5", phase: 0.62, orbit: 1, eccentricity: 0.017, mass: 0, formed: 0.35, volatiles: 0.6, day: 24, tilt: 23, playable: true, life: true },
    { id: "moon", name: "Moon", parent: "earth", color: "#a8a8a1", phase: 0.1, orbit: 60, eccentricity: 0.055, mass: -1.91, formed: 0.35, volatiles: 0.05, day: 655, tilt: 7 },
    { id: "vesper", name: "Vesper", parent: null, color: "#b47d5e", phase: 0.88, orbit: 1.6, eccentricity: 0.09, mass: -0.9, formed: 0.45, volatiles: 0.5, day: 25, tilt: 25 },
    { id: "aster", name: "Aster", parent: "vesper", color: "#9a9188", phase: 0.4, orbit: 6, eccentricity: 0.02, mass: -2.4, formed: 0.5, volatiles: 0.05, day: 30, tilt: 0 },
    { id: "jovian", name: "Jovian", parent: null, color: "#d7ad83", phase: 0.35, orbit: 5.2, eccentricity: 0.05, mass: 2.5, formed: 0.8, volatiles: 0.6, day: 10, tilt: 3 },
    { id: "pyre", name: "Pyre", parent: "jovian", color: "#ce9464", phase: 0.2, orbit: 5.9, eccentricity: 0.0041, mass: -1.82, formed: 0.55, volatiles: 0.05, day: 42, tilt: 0 },
    { id: "nacre", name: "Nacre", parent: "jovian", color: "#c9d6da", phase: 0.55, orbit: 9.4, eccentricity: 0.009, mass: -2.1, formed: 0.8, volatiles: 0.6, day: 85, tilt: 0 },
    { id: "hale", name: "Hale", parent: "jovian", color: "#a9b9b4", phase: 0.75, orbit: 15, eccentricity: 0.0013, mass: -1.6, formed: 0.8, volatiles: 0.5, day: 172, tilt: 0 },
    { id: "morrow", name: "Morrow", parent: "jovian", color: "#868a91", phase: 0.05, orbit: 26, eccentricity: 0.007, mass: -1.73, formed: 0.85, volatiles: 0.4, day: 400, tilt: 0 },
]

/** The causes the body model accepts (terragen/body.cpp: check), so sliders can't ask for a world that can't exist. */
export const LIMITS = {
    mass: { min: -2.6, max: 2.6 },
    orbit: { star: { min: 0.05, max: 100 }, planet: { min: 1.5, max: 200 } },
    eccentricity: { min: 0, max: 0.9 },
    day: { min: 1, max: 6000 },
} as const

const COLORS = ["#8c9eaa", "#ba9678", "#87a99e", "#b3a090", "#aba4bf"]

export function System() {
    return {
        age: 4.5,       // billions of years
        activity: 0.5,  // how hard the star's wind blows
        selected: "earth" as string,
        bodies: STARTER.map((s): Body => ({ playable: false, life: false, inclination: 0, ...s })),
        made: 0,        // bodies added so far, for names and ids
    }
}

export type SystemHandle = ReturnType<typeof System>

export function body(system: SystemHandle, id: string): Body {
    const found = system.bodies.find((b) => b.id === id)
    if (!found) throw new Error(`no body ${id} in the system`)
    return found
}

export function planets(system: SystemHandle) {
    return system.bodies.filter((b) => b.parent === null)
}

export function moonsOf(system: SystemHandle, id: string) {
    return system.bodies.filter((b) => b.parent === id)
}

/** The system as the body model takes it: parents before their moons, masses unlogged. */
export function causes(system: SystemHandle, star: string) {
    const ordered = [...planets(system), ...system.bodies.filter((b) => b.parent !== null)]
    return {
        star: star,
        age: system.age,
        activity: system.activity,
        bodies: ordered.map((b): BodyCausesWire => ({
            id: b.id, parent: b.parent, orbit: b.orbit, eccentricity: b.eccentricity, mass: 10 ** b.mass,
            formed: b.formed, volatiles: b.volatiles, day: b.day, life: b.life,
        })),
    }
}

export function addPlanet(system: SystemHandle) {
    const n = ++system.made
    const id = `planet-${n}`
    const farthest = Math.max(0.4, ...planets(system).map((p) => p.orbit))
    system.bodies.push({
        id: id, name: `Planet ${n}`, parent: null, color: COLORS[n % COLORS.length]!, phase: (n * 0.173) % 1,
        playable: false, orbit: Math.min(LIMITS.orbit.star.max, +(farthest * 1.5).toFixed(2)), eccentricity: 0.03,
        inclination: 1, mass: -0.3, formed: Math.min(1, 0.3 + Math.log10(farthest * 1.5 + 1) * 0.6), volatiles: 0.4,
        day: 30, tilt: 10, life: false,
    })
    system.selected = id
}

export function addMoon(system: SystemHandle, parent: string) {
    if (body(system, parent).parent !== null) throw new Error("a moon needs a planet, not a moon")
    const n = ++system.made
    const id = `moon-${n}`
    const siblings = moonsOf(system, parent)
    const host = body(system, parent)
    system.bodies.push({
        id: id, name: `${host.name} ${["I", "II", "III", "IV", "V", "VI", "VII"][siblings.length] ?? siblings.length + 1}`,
        parent: parent, color: "#aabec5", phase: (n * 0.277) % 1, playable: false,
        orbit: Math.min(LIMITS.orbit.planet.max, 6 + siblings.length * 6), eccentricity: 0.01, inclination: 0,
        mass: -2, formed: host.formed, volatiles: 0.2, day: 100, tilt: 0, life: false,
    })
    system.selected = id
}

export function removeBody(system: SystemHandle, id: string) {
    const gone = body(system, id)
    if (gone.playable) throw new Error("the playable world can't be removed")
    system.bodies = system.bodies.filter((b) => b.id !== id && b.parent !== id)
    system.selected = gone.parent ?? STAR
}
