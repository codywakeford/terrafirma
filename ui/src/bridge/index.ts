import type { EngineMessage, Intent } from "./types"

export type { EngineMessage, Intent, Mode } from "./types"

type Listener = (message: EngineMessage) => void

/** The UI's only line to the engine: intents out, engine messages in. */
export function Bridge() {
    const listeners = new Set<Listener>()
    const query = window.terraQuery

    window.__terra = {
        receive: (message) => listeners.forEach((listener) => listener(message)),
    }

    return {
        connected: query !== undefined,
        send(intent: Intent): Promise<string> {
            if (!query) throw new Error("Bridge is not connected: not running inside the engine")
            return new Promise((resolve, reject) => {
                query({
                    request: JSON.stringify(intent),
                    onSuccess: resolve,
                    onFailure: (code, message) => reject(new Error(`${intent.kind} failed (${code}): ${message}`)),
                })
            })
        },
        on(listener: Listener) {
            listeners.add(listener)
            return () => listeners.delete(listener)
        },
    }
}

export type BridgeHandle = ReturnType<typeof Bridge>
