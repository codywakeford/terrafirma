// The wiki's demos as sandbox specs: one generated for each reaction from
// its own numbers, and the Lab's hand-made ones for physical behaviour.
// Nothing here simulates: the engine does (src/sandbox).

import type { MatterReaction, MatterState, MatterSubstance, SandboxBlock, SandboxSpec } from "../bridge"

export const AIR: Record<string, number> = { nitrogen: 0.79, oxygen: 0.21 }
const ROOM = 293.15
const ATM = 101325
const R = 8.314462618

/** A substance's state at 1 atm and temperature t, from its anchors (the engine's phase model, simplified to one pressure). */
export function stateAt(s: MatterSubstance, t: number): MatterState {
    if (s.boil !== null && t >= s.boil) return "gas"
    if (s.sublime !== undefined && t >= s.sublime) return "gas"
    if (s.boil === null && s.sublime === undefined && t >= s.critical[0]) return "gas"
    return t >= s.triple[0] ? "liquid" : "solid"
}

export type Demo =
    | { runnable: true; spec: SandboxSpec; temperature: number; note: string }
    | { runnable: false; reason: string }

/**
 * A reaction's demo, from its own numbers: the reactants measured by the
 * equation and put together as a furnace or a flask would, in a 3 × 3 × 3
 * box of room air.
 *
 * - Temperature: where it runs at a pace you can watch (k = 0.2/s), kept
 *   where the liquids it needs are liquid, held there by a heater. (Some, like
 *   rusting, are slow even then: the speed control shows them over hours.)
 * - Solid and liquid reactants share one block; the gas reactants fill the
 *   space around them, in proportion (ore with its carbon monoxide).
 * - A reaction of gases alone fills a few blocks with the mixture, and one
 *   that gives off heat gets a spark.
 */
export function reactionDemo(r: MatterReaction, substances: Map<string, MatterSubstance>): Demo {
    if (r.energy)
        return { runnable: false, reason: `It runs on ${r.energy}: the Reactor can drive it; the sandbox has no ${r.energy === "electricity" ? "current" : "lamp"} yet.` }
    const sub = (id: string) => {
        const s = substances.get(id)
        if (!s) throw new Error(`reaction ${r.id} names ${id}, which matter.list doesn't have`)
        return s
    }
    // where it runs at a visible pace (when its numbers allow one), and above any melting it needs
    let t = ROOM
    if (r.activation > 0 && r.factor > 0.2) t = r.activation * 1000 / (R * Math.log(r.factor / 0.2))
    // a liquid it needs must be liquid: above its melting point, below its boiling point
    for (const q of r.requires ?? [])
        if (q.state === "liquid") t = Math.max(t, sub(q.substance).triple[0] + 40)
    for (const q of r.requires ?? []) {
        const boil = sub(q.substance).boil
        if (q.state === "liquid" && boil !== null) t = Math.min(t, boil - 10)
    }
    if (!Number.isFinite(t)) throw new Error(`reaction ${r.id}: no temperature for its demo`)
    t = Math.max(ROOM, Math.min(3000, t))

    const condensed: [string, number][] = [], gases: [string, number][] = []
    for (const [id, count] of Object.entries(r.reactants)) {
        const s = sub(id)
        const state = stateAt(s, t)
        if (state === "gas") gases.push([id, count])
        else condensed.push([id, count * s.molar_mass / (state === "liquid" ? s.density.liquid : s.density.solid)])
    }
    // what it needs present without using up (rusting's water) goes in too, a share of the block
    for (const q of r.requires ?? [])
        if (!(q.substance in r.reactants) && q.state !== "gas") {
            const s = sub(q.substance)
            const v = condensed.reduce((a, [, x]) => a + x, 0) || s.molar_mass / s.density.liquid
            condensed.push([q.substance, v * 0.3])
        }
    const shares = (list: [string, number][], scale: number) => {
        const total = list.reduce((a, [, v]) => a + v, 0)
        return Object.fromEntries(list.map(([id, v]) => [id, scale * v / total]))
    }
    const gas = gases.length ? shares(gases, 1) : undefined
    const blocks: SandboxBlock[] = []
    if (condensed.length)
        blocks.push({ at: [1, 0, 1], fill: shares(condensed, 0.8), ...(gas ? { gas: gas } : {}), temperature: t, heater: 150 })
    else if (gas) {
        for (const at of [[1, 0, 1], [0, 0, 1], [2, 0, 1], [1, 0, 0], [1, 0, 2], [1, 1, 1]] as const)
            blocks.push({ at: [...at], gas: gas, temperature: t })
        if (r.heat > 0) blocks.push({ at: [1, 0, 1], temperature: Math.max(t, 1100) }) // a spark
    }
    const heat = r.heat > 0 ? "gives off heat" : "takes in heat"
    return {
        runnable: true,
        temperature: t,
        spec: { size: [3, 3, 3], block: 0.05, ambient: ROOM, pressure: ATM, air: AIR, blocks: blocks },
        note: `Its reactants, measured by the equation, at ${Math.round(t)} K, where it runs at a pace you can watch${condensed.length ? "; a heater holds them there" : ""}. It ${heat}.`,
    }
}

/** A physical demo for the Lab: what it shows, and the box that shows it. */
export type LabDemo = { id: string; group: string; title: string; note: string; spec: SandboxSpec; speed: number }

const box = (size: [number, number, number], block: number, blocks: SandboxBlock[], air = AIR): SandboxSpec =>
    ({ size: size, block: block, ambient: ROOM, pressure: ATM, air: air, blocks: blocks })

export const LAB_DEMOS: LabDemo[] = [
    {
        id: "ice", group: "Heat", title: "Ice melts", speed: 60,
        note: "Ice in a warm room warms to 0 °C, then stays there, part ice and part water, until the heat to melt it all has come in. The water runs down and pools.",
        spec: box([1, 3, 1], 0.03, [{ at: [0, 2, 0], fill: { water: 0.9 }, temperature: 258 }]),
    },
    {
        id: "boil", group: "Heat", title: "Water boils", speed: 10,
        note: "A heater under water: it warms to 100 °C and holds there while it boils. The steam rises, meets cooler air and condenses again.",
        spec: box([1, 4, 1], 0.03, [{ at: [0, 0, 0], fill: { water: 0.8 }, temperature: 353, heater: 120 }]),
    },
    {
        id: "conduct", group: "Heat", title: "Copper and wood, heated at one end", speed: 10,
        note: "The same heat into the end of each rod. Copper carries it along in seconds; wood keeps it where it's put.",
        spec: box([2, 1, 6], 0.02, [
            ...[0, 1, 2, 3, 4, 5].flatMap((z): SandboxBlock[] => [
                { at: [0, 0, z], fill: { copper: 1 }, temperature: ROOM },
                { at: [1, 0, z], fill: { wood: 1 }, temperature: ROOM },
            ]),
            { at: [0, 0, 0], heater: 20 }, { at: [1, 0, 0], heater: 20 },
        ]),
    },
    {
        id: "sublime", group: "Heat", title: "Dry ice", speed: 10,
        note: "Solid carbon dioxide at 1 atm never melts: it turns straight to gas at −78 °C, and the heavy cold gas spills out over the top.",
        spec: box([3, 2, 3], 0.04, [{ at: [1, 0, 1], fill: { carbon_dioxide: 0.8 }, temperature: 190 }]),
    },
    {
        id: "pour", group: "Heat", title: "Tin melts and pours", speed: 10,
        note: "Tin melts at 232 °C, low for a metal. Heated on a shelf, it melts and runs down into the space below.",
        spec: box([1, 3, 1], 0.03, [{ at: [0, 2, 0], fill: { tin: 0.7 }, temperature: 480, heater: 80 }]),
    },
    {
        id: "fire", group: "Fire", title: "Wood burns in air", speed: 1,
        note: "Lit wood burns where air reaches it: the fire draws oxygen in and sends carbon dioxide and steam up and out of the open top. Its own heat keeps it going.",
        spec: box([3, 3, 3], 0.05, [{ at: [1, 0, 1], fill: { wood: 0.6 }, temperature: 900, heater: 200 }]),
    },
    {
        id: "kiln", group: "Fire", title: "Wood heated without air", speed: 10,
        note: "The same wood, as hot, but in nitrogen: with no oxygen it can't burn. It chars instead, giving off steam and leaving charcoal: a charcoal kiln.",
        spec: box([3, 3, 3], 0.05, [{ at: [1, 0, 1], fill: { wood: 0.6 }, temperature: 900, heater: 200 }], { nitrogen: 1 }),
    },
    {
        id: "bronze", group: "Metals", title: "Bronze from copper and tin", speed: 10,
        note: "Copper and tin heated together: once both are molten they mix into bronze, harder than either.",
        spec: box([3, 2, 3], 0.04, [{ at: [1, 0, 1], fill: { copper: 0.7, tin: 0.1 }, temperature: 1400, heater: 400 }]), // 15 : 1 by moles
    },
]
