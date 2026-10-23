// How each engine setting is shown. Ranges are enforced by the engine
// (src/shell/options.cpp); keep min/max inside them.

export type Option =
    | { key: string; label: string; kind: "slider"; min: number; max: number; step: number; format?: (v: number) => string }
    | { key: string; label: string; kind: "toggle" }

export type Group = { name: string; options: Option[] }

const percent = (v: number) => `${Math.round(v * 100)}%`

export const GROUPS: Group[] = [
    {
        name: "Video",
        options: [
            { key: "viewing_range", label: "Render distance", kind: "slider", min: 20, max: 1000, step: 10, format: (v) => `${v} blocks` },
            { key: "fov", label: "Field of view", kind: "slider", min: 45, max: 160, step: 1, format: (v) => `${v}°` },
            { key: "fps_max", label: "Max framerate", kind: "slider", min: 30, max: 360, step: 10, format: (v) => `${v} fps` },
            { key: "fullscreen", label: "Fullscreen", kind: "toggle" },
            { key: "vsync", label: "VSync", kind: "toggle" },
            { key: "enable_dynamic_shadows", label: "Shadows", kind: "toggle" },
        ],
    },
    {
        name: "Controls",
        options: [{ key: "mouse_sensitivity", label: "Mouse sensitivity", kind: "slider", min: 0.05, max: 1, step: 0.01, format: percent }],
    },
    {
        name: "Audio",
        options: [{ key: "sound_volume", label: "Master volume", kind: "slider", min: 0, max: 1, step: 0.01, format: percent }],
    },
]
