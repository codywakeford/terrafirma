import { inject, reactive, watch, type InjectionKey } from "vue"
import type { BridgeHandle, LabScenario } from "../bridge"
import type { GameStoreHandle } from "../game/store"

/**
 * The Lab as the page runs it. It outlives the worlds under it: opening the
 * Lab lists the scenarios (in the menu); choosing one builds and enters its
 * own world; choosing another leaves that world and enters the next, all
 * under the same page.
 *
 * In a scenario's world the Lab view is a screen: open, the camera orbits
 * the scenario (tf_lab "browsing"); closed — Step in, L or Esc — the player
 * has the camera. Arriving opens it.
 */
export function LabFlow(opts: { bridge: BridgeHandle; store: GameStoreHandle }) {
    const { bridge, store } = opts
    const lab = reactive({
        open: false,
        scenarios: [] as LabScenario[],
        chosen: null as string | null, // the scenario being watched or loaded
        next: null as string | null,   // to enter once the current world has closed
    })

    async function open() {
        lab.scenarios = await bridge.send("lab.list", {})
        lab.open = true
    }

    function choose(id: string) {
        if (id === lab.chosen && store.mode === "game") return
        lab.chosen = id
        if (store.mode === "game") {
            lab.next = id
            bridge.send("game.leave", {})
        } else {
            bridge.send("lab.enter", { id: id })
        }
    }

    /** In a scenario: leave it for the list. In the list: leave the Lab. */
    function back() {
        if (store.mode === "game") {
            lab.chosen = null
            bridge.send("game.leave", {})
        } else {
            lab.open = false
        }
    }

    function find(id: string | null) {
        return lab.scenarios.find((s) => s.id === id) ?? null
    }

    // the old world has closed: enter the next one
    watch(() => store.mode, (mode) => {
        if (mode === "menu" && lab.open && lab.next) {
            const id = lab.next
            lab.next = null
            bridge.send("lab.enter", { id: id })
        }
    })

    // arriving in a scenario opens the Lab view
    watch(() => store.ui.lab !== null, (here, was) => {
        if (here && !was && lab.open) bridge.send("screen.open", { name: "lab" })
    })

    // the view open means orbiting; closed means the player has the camera
    watch(() => store.screen, (screen, was) => {
        if (!store.ui.lab || store.mode !== "game") return
        if (screen === "lab" && was !== "lab") bridge.send("ui.send", { name: "lab.browse", payload: {} })
        if (was === "lab" && screen !== "lab") bridge.send("ui.send", { name: "lab.step_in", payload: {} })
    })

    return { state: lab, open: open, choose: choose, back: back, find: find }
}

export type LabFlowHandle = ReturnType<typeof LabFlow>

export const LAB: InjectionKey<LabFlowHandle> = Symbol("lab")

export function useLab() {
    const lab = inject(LAB)
    if (!lab) throw new Error("useLab() outside the app: the Lab flow was not provided")
    return lab
}
