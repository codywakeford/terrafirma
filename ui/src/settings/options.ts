// The visible catalog is also the source for navigation and search.
// Every key and range is validated by src/shell/options.cpp.
export type Option =
    | { key: string; label: string; note: string; kind: "slider"; min: number; max: number; step: number; format?: (v: number) => string; terms?: string }
    | { key: string; label: string; note: string; kind: "toggle"; terms?: string }
export type Page = { id: string; title: string; note: string; group: string; options: Option[]; preset?: boolean; bindings?: boolean }
export type Category = { id: string; title: string; pages: Page[] }

const percent = (v: number) => `${Math.round(v * 100)}%`
const detail = (v: number) => ["Low", "Medium", "High", "Ultra"][Math.round(v) - 1] ?? String(v)
const corner = (v: number) => ["Top left", "Top right", "Bottom left", "Bottom right"][Math.round(v)] ?? String(v)
export const CATEGORIES: Category[] = [
    { id: "display", title: "Display", pages: [
        { id: "window", title: "Window", group: "Screen", note: "How Terra Firma uses your screen.", options: [
            { key: "fullscreen", label: "Fullscreen", note: "Fill the display instead of using a window.", kind: "toggle" },
            { key: "vsync", label: "VSync", note: "Match frames to the display to reduce tearing.", kind: "toggle" },
        ] },
        { id: "framerate", title: "Frame rate", group: "Performance", note: "Set limits for play and when the window is inactive.", options: [
            { key: "fps_max", label: "Maximum frame rate", note: "The frame rate cap while playing.", kind: "slider", min: 30, max: 360, step: 10, format: (v) => `${v} fps`, terms: "fps performance" },
            { key: "fps_max_unfocused", label: "Background frame rate", note: "The cap when Terra Firma is out of focus.", kind: "slider", min: 5, max: 60, step: 5, format: (v) => `${v} fps` },
        ] },
    ] },
    { id: "graphics", title: "Graphics", pages: [
        { id: "presets", title: "Presets", group: "Quality", note: "Start with a graphics profile, then tune individual settings.", preset: true, options: [] },
        { id: "distance", title: "Distance", group: "World", note: "How much of the world you can see.", options: [
            { key: "viewing_range", label: "Render distance", note: "Blocks rendered at full detail around you.", kind: "slider", min: 20, max: 1000, step: 10, format: (v) => `${v} blocks`, terms: "view range" },
            { key: "far_view", label: "Distant terrain", note: "Draw the land beyond loaded blocks.", kind: "toggle", terms: "horizon" },
            { key: "far_view_detail", label: "Horizon detail", note: "Detail in the distant terrain mesh.", kind: "slider", min: 1, max: 4, step: 1, format: detail },
            { key: "far_view_max", label: "Furthest view", note: "Maximum distance of the distant terrain.", kind: "slider", min: 2000, max: 64000, step: 1000, format: (v) => `${v / 1000} km`, terms: "horizon distance" },
        ] },
        { id: "effects", title: "Light & effects", group: "World", note: "Lighting and atmosphere in the world.", options: [
            { key: "enable_dynamic_shadows", label: "Dynamic shadows", note: "Cast real-time shadows from the sun.", kind: "toggle" },
            { key: "enable_bloom", label: "Bloom", note: "Add glow around bright light.", kind: "toggle" },
            { key: "enable_fog", label: "Fog", note: "Blend the edge of the world into the atmosphere.", kind: "toggle" },
            { key: "enable_3d_clouds", label: "3D clouds", note: "Give clouds volume instead of a flat layer.", kind: "toggle" },
        ] },
    ] },
    { id: "controls", title: "Controls", pages: [
        { id: "keybindings", title: "Keybindings", group: "Input", note: "Assign keys for movement, actions, and game screens.", bindings: true, options: [] },
        { id: "camera", title: "Camera & mouse", group: "Input", note: "How the view responds to input.", options: [
            { key: "mouse_sensitivity", label: "Mouse sensitivity", note: "How quickly the camera turns.", kind: "slider", min: 0.05, max: 1, step: 0.01, format: percent },
            { key: "invert_mouse", label: "Invert vertical look", note: "Move the mouse down to look up.", kind: "toggle", terms: "invert y axis" },
            { key: "fov", label: "Field of view", note: "The width of your first-person view.", kind: "slider", min: 45, max: 160, step: 1, format: (v) => `${v}°` },
        ] },
        { id: "movement", title: "Movement", group: "Gameplay", note: "Choose how movement shortcuts behave.", options: [
            { key: "doubletap_jump", label: "Double-tap to fly", note: "In worlds that allow flight, double-tap jump to take off.", kind: "toggle" },
            { key: "toggle_sneak_key", label: "Toggle sneak", note: "Press once to sneak until you press again.", kind: "toggle" },
        ] },
    ] },
    { id: "audio", title: "Audio", pages: [
        { id: "levels", title: "Volume", group: "Sound", note: "Set the sound level for active and background play.", options: [
            { key: "sound_volume", label: "Master volume", note: "Overall game sound level.", kind: "slider", min: 0, max: 1, step: 0.01, format: percent },
            { key: "sound_volume_unfocused", label: "Background volume", note: "Sound level when Terra Firma is out of focus.", kind: "slider", min: 0, max: 1, step: 0.01, format: percent },
            { key: "mute_sound", label: "Mute sound", note: "Turn off all game audio.", kind: "toggle" },
        ] },
    ] },
    { id: "interface", title: "Interface", pages: [
        { id: "hud", title: "HUD", group: "In game", note: "Tune the in-world heads-up display.", options: [
            { key: "hud_scaling", label: "HUD size", note: "Scale the hotbar and in-world indicators.", kind: "slider", min: 0.5, max: 2, step: 0.05, format: percent, terms: "ui scale" },
        ] },
    ] },
    { id: "hud", title: "HUD", pages: [
        { id: "map", title: "Map", group: "Widgets", note: "A small terrain map centred on your position. Available in worlds generated by Terra Firma.", options: [
            { key: "widget_map", label: "Show map", note: "Show the terrain around you while playing.", kind: "toggle", terms: "minimap widget" },
            { key: "widget_map_corner", label: "Map position", note: "Choose the corner where the map sits.", kind: "slider", min: 0, max: 3, step: 1, format: corner, terms: "anchor position" },
            { key: "widget_map_scale", label: "Map size", note: "Choose how large the map appears on screen.", kind: "slider", min: 1, max: 3, step: 1,
                format: (v) => ["", "Small", "Medium", "Large"][Math.round(v)] ?? String(v), terms: "scale" },
        ] },
        { id: "world-controls", title: "World controls", group: "Widgets", note: "Change your build mode and the world's clock from the pause screen. Requires the server privilege.", options: [
            { key: "widget_world_controls", label: "Show world controls", note: "Show the world controls widget when the game is paused and you have permission.", kind: "toggle", terms: "creative survival time" },
            { key: "widget_world_controls_corner", label: "Controls position", note: "Choose the corner where world controls sit.", kind: "slider", min: 0, max: 3, step: 1, format: corner, terms: "anchor position" },
        ] },
    ] },
    { id: "accessibility", title: "Accessibility", pages: [
        { id: "motion", title: "Motion & feedback", group: "Comfort", note: "Reduce camera movement and flashing effects.", options: [
            { key: "view_bobbing_amount", label: "View bobbing", note: "Camera movement while walking. Set to zero to turn it off.", kind: "slider", min: 0, max: 2, step: 0.1, format: percent, terms: "motion walking" },
            { key: "hurt_flash_enabled", label: "Damage flash", note: "Flash the screen when you take damage.", kind: "toggle", terms: "flashing" },
        ] },
    ] },
]

export const GRAPHICS_PRESETS = [
    { id: "performance", label: "Performance", note: "Shorter view, minimal effects.", values: { viewing_range: 100, far_view: false, far_view_detail: 1, far_view_max: 2000, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, enable_3d_clouds: false } },
    { id: "balanced", label: "Balanced", note: "Clear distance with modest detail.", values: { viewing_range: 200, far_view: true, far_view_detail: 2, far_view_max: 16000, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, enable_3d_clouds: true } },
    { id: "high", label: "High", note: "Long views and full lighting.", values: { viewing_range: 350, far_view: true, far_view_detail: 3, far_view_max: 32000, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, enable_3d_clouds: true } },
    { id: "ultra", label: "Ultra", note: "Maximum horizon and detail.", values: { viewing_range: 600, far_view: true, far_view_detail: 4, far_view_max: 64000, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, enable_3d_clouds: true } },
] as const
