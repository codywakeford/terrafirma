import type { PreviewWire } from "../worldgen/fields"

// The contract between the engine (src/shell/) and the UI.
// Change both sides together: intents are answered by verbs registered with
// Shell::on() (shell.cpp, menu.cpp), messages are sent by Shell::send().

export type Mode = "menu" | "game"

/** In-game screens the engine can open (src/shell/screens.cpp: SCREENS). */
export type ScreenName = "pause" | "inventory" | "lab"

/** A slot in the local player's inventory. */
export type SlotRef = { list: string; index: number }

export type World = { name: string; path: string }

/** How to show an item; icon is null when it has nothing to draw. */
export type ItemInfo = { description: string; icon: string | null }

export type Stack = { item: string; count: number; wear: number }

/** The game as the HUD sees it (src/shell/state.cpp). Each section arrives whole when it changes. */
export type StateSections = {
    hud: { visible: boolean; crosshair: boolean; hotbar: boolean; health: boolean; breath: boolean }
    player: { hp: number; hpMax: number; breath: number; breathMax: number }
    hotbar: { size: number; selected: number }
    inventory: Record<string, { width: number; stacks: (Stack | null)[] }>
}

export type StateMessage = {
    [S in keyof StateSections]: { kind: "state"; section: S; value: StateSections[S] }
}[keyof StateSections]

/** One body's causes, as `system.preview` takes them (src/shell/system.cpp → terragen/body.h). */
export type BodyCausesWire = {
    id: string
    parent: string | null // null orbits the star; else a planet listed before it
    orbit: number         // AU around the star; parent radii around a planet
    eccentricity: number
    mass: number          // Earth masses
    formed: number        // 0..1: hot and close to beyond the frost line
    volatiles: number     // 0..1: the water and gases it formed with
    day: number           // hours
    life: boolean
}

export type BodyKind = "temperate" | "cold desert" | "hothouse" | "volcanic" | "ice shell" | "hydrocarbon" | "airless" | "giant"

/** What a body is today, and how it got there — everything the page shows about it comes from here. */
export type BodyCard = {
    id: string
    kind: BodyKind
    giant: boolean
    metal: number; rock: number; ice: number
    radius: number; gravity: number   // Earth = 1
    escape: number                    // km/s
    sunlight: number                  // Earth = 1
    temperature: number               // kelvin (cloud tops for a giant)
    equilibrium: number               // kelvin, without its air
    pressure: number | null           // bar; null: a giant, no surface
    gases: Partial<Record<"CO2" | "N2" | "O2" | "CH4" | "H2" | "He", number>>
    field: number; heat: number; tidal: number
    ocean: boolean; plates: boolean; volcanism: boolean; locked: boolean
    /** Its shells, centre outward: outer edge as a share of the radius. */
    layers: { name: "core" | "mantle" | "crust" | "ocean" | "ice" | "metallic hydrogen" | "envelope"; outer: number; molten: boolean }[]
    history: { age: number; text: string }[]  // billions of years after formation
}

/** An element's identity, for the periodic table (matter/elements.json). Grid row 9–10: the f-block. */
export type MatterElement = {
    number: number; symbol: string; name: string; mass: number
    category: "nonmetal" | "noble" | "alkali" | "alkaline" | "metalloid" | "halogen" | "post" | "transition" | "lanthanide" | "actinide"
    period: number; group: number | null; row: number; col: number
    color: string   // for drawing its atoms
    radius: number  // covalent radius, pm
}

export type MatterStructure =
    | { type: "molecule"; atoms: [string, number, number, number][]; bonds: [number, number, number][] } // ångströms; bond order
    | { type: "crystal"; lattice: "fcc" | "bcc" | "diamond" | "rocksalt" | "layered"; atoms: string[] }

/** A substance with its phase anchors (matter/substances.json), its phase boundaries sampled by the engine (src/matter). */
export type MatterSubstance = {
    id: string; name: string; formula: string
    kind: "element" | "compound"
    composition: Record<string, number>
    molar_mass: number
    triple: [number, number]; critical: [number, number] // [kelvin, pascals]
    boil: number | null; sublime?: number                // at 1 atm
    fusion: number; vaporization: number                 // kJ/mol
    density: { solid: number; liquid: number }           // kg/m³
    heat_capacity: number                                // J/(kg·K)
    hardness?: number                                    // Mohs
    approximate: boolean
    note: string
    structure: MatterStructure
    curves: { sublimation: [number, number][]; vapour: [number, number][]; melting: [number, number][] }
}

export type MatterState = "solid" | "liquid" | "gas" | "supercritical"

/** A reaction (matter/reactions.json): balanced on load by the engine. */
export type MatterReaction = {
    id: string; name: string; note: string
    reactants: Record<string, number>; products: Record<string, number>
    heat: number        // kJ released per reaction as written (negative: takes heat)
    activation: number  // kJ/mol
    factor: number      // per second
    energy?: "electricity" | "light"
    requires?: { substance: string; state: MatterState }[]
}

/** A rock: a named mixture of minerals by mass (matter/rocks.json). */
export type MatterRock = {
    id: string; name: string; note: string
    origin: "igneous" | "metamorphic" | "sedimentary" | "sediment"
    minerals: Record<string, number>
}

/** A mixture as the Reactor holds it between steps; the engine steps it (`matter.react`). */
export type MatterMixture = {
    amounts: Record<string, number> // moles
    temperature: number             // kelvin
}

/** A Lab scenario control, as a mod's lab.json declares it (games/terrafirma/mods/tf_lab/registry.lua). */
export type LabControl =
    | { id: string; kind: "button"; label: string }
    | { id: string; kind: "toggle"; label: string; default: boolean }
    | { id: string; kind: "slider"; label: string; min: number; max: number; step: number; default: number; unit?: string }
    | { id: string; kind: "choice"; label: string; options: { value: string | number; label: string }[]; default: string | number }

export type LabValue = boolean | number | string

/** A Lab scenario from a mod's lab.json, as `lab.list` returns it (src/shell/lab.cpp). */
export type LabScenario = {
    id: string
    mod: string
    title: string
    description: string
    size: { x: number; y: number; z: number }
    controls?: LabControl[]
}

/**
 * Values game Lua sets for this player's page (player:set_ui). The page keeps
 * the latest per key; null clears it. Each key's owner is noted.
 */
export type UiValues = {
    /**
     * tf_lab/panel.lua, in a scenario's world: which scenario, its control
     * values, whether the camera is orbiting it (`browsing`), and time.
     */
    lab: { id: string; values: Record<string, LabValue>; browsing: boolean; speed: number; speeds: number[] }
}

export type UiMessage = {
    [K in keyof UiValues]: { kind: "ui"; key: K; value: UiValues[K] | null }
}[keyof UiValues]

/** Events the page sends game Lua (core.register_on_ui_event), by name. */
export type UiEvents = {
    /** A control changed (a button press carries no value). */
    "lab.control": { id: string; value: LabValue | null }
    /** Build the scenario again with its current values. */
    "lab.restart": {}
    "lab.speed": { speed: number }
    /** The Lab view opened: orbit the scenario. */
    "lab.browse": {}
    /** The Lab view closed: the camera is the player's again, by the scenario. */
    "lab.step_in": {}
}

export type UiEvent = { [E in keyof UiEvents]: { name: E; payload: UiEvents[E] } }[keyof UiEvents]

/** Engine → UI */
export type EngineMessage =
    | { kind: "mode"; mode: Mode }
    | { kind: "error"; message: string }
    | { kind: "items"; items: Record<string, ItemInfo> }
    | { kind: "screen"; screen: ScreenName | null }
    | StateMessage
    | UiMessage

/** UI → Engine: each intent's request and the engine's response. */
export type Intents = {
    ready: { request: {}; response: null }
    capture: { request: { keyboard: boolean }; response: null }
    "worlds.list": { request: {}; response: World[] }
    /** settings: written to the world's world.mt — creative_mode, enable_damage, tf_* (src/shell/worlds.cpp) */
    "worlds.create": { request: { name: string; code: string; settings: Record<string, string | number | boolean> }; response: World }
    /** terragen's preview of a world code; date 0..1 through the year (src/shell/menu.cpp) */
    "worldgen.preview": { request: { code: string; date: number | null; resolution: number }; response: PreviewWire }
    "worlds.delete": { request: { path: string }; response: null }
    /** The building blocks: every element, and every substance with its phase curves (src/shell/matter.cpp) */
    "matter.list": { request: {}; response: { elements: MatterElement[]; substances: MatterSubstance[]; reactions: MatterReaction[]; rocks: MatterRock[] } }
    /** Step a mixture for `seconds` in its surroundings: what it's become, the states of what's in it, and each running reaction's rate (per second) */
    "matter.react": {
        request: MatterMixture & { surroundings: number; pressure: number; power: number; light: number; seconds: number; spark: boolean }
        response: MatterMixture & { states: Record<string, MatterState>; rates: Record<string, number> }
    }
    /** What a substance is at a temperature (K) and pressure (Pa), and where it would boil and melt at that pressure */
    "matter.state": { request: { id: string; t: number; p: number }; response: { state: MatterState; boils_at: number | null; melts_at: number | null } }
    /** terragen's body model: every body's card and history from the system's causes (src/shell/system.cpp) */
    "system.preview": { request: { star: string; age: number; activity: number; bodies: BodyCausesWire[] }; response: BodyCard[] }
    play: { request: { path: string }; response: null }
    /** Every Lab scenario, from the game's lab.json files (src/shell/lab.cpp). */
    "lab.list": { request: {}; response: LabScenario[] }
    /** Builds that scenario's own world and enters it (src/shell/worlds.cpp: Worlds::lab). */
    "lab.enter": { request: { id: string }; response: null }
    /** An event for game Lua (src/shell/ui.cpp). */
    "ui.send": { request: UiEvent; response: null }
    quit: { request: {}; response: null }
    "settings.get": { request: {}; response: Record<string, number | boolean> }
    "settings.set": { request: { key: string; value: number | boolean }; response: null }
    open: { request: { url: string }; response: null }
    /** Opens a screen the page decides on (src/shell/screens.cpp: SCREENS); one that needs a UI value stays shut without it. */
    "screen.open": { request: { name: ScreenName }; response: null }
    "screen.close": { request: {}; response: null }
    "game.leave": { request: {}; response: null }
    "game.quit": { request: {}; response: null }
    /** No `to.index`: the first slot in the list that fits. */
    "inventory.move": { request: { from: SlotRef; to: { list: string; index?: number }; count: number }; response: null }
    /** Craft from the grid into craftresult; `collect` moves the result straight into main. */
    "inventory.craft": { request: { count: number; collect: boolean }; response: null }
    "inventory.drop": { request: { from: SlotRef; count: number }; response: null }
}

export type IntentKind = keyof Intents

/** Failure codes from bridge.cpp. */
export const FailureCode = {
    Malformed: 1,
    UnknownIntent: 2,
    Refused: 3, // an expected "no": show the message to the player
    EngineError: 4,
} as const

/** Injected by the engine's renderer process (CefMessageRouter). */
export type TerraQuery = (query: {
    request: string
    onSuccess: (response: string) => void
    onFailure: (code: number, message: string) => void
}) => number

declare global {
    interface Window {
        terraQuery?: TerraQuery
        __terra?: { receive: (message: EngineMessage) => void }
    }
}
