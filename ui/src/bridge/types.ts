// The contract between the engine (src/shell/bridge.cpp) and the UI.
// Change both sides together.

export type Mode = "menu" | "game"

/** Engine → UI */
export type EngineMessage =
    | { kind: "mode"; mode: Mode }

/** UI → Engine */
export type Intent =
    | { kind: "ready" }
    | { kind: "capture"; keyboard: boolean }

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
