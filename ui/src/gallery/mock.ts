// A stand-in engine for the component gallery: canned replies to every
// intent, and emit() to push engine messages through the real store.

import { inject, type InjectionKey } from "vue"
import type { BodyCard, BodyKind, EngineMessage, MatterElement, MatterSubstance, IntentKind, Intents, LabScenario, LabValue, SlotRef, Stack, StateSections, UiValues, World } from "../bridge"
import { decodeWorldCode } from "../worldgen/config"
import { mockPreview } from "./mockWorld"

type Handlers = { [K in IntentKind]: (request: Intents[K]["request"]) => Intents[K]["response"] }

export function MockBridge() {
    const listeners = new Set<(message: EngineMessage) => void>()
    let worlds: World[] = [
        { name: "Pine Ridge", path: "/worlds/pine_ridge" },
        { name: "Saltwater", path: "/worlds/saltwater" },
    ]
    const settings: Record<string, number | boolean> = {
        viewing_range: 200, fov: 72, fps_max: 60, mouse_sensitivity: 0.2, sound_volume: 0.8,
        fullscreen: false, vsync: false, enable_dynamic_shadows: true,
    }

    // A tiny stand-in for the server's inventory rules: enough to click through.
    const lists: Record<string, { width: number; stacks: (Stack | null)[] }> = {
        main: { width: 0, stacks: Array(32).fill(null) },
        craft: { width: 3, stacks: Array(9).fill(null) },
        craftpreview: { width: 0, stacks: [null] },
        craftresult: { width: 0, stacks: [null] },
    }
    const RECIPES = [
        { needs: "tf_world:sand", makes: { item: "tf_world:sandstone", count: 1, wear: 0 } },
        { needs: "tf_world:stone", makes: { item: "tf_world:stone_bricks", count: 4, wear: 0 } },
    ]
    // The Lab as the game's lab.json files and tf_lab would answer it.
    const SCENARIOS: LabScenario[] = [
        {
            id: "water.spill", mod: "tf_world", title: "Water finds its level", size: { x: 7, y: 3, z: 7 },
            description: "A full basin with a gap in its front wall. Water runs out through the gap and spreads across the floor, thinning as it goes.",
            controls: [{ id: "gap", kind: "slider", label: "Gap", min: 1, max: 5, step: 1, default: 3, unit: "blocks" }],
        },
        {
            id: "falling.blocks", mod: "tf_world", title: "Loose blocks fall", size: { x: 5, y: 7, z: 5 },
            description: "A column hung in the air drops and piles on the floor.",
            controls: [
                { id: "material", kind: "choice", label: "Material", default: "tf_world:sand", options: [{ value: "tf_world:sand", label: "Sand" }, { value: "tf_world:gravel", label: "Gravel" }] },
                { id: "drop", kind: "button", label: "Drop again" },
            ],
        },
        {
            id: "sky.sun", mod: "tf_sky", title: "The sun's path", size: { x: 9, y: 6, z: 9 },
            description: "The solar sky for any latitude and day.",
            controls: [
                { id: "latitude", kind: "slider", label: "Latitude", min: -90, max: 90, step: 1, default: 50, unit: "°" },
                { id: "day", kind: "slider", label: "Day after the spring equinox", min: 0, max: 364, step: 1, default: 91, unit: "days" },
                { id: "noon", kind: "button", label: "Noon" },
            ],
        },
    ]
    let lab: UiValues["lab"] | null = null

    function labDefaults(id: string): Record<string, LabValue> {
        const values: Record<string, LabValue> = {}
        for (const c of SCENARIOS.find((s) => s.id === id)?.controls ?? []) if (c.kind !== "button") values[c.id] = c.default
        return values
    }

    function publishLab() {
        const value = lab ? { ...lab, values: { ...lab.values } } : null
        queueMicrotask(() => emitAll({ kind: "ui", key: "lab", value: value }))
    }

    // Canned body cards: one fixed card per kind, so the System tab can be
    // laid out without the engine. No physics here — that's terragen's alone.
    const CANNED: Record<string, BodyKind> = {
        cinder: "airless", earth: "temperate", moon: "airless", vesper: "cold desert", aster: "airless",
        jovian: "giant", pyre: "volcanic", nacre: "ice shell", hale: "ice shell", morrow: "airless",
    }
    function cannedCard(id: string, kind: BodyKind): BodyCard {
        const temperate = kind === "temperate"
        return {
            id: id, kind: kind, giant: kind === "giant", metal: 0.3, rock: 0.6, ice: kind === "ice shell" ? 0.5 : 0.1,
            radius: kind === "giant" ? 11 : temperate ? 1 : 0.3, gravity: kind === "giant" ? 2.5 : temperate ? 1 : 0.15,
            escape: temperate ? 11.2 : 2.4, sunlight: 1, temperature: temperate ? 288 : 210, equilibrium: 254,
            pressure: kind === "giant" ? null : temperate ? 1 : 0, gases: temperate ? { N2: 0.78, O2: 0.21 } : kind === "giant" ? { H2: 0.86, He: 0.14 } : {},
            field: temperate ? 1 : 0, heat: kind === "volcanic" ? 40 : 1, tidal: kind === "volcanic" ? 40 : 0,
            ocean: temperate, plates: temperate, volcanism: kind === "volcanic", locked: kind !== "temperate" && kind !== "giant",
            layers: kind === "giant"
                ? [{ name: "core", outer: 0.12, molten: false }, { name: "metallic hydrogen", outer: 0.7, molten: true }, { name: "envelope", outer: 1, molten: true }]
                : [{ name: "core", outer: 0.5, molten: temperate }, { name: "mantle", outer: 0.97, molten: false }, { name: "crust", outer: 1, molten: false }],
            history: temperate ? [{ age: 0.1, text: "oceans condense (canned)" }, { age: 0.8, text: "life appears in the oceans (canned)" }] : [],
        }
    }

    // Canned matter: two elements and water, with fixed curve points, so the
    // Matter screen can be laid out. The physics is src/matter's alone.
    const ELEMENTS: MatterElement[] = [
        { number: 1, symbol: "H", name: "Hydrogen", mass: 1.008, category: "nonmetal", period: 1, group: 1, row: 1, col: 1, color: "#ffffff", radius: 31 },
        { number: 8, symbol: "O", name: "Oxygen", mass: 15.999, category: "nonmetal", period: 2, group: 16, row: 2, col: 16, color: "#ff0d0d", radius: 66 },
    ]
    const WATER: MatterSubstance = {
        id: "water", name: "Water", formula: "H₂O", kind: "compound", composition: { H: 2, O: 1 }, molar_mass: 18.015,
        triple: [273.16, 611.657], critical: [647.1, 22.064e6], boil: 373.15, fusion: 6.01, vaporization: 40.65,
        density: { solid: 917, liquid: 1000 }, heat_capacity: 4186, approximate: false, note: "Canned for the gallery.",
        structure: { type: "molecule", atoms: [["O", 0, 0, 0], ["H", 0.757, 0.586, 0], ["H", -0.757, 0.586, 0]], bonds: [[0, 1, 1], [0, 2, 1]] },
        curves: {
            sublimation: [[136.6, 1e-4], [180, 0.05], [220, 3], [250, 76], [273.16, 611.657]],
            vapour: [[273.16, 611.657], [320, 10500], [373.15, 101325], [450, 932000], [550, 6.1e6], [647.1, 22.064e6]],
            melting: [[273.16, 611.657], [273.1, 1e6], [266, 1e8], [200, 1e9], [-466, 1e10]],
        },
    }

    const at = (s: SlotRef) => lists[s.list]!.stacks
    const emitAll = (message: EngineMessage) => listeners.forEach((l) => l(message))

    // a 2×2 of one item anywhere in the 3×3 grid
    function recipe() {
        const g = lists.craft!.stacks
        for (const r of RECIPES)
            for (const [x, y] of [[0, 0], [1, 0], [0, 1], [1, 1]] as const) {
                const square = [g[y * 3 + x], g[y * 3 + x + 1], g[(y + 1) * 3 + x], g[(y + 1) * 3 + x + 1]]
                const others = g.filter((s, i) => s && ![y * 3 + x, y * 3 + x + 1, (y + 1) * 3 + x, (y + 1) * 3 + x + 1].includes(i))
                if (square.every((s) => s?.item === r.needs) && others.length === 0) return { recipe: r, x: x, y: y }
            }
        return null
    }

    function publish() {
        const found = recipe()
        lists.craftpreview!.stacks = [found ? { ...found.recipe.makes } : null]
        const value: StateSections["inventory"] = JSON.parse(JSON.stringify(lists))
        emitAll({ kind: "state", section: "inventory", value: value })
    }

    function take(from: SlotRef, count: number): Stack | null {
        const stacks = at(from)
        const s = stacks[from.index]
        if (!s) return null
        const n = Math.min(count, s.count)
        stacks[from.index] = s.count - n > 0 ? { ...s, count: s.count - n } : null
        return { ...s, count: n }
    }

    function put(list: string, index: number, stack: Stack): Stack | null {
        const stacks = lists[list]!.stacks
        const there = stacks[index]
        if (!there) { stacks[index] = stack; return null }
        if (there.item === stack.item) {
            const room = 99 - there.count, n = Math.min(room, stack.count)
            stacks[index] = { ...there, count: there.count + n }
            return stack.count - n > 0 ? { ...stack, count: stack.count - n } : null
        }
        stacks[index] = stack
        return there // swapped out
    }

    function somewhere(list: string, stack: Stack): Stack | null {
        const stacks = lists[list]!.stacks
        let rest: Stack | null = stack
        for (let i = 0; i < stacks.length && rest; i++) if (stacks[i]?.item === rest.item) rest = put(list, i, rest)
        for (let i = 0; i < stacks.length && rest; i++) if (!stacks[i]) rest = put(list, i, rest)
        return rest
    }

    const handlers: Handlers = {
        ready: () => null,
        capture: () => null,
        "worlds.list": () => worlds,
        "worlds.create": ({ name }) => {
            const world = { name: name, path: `/worlds/${name.toLowerCase().replace(/\W+/g, "_")}` }
            worlds = [...worlds, world]
            return world
        },
        "worlds.delete": ({ path }) => {
            worlds = worlds.filter((w) => w.path !== path)
            return null
        },
        play: () => null,
        "lab.list": () => SCENARIOS,
        "lab.enter": ({ id }) => {
            // as the engine would: a world loads, then tf_lab speaks
            setTimeout(() => {
                emitAll({ kind: "mode", mode: "game" })
                lab = { id: id, values: labDefaults(id), browsing: true, speed: 1, speeds: [0, 1, 10, 100] }
                publishLab()
            }, 600)
            return null
        },
        "screen.open": ({ name }) => {
            queueMicrotask(() => emitAll({ kind: "screen", screen: name }))
            return null
        },
        "ui.send": (event) => {
            if (!lab) return null
            switch (event.name) {
                case "lab.control": if (event.payload.value !== null) lab.values[event.payload.id] = event.payload.value; break
                case "lab.restart": break
                case "lab.speed": lab.speed = event.payload.speed; break
                case "lab.browse": lab.browsing = true; break
                case "lab.step_in": lab.browsing = false; break
            }
            publishLab()
            return null
        },
        quit: () => null,
        "settings.get": () => ({ ...settings }),
        "settings.set": ({ key, value }) => {
            settings[key] = value
            return null
        },
        open: () => null,
        "system.preview": ({ bodies }) => bodies.map((b) => cannedCard(b.id, CANNED[b.id] ?? "airless")),
        "matter.list": () => ({ elements: ELEMENTS, substances: [WATER], reactions: [], rocks: [] }),
        // the mixture comes back as it went in: the gallery runs no chemistry
        "matter.react": ({ amounts, temperature }) => ({ amounts: amounts, temperature: temperature, states: {}, rates: {} }),
        "matter.state": () => ({ state: "liquid", boils_at: 373.15, melts_at: 273.15 }),
        "worldgen.preview": ({ code, date, resolution }) => mockPreview(decodeWorldCode(code), date, resolution),
        "screen.close": () => {
            queueMicrotask(() => listeners.forEach((l) => l({ kind: "screen", screen: null })))
            return null
        },
        "game.leave": () => {
            lab = null
            queueMicrotask(() => emitAll({ kind: "mode", mode: "menu" }))
            return null
        },
        "game.quit": () => null,
        "inventory.move": ({ from, to, count }) => {
            const moving = take(from, count)
            if (!moving) return null
            const back = to.index === undefined ? somewhere(to.list, moving) : put(to.list, to.index, moving)
            if (back) put(from.list, from.index, back) // a swap: the displaced stack takes the emptied slot
            publish()
            return null
        },
        "inventory.craft": ({ count, collect }) => {
            for (let n = 0; n < count; n++) {
                const found = recipe()
                if (!found) break
                const r = lists.craftresult!.stacks[0]
                if (r && r.item !== found.recipe.makes.item) break
                for (const i of [found.y * 3 + found.x, found.y * 3 + found.x + 1, (found.y + 1) * 3 + found.x, (found.y + 1) * 3 + found.x + 1])
                    take({ list: "craft", index: i }, 1)
                put("craftresult", 0, { ...found.recipe.makes })
            }
            if (collect) {
                const r = take({ list: "craftresult", index: 0 }, 999)
                if (r) somewhere("main", r)
            }
            publish()
            return null
        },
        "inventory.drop": ({ from, count }) => {
            take(from, count)
            publish()
            return null
        },
    }

    return {
        connected: true,
        send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<Intents[K]["response"]> {
            console.info(`[mock] ${kind}`, request)
            return Promise.resolve(handlers[kind](request))
        },
        on(listener: (message: EngineMessage) => void) {
            listeners.add(listener)
            return () => listeners.delete(listener)
        },
        emit: emitAll,
        /** Fill the mock inventory and publish it, as joining a world would. */
        stock(stacks: Record<number, Stack>) {
            for (const [i, s] of Object.entries(stacks)) lists.main!.stacks[Number(i)] = s
            publish()
        },
    }
}

/** A flat 16×16 icon with a darker rim, as a data URL. */
export function swatch(top: string, side: string) {
    const c = document.createElement("canvas")
    c.width = c.height = 16
    const g = c.getContext("2d")!
    g.fillStyle = side
    g.fillRect(0, 0, 16, 16)
    g.fillStyle = top
    g.fillRect(1, 1, 14, 5)
    g.fillStyle = "rgb(0 0 0 / 0.35)"
    g.fillRect(0, 15, 16, 1)
    g.fillRect(15, 0, 1, 16)
    return c.toDataURL()
}

export type MockBridgeHandle = ReturnType<typeof MockBridge>
export const MOCK: InjectionKey<MockBridgeHandle> = Symbol("mock")

/** The mock engine, for gallery pages that drive state (emit) directly. */
export function useMock() {
    const mock = inject(MOCK)
    if (!mock) throw new Error("useMock() outside the gallery")
    return mock
}
