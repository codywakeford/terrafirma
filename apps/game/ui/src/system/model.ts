import type { BodyCausesWire, SystemBodyWire, SystemWire } from "../bridge"

/**
 * A planetary system as the player designs it: causes only. What each body
 * becomes — its air, temperature, field, seas, kind and history — comes from
 * terragen's body model (`system.preview`, see ./preview.ts), never from here.
 * planning/systems-and-bodies.md has the model.
 */
/** A body as the page holds it: the engine's shape (`SystemBodyWire`). */
export type Body = SystemBodyWire

export const STAR = "star" // what's selected when the star is

/** The causes the body model accepts (terragen/body.cpp: check), so sliders can't ask for a world that can't exist. */
export const LIMITS = {
    mass: { min: -2.6, max: 2.6 },
    orbit: { star: { min: 0.05, max: 100 }, planet: { min: 1.5, max: 200 } },
    eccentricity: { min: 0, max: 0.9 },
    day: { min: 1, max: 6000 },
} as const

const COLORS = ["#8c9eaa", "#ba9678", "#87a99e", "#b3a090", "#aba4bf"]

/** A draft of a system, from one the engine gave (`system.home`: the one every world has today). */
export function System(from: SystemWire) {
    return {
        age: from.age,           // billions of years
        activity: from.activity, // how hard the star's wind blows
        selected: from.bodies.find((b) => b.playable)?.id ?? STAR,
        bodies: from.bodies.map((b): Body => ({ ...b })),
        made: 0,                 // bodies added so far, for names and ids
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
