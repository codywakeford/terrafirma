import { shallowRef } from "vue"

export type AudioLevels = {
    master: number
    background: number
    muted: boolean
}

/** The settings page publishes changes so menu music follows the same controls immediately. */
export const audioLevels = shallowRef<AudioLevels | null>(null)

export function publishAudioLevels(settings: Record<string, number | boolean>) {
    const master = settings.sound_volume
    const background = settings.sound_volume_unfocused
    const muted = settings.mute_sound
    if (typeof master !== "number" || typeof background !== "number" || typeof muted !== "boolean")
        throw new Error("audio settings missing from the engine")
    audioLevels.value = { master, background, muted }
}
