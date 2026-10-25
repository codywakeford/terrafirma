import { reactive } from "vue"
import { DEFAULTS, randomSeed, type WorldConfig } from "../../worldgen/config"
import { worldName } from "../../worldgen/names"
import { DEFAULT_RULES, type Mode, type Rules } from "./rules"
import { System } from "../../system/model"

/** The world being set up: everything the create screen's tabs edit together. */
export function WorldDraft() {
    return reactive({
        name: worldName([]),
        /** Names of worlds that already exist, once the screen has asked the engine. */
        taken: [] as string[],
        mode: "survival" as Mode,
        rules: { ...DEFAULT_RULES } as Rules,
        config: { ...DEFAULTS, seed: randomSeed() } as WorldConfig,
        /** The planetary system: causes only (src/system/model.ts). */
        system: System(),
        spawn: null as { x: number; z: number } | null,
    })
}

export type WorldDraftHandle = ReturnType<typeof WorldDraft>

/** Which preset the current values match exactly, if any — so the menu can show "custom". */
export function matching<V extends object>(presets: readonly { id: string; values: Partial<V> }[], current: V): string | null {
    const found = presets.find((p) => Object.entries(p.values).every(([k, v]) => Math.abs(Number(current[k as keyof V]) - Number(v)) < 0.005 || current[k as keyof V] === v))
    return found?.id ?? null
}
