import type { PreviewWire } from "../worldgen/fields"
import type { WeatherWire } from "../worldgen/weather"
import type { StarId } from "../worldgen/config"

// The contract between the engine (src/shell/) and the UI.
// Change both sides together: intents are answered by verbs registered with
// Shell::on() (shell.cpp, menu.cpp), messages are sent by Shell::send().

export type Mode = "menu" | "game"

/** In-game screens the engine can open (src/shell/screens.cpp: SCREENS). */
export type ScreenName = "pause" | "inventory" | "lab" | "wiki" | "map"

/** A slot in the local player's inventory. */
export type SlotRef = { list: string; index: number }

export type World = {
    name: string; path: string; code: string | null
    mode: "survival" | "creative" | null; damage: boolean | null
    pinned: boolean; lastOpened: number | null; rules: Record<string, string>
}

/** Local content: reported by src/shell/community.cpp, not a remote catalog. */
export type CommunityMod = {
    id: string; name: string; title: string; description: string; author: string
    source: "builtIn" | "installed"; depends: string[]; enabledWorlds: string[]; configurable: boolean; issue?: string
}
export type CommunityTexture = {
    id: string; name: string; description: string; author: string; active: boolean
}
export type CommunityWorld = { name: string; path: string; code: string | null }
export type PublicServer = {
    id: string; name: string; description: string; host: string
    region: string; players: number; capacity: number; pingMs: number | null
    version: string; tags: string[]; mods: string[]; featured: boolean
    favorite: boolean; compatible: boolean
}
export type ServerDirectory = { status: "ready" | "unavailable"; servers: PublicServer[] }
export type CommunityCatalog = {
    mods: CommunityMod[]; textures: CommunityTexture[]; worlds: CommunityWorld[]
}

/** How to show an item; icon is null when it has nothing to draw. */
export type ItemInfo = { description: string; icon: string | null }
export type CreativeItem = ItemInfo & { item: string; stackMax: number; category: "building" | "nature" | "functional" | "tools" | "items" }

export type Stack = { item: string; count: number; wear: number }

/** The game as the HUD sees it (src/shell/state.cpp). Each section arrives whole when it changes. */
export type StateSections = {
    hud: { visible: boolean; crosshair: boolean; hotbar: boolean; health: boolean; breath: boolean }
    player: { hp: number; hpMax: number; breath: number; breathMax: number }
    mapPose: { u: number; v: number; heading: number; height: number }
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
    conductivity: { solid: number; liquid: number; gas: number } // W/(m·K)
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

/** One block of a sandbox, as the page sets it (src/shell/wiki.cpp): solids or liquids by share of its volume, or a gas by mole share. */
export type SandboxBlock = {
    at: [number, number, number] // y is up
    fill?: Record<string, number>
    gas?: Record<string, number>
    temperature?: number // K
    heater?: number      // W, kept on until set again
}

/** A sandbox to make: blocks along each side (1..16), a block's edge (m), the room around it. */
export type SandboxSpec = {
    size: [number, number, number]
    block: number
    ambient: number  // K
    pressure: number // Pa
    air: Record<string, number> // the room's gas by mole share
    blocks: SandboxBlock[]
}

/** What a sandbox looks like now: per block, x fastest, then z, then y from the bottom. */
export type SandboxFrame = {
    size: [number, number, number]
    time: number          // s simulated
    main: number[]        // index into matter.list's substances, −1: only gas
    state: MatterState[]  // of `main`
    fill: number[]        // share of the block that's solid or liquid
    temperature: number[] // K
    activity: number[]    // W of reaction heat (+ released)
    vapour: number[]      // the gas most unlike the room's air, −1: none
    haze: number[]        // 0..1: how much of the block's gas that is
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
	/** tf_player: the server permits this player to use the creative catalogue. */
	creative: boolean
    /** tf_player: the planet and this player's saved exploration mask. */
    map: { code: string; size: number; radius: number; sea: number; star: StarId; tilt: number; mode: "creative" | "survival"; explored: string; grid: number }
    /** tf_flight: thrust-free projected path, sampled while the full-screen map is open. */
    map_trajectory: { points: [number, number, number][]; ending: "ground" | "time"; seconds: number }
    /** tf_player: the player's place on the world model and the current date. */
    map_position: { x: number; z: number; date: number }
    /** tf_player: server-approved world controls and their current state. */
    world_controls: { mode: "creative" | "survival"; damage: boolean; time: number; paused: boolean }
    /**
     * tf_lab/panel.lua, in a scenario's world: which scenario, its control
     * values, whether the camera is orbiting it (`browsing`), and time.
     */
    lab: { id: string; values: Record<string, LabValue>; browsing: boolean; speed: number; speeds: number[] }
    /** tf_player: the world's rules give this player the wiki (key G) */
    wiki: boolean
    /** tf_matter: the reactions this player has seen happen in the world (ids) */
    discovered: string[]
    /** tf_matter: the last block the probe (or heat gun) read (core.matter.inspect) */
    probe: { node: string; empty?: boolean; temperature?: number; substance?: string; state?: string; fill?: number
        activity?: number; vapour?: string; haze?: number; awake?: boolean }
    /** tf_flight: riding the pack over the blocks, or in space (core.orbit) */
    flight: FlightNear | FlightSpace
    /** tf_weather: the weather where the player stands (core.terra.weather) */
    weather: {
        temperature: number // °C
        humidity: number    // 0..1
        precipitation: number // mm/h
        snow: boolean
        wind: { x: number; z: number } // m/s, east and north
        pressure: number    // hPa
        events?: string[]   // thunderstorm, blizzard, fog, hurricane, drought, ashfall
    }
}

type FlightCommon = {
    boundary: number   // m above the sea where space begins
    altitude: number   // m above the sea
    speed: number      // m/s over the ground
    climb: number      // m/s up
}
/** Riding the flight pack over the planet's blocks. */
export type FlightNear = FlightCommon & { regime: "near"; resting?: boolean }
/** In a bubble of space (pack or not): the orbit it's on. */
export type FlightSpace = FlightCommon & {
    regime: "space"
    radius: number         // m: the planet's
    orbital_speed: number  // m/s against the stars
    apoapsis?: number      // m above the sea; absent when it escapes
    periapsis: number      // m; below 0, the orbit meets the ground
    period?: number        // s; absent when it escapes
    inclination: number    // radians
    density: number        // kg/m³: the air here
    riding: boolean        // in the pack (else floating on foot)
    lone: boolean          // alone in the bubble: the pack's thrust moves the orbit
    buildable: boolean     // blocks can go here (the orbit stays above the boundary)
}

export type UiMessage = {
    [K in keyof UiValues]: { kind: "ui"; key: K; value: UiValues[K] | null }
}[keyof UiValues]

/** Events the page sends game Lua (core.register_on_ui_event), by name. */
export type UiEvents = {
	/** Ask the flight system for a fresh projected path while viewing the globe. */
	"map.trajectory": {}
	/** Replace one hotbar slot from the catalogue; tf_player validates on the server. */
	"creative.take": { item: string; slot: number; count: number }
	"creative.clear": { slot: number }
    "world.mode": { mode: "creative" | "survival" }
    "world.time": { time: "dawn" | "noon" | "dusk" | "midnight" | number }
    "world.clock": { paused: boolean }
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

/**
 * The player's ArcLabs account (src/shell/account.cpp). Signing in is the
 * device flow: `pending` holds the code the browser's approval page shows.
 * Sent whole as an `account` message whenever it changes.
 */
export type Account =
    | { status: "signedOut"; problem?: string }
    | { status: "starting" }
    | { status: "pending"; code: string; url: string }
    | { status: "checking" }
    | { status: "signedIn"; id: string; username: string; name: string | null }
    | { status: "unreachable"; problem: string }

/** An ArcLabs player, as Terra Firma's friends list shows them. */
export type Player = { id: string; username: string; name: string | null }
/** Where a player is: the menu, someone's world, or their own (hosting; `open`: friends can join). */
export type Presence = { state: "menu" | "playing" | "hosting"; world: string | null; open: boolean }
/** A friend, with where they are now (null: offline). */
export type Friend = Player & { presence: Presence | null }
export type Invite = { id: string; from: Player; world: string; expiresAt: string }

/**
 * Friends, presence and invites (src/shell/social.cpp ← arclabs
 * platform/terrafirma/social.ts). Null while signed out. `view` is null until
 * the backend first answers; `problem` says why it may be stale.
 */
export type Social = {
    me: Presence
    /** In the player's own world: they can open it to friends and invite. */
    joinable: boolean
    view: { friends: Friend[]; incoming: Player[]; outgoing: Player[]; invites: Invite[] } | null
    problem?: string
}

/** Engine → UI */
export type EngineMessage =
    | { kind: "mode"; mode: Mode }
    | { kind: "error"; message: string }
    | { kind: "items"; items: Record<string, ItemInfo> }
    | { kind: "screen"; screen: ScreenName | null }
    | { kind: "account"; account: Account }
    | { kind: "social"; social: Social | null }
    | StateMessage
    | UiMessage

/** UI → Engine: each intent's request and the engine's response. */
export type Intents = {
    ready: { request: {}; response: null }
    capture: { request: { keyboard: boolean }; response: null }
    "worlds.list": { request: {}; response: World[] }
    "worlds.pin": { request: { path: string; pinned: boolean }; response: null }
    "servers.list": { request: {}; response: ServerDirectory }
    "servers.join": { request: { id: string }; response: null }
    "servers.favorite.set": { request: { id: string; favorite: boolean }; response: null }
    "community.list": { request: {}; response: CommunityCatalog }
    /** User-installed mods are selected per world; built-ins are required by this build. */
    "community.mod.set": { request: { id: string; world: string; enabled: boolean }; response: null }
    /** A local texture pack is used after restarting Terra Firma; empty id restores defaults. */
    "community.texture.set": { request: { id: string }; response: null }
    /** settings: written to the world's world.mt — creative_mode, enable_damage, tf_* (src/shell/worlds.cpp) */
    "worlds.create": { request: { name: string; code: string; settings: Record<string, string | number | boolean> }; response: World }
    /** terragen's preview of a world code; date 0..1 through the year (src/shell/menu.cpp) */
    "worldgen.preview": { request: { code: string; date: number | null; resolution: number }; response: PreviewWire }
    /** a settled year of the world's weather, `snapshots` maps through it (src/shell/menu.cpp) */
    "weather.preview": { request: { code: string; snapshots: number }; response: WeatherWire }
    "worlds.delete": { request: { path: string }; response: null }
    /** The building blocks: every element, and every substance with its phase curves (src/shell/matter.cpp) */
    "matter.list": { request: {}; response: { elements: MatterElement[]; substances: MatterSubstance[]; reactions: MatterReaction[]; rocks: MatterRock[] } }
    /** The wiki's sandboxes: tiny pieces of the universe run with the game's physics (src/sandbox) */
    "sandbox.create": { request: SandboxSpec; response: { id: number } }
    "sandbox.step": { request: { id: number; seconds: number }; response: SandboxFrame }
    "sandbox.set": { request: SandboxBlock & { id: number }; response: SandboxFrame }
    "sandbox.drop": { request: { id: number }; response: null }
    /** Newton's cannonball on the world's planet (src/shell/wiki.cpp: orbit::Flight); points in planet radii, x east, y up */
    "orbit.coast": {
        request: { altitude: number; speed: number; angle: number; mass: number; drag_area: number; seconds: number }
        response: { radius: number; scale_height: number; seconds: number; end: "ground" | "around" | "time"; bound: boolean
            periapsis: number; apoapsis?: number; orbital_speed: number; points: [number, number][] }
    }
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
    "settings.get": { request: {}; response: Record<string, number | boolean | string> }
    "settings.set": { request: { key: string; value: number | boolean | string }; response: null }
    "settings.apply": { request: { values: Record<string, number | boolean | string> }; response: null }
    "settings.bindings.get": { request: {}; response: Record<string, string> }
    "settings.bindings.set": { request: { key: string; scancode: number; replace: boolean }; response: null }
    "settings.bindings.reset": { request: {}; response: null }
    open: { request: { url: string }; response: null }
    /** The ArcLabs account (src/shell/account.cpp); changes arrive as `account` messages. */
    "account.get": { request: {}; response: Account }
    /** Starts the device flow and opens the browser on the approval page. */
    "account.signIn": { request: {}; response: null }
    "account.cancel": { request: {}; response: null }
    /** Forgets the key on this machine. */
    "account.signOut": { request: {}; response: null }
    /** Checks the saved key again after ArcLabs couldn't be reached. */
    "account.retry": { request: {}; response: null }
    /** Friends, presence and invites (src/shell/social.cpp); changes arrive as `social` messages. */
    "social.get": { request: {}; response: Social | null }
    /** Friends can join the world you're in (your own). */
    "social.open": { request: { open: boolean }; response: null }
    /** Ask a player to be friends, by exact ArcLabs username; accepts if they'd asked you. */
    "social.request": { request: { username: string }; response: null }
    "social.accept": { request: { id: string }; response: null }
    /** Unfriend, decline, or take back a request. */
    "social.remove": { request: { id: string }; response: null }
    /** Invite a friend into the world you're in. */
    "social.invite": { request: { id: string }; response: null }
    "social.dismiss": { request: { id: string }; response: null }
    /**
     * Join a friend's world (menu only): asks STUN and ArcLabs, then the menu
     * leaves for it (src/shell/menu.cpp → src/online/join.h). `name` is theirs, for messages.
     */
    "social.join": { request: { id: string; name: string }; response: null }
    /** Opens a screen the page decides on (src/shell/screens.cpp: SCREENS); one that needs a UI value stays shut without it. */
    "screen.open": { request: { name: ScreenName }; response: null }
    "screen.close": { request: {}; response: null }
    "game.leave": { request: {}; response: null }
    "game.quit": { request: {}; response: null }
	"creative.list": { request: {}; response: CreativeItem[] }
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
