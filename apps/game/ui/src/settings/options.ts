// Visible catalog, navigation, and search. Native validation lives in shell/options.cpp.
export type SettingValue = number | boolean | string
type Requirement = { key: string; value: SettingValue; note?: string }
type Base = { key: string; label: string; note: string; terms?: string; requires?: Requirement; restart?: boolean }
export type Option =
    | Base & { kind: "slider"; min: number; max: number; step: number; format?: (v: number) => string }
    | Base & { kind: "toggle" }
    | Base & { kind: "choice"; choices: readonly { value: string; label: string }[] }
export type Page = { id: string; title: string; note: string; group: string; options: Option[]; preset?: boolean; bindings?: boolean }
export type Category = { id: string; title: string; pages: Page[] }

const pct = (v: number) => `${Math.round(v * 100)}%`
const detail = (v: number) => ["Low", "Medium", "High", "Ultra"][Math.round(v) - 1] ?? String(v)
const corner = (v: number) => ["Top left", "Top right", "Bottom left", "Bottom right"][Math.round(v)] ?? String(v)
const yes = (key: string, note?: string): Requirement => ({ key, value: true, note })
const pick = (...items: [string, string][]) => items.map(([value, label]) => ({ value, label }))

export const CATEGORIES: Category[] = [
{ id: "display", title: "Display", pages: [
  { id: "window", title: "Window", group: "Screen", note: "How Terra Firma uses your display and behaves in the background.", options: [
    { key: "fullscreen", label: "Fullscreen", note: "Fill the display instead of using a window.", kind: "toggle" },
    { key: "vsync", label: "VSync", note: "Match frames to the display to reduce tearing.", kind: "toggle", restart: true },
    { key: "pause_on_lost_focus", label: "Pause when unfocused", note: "Pause a local world when you switch applications.", kind: "toggle" },
  ] },
  { id: "framerate", title: "Frame rate", group: "Performance", note: "Frame limits and world rendering resolution.", options: [
    { key: "fps_max", label: "Maximum frame rate", note: "The frame rate cap while playing.", kind: "slider", min: 30, max: 360, step: 10, format: v => `${v} fps`, terms: "fps" },
    { key: "fps_max_unfocused", label: "Background frame rate", note: "The cap while Terra Firma is out of focus.", kind: "slider", min: 5, max: 60, step: 5, format: v => `${v} fps` },
    { key: "undersampling", label: "World resolution", note: "Render the world lower while keeping the interface sharp.", kind: "slider", min: 1, max: 4, step: 1, format: v => v === 1 ? "100%" : `${Math.round(100 / v)}%`, terms: "render scale" },
  ] },
  { id: "picture", title: "Picture", group: "Screen", note: "Calibrate the image and camera framing.", options: [
    { key: "display_gamma", label: "Brightness curve", note: "Brighten or darken the image mid-tones.", kind: "slider", min: .33, max: 3, step: .05, format: v => v.toFixed(2), terms: "gamma" },
  ] },
  { id: "screenshots", title: "Screenshots", group: "Capture", note: "Choose the format used by the screenshot key.", options: [
    { key: "screenshot_format", label: "File format", note: "PNG is lossless; JPEG creates smaller files.", kind: "choice", choices: pick(["png", "PNG"], ["jpg", "JPEG"]) },
    { key: "screenshot_quality", label: "JPEG quality", note: "Higher quality creates larger files; zero is automatic.", kind: "slider", min: 0, max: 100, step: 5, format: v => v ? `${v}%` : "Automatic", requires: { key: "screenshot_format", value: "jpg", note: "Available for JPEG screenshots." } },
  ] },
] },
{ id: "graphics", title: "Graphics", pages: [
  { id: "presets", title: "Presets", group: "Quality", note: "Start with a graphics profile, then tune individual settings.", preset: true, options: [] },
  { id: "distance", title: "Distance & fog", group: "World", note: "How much world you see and how it meets the atmosphere.", options: [
    { key: "viewing_range", label: "Render distance", note: "Blocks rendered at full detail around you.", kind: "slider", min: 20, max: 1000, step: 10, format: v => `${v} blocks` },
    { key: "far_view", label: "Distant terrain", note: "Draw land beyond loaded blocks.", kind: "toggle", terms: "horizon" },
    { key: "far_view_detail", label: "Horizon detail", note: "Detail in the distant terrain mesh.", kind: "slider", min: 1, max: 4, step: 1, format: detail, requires: yes("far_view") },
    { key: "far_view_max", label: "Furthest view", note: "Maximum distant-terrain range in world blocks.", kind: "slider", min: 2000, max: 64000, step: 1000, format: v => `${v.toLocaleString()} blocks`, requires: yes("far_view") },
    { key: "enable_fog", label: "Fog", note: "Blend the edge of the world into the atmosphere.", kind: "toggle" },
    { key: "fog_start", label: "Fog start", note: "How far into the visible range fog begins.", kind: "slider", min: 0, max: .99, step: .01, format: pct, requires: yes("enable_fog") },
    { key: "directional_colored_fog", label: "Coloured fog", note: "Tint fog for sunrise, sunset, and view direction.", kind: "toggle", requires: yes("enable_fog") },
  ] },
  { id: "lighting", title: "Lighting & shadows", group: "World", note: "Light quality, real-time shadows, and sunlit materials.", options: [
    { key: "smooth_lighting", label: "Smooth lighting", note: "Blend light across blocks and add ambient occlusion.", kind: "toggle" },
    { key: "enable_dynamic_shadows", label: "Dynamic shadows", note: "Cast real-time shadows from the sun.", kind: "toggle" },
    { key: "shadow_filters", label: "Shadow filtering", note: "Smoother edges cost more graphics performance.", kind: "choice", choices: pick(["0", "Sharp"], ["1", "Soft"], ["2", "Softer"]), requires: yes("enable_dynamic_shadows") },
    { key: "shadow_map_texture_size", label: "Shadow resolution", note: "Higher resolution makes distant shadow edges clearer.", kind: "choice", choices: pick(["1024", "Low"], ["2048", "Medium"], ["4096", "High"], ["8192", "Ultra"]), requires: yes("enable_dynamic_shadows") },
    { key: "shadow_map_max_distance", label: "Shadow distance", note: "How far dynamic shadows are drawn.", kind: "slider", min: 10, max: 1000, step: 10, format: v => `${v} blocks`, requires: yes("enable_dynamic_shadows") },
    { key: "shadow_soft_radius", label: "Shadow softness", note: "Increase the apparent sun size for softer edges.", kind: "slider", min: 1, max: 15, step: .5, requires: yes("enable_dynamic_shadows") },
    { key: "shadow_strength_gamma", label: "Shadow strength", note: "Higher values make shadows darker.", kind: "slider", min: .1, max: 3, step: .1, requires: yes("enable_dynamic_shadows") },
    { key: "shadow_map_texture_32bit", label: "High precision shadows", note: "Reduce artefacts at a memory cost.", kind: "toggle", requires: yes("enable_dynamic_shadows") },
    { key: "shadow_map_color", label: "Coloured shadows", note: "Let translucent blocks tint their shadows.", kind: "toggle", requires: yes("enable_dynamic_shadows") },
    { key: "enable_translucent_foliage", label: "Sunlit foliage", note: "Simulate sunlight passing through leaves.", kind: "toggle", requires: yes("enable_dynamic_shadows") },
  ] },
  { id: "textures", title: "Textures & edges", group: "Quality", note: "Texture clarity, block edges, and transparent materials.", options: [
    { key: "leaves_style", label: "Leaves", note: "Fancy draws every face; opaque is fastest.", kind: "choice", choices: pick(["opaque", "Opaque"], ["simple", "Simple"], ["fancy", "Fancy"]) },
    { key: "mip_map", label: "Mipmapping", note: "Keep distant textures stable.", kind: "toggle" },
    { key: "bilinear_filter", label: "Bilinear filtering", note: "Smooth textures when scaled.", kind: "toggle" },
    { key: "trilinear_filter", label: "Trilinear filtering", note: "Smooth transitions between detail levels.", kind: "toggle", requires: yes("mip_map") },
    { key: "anisotropic_filter", label: "Anisotropic filtering", note: "Keep angled surfaces clear.", kind: "toggle", requires: yes("mip_map") },
    { key: "antialiasing", label: "Edge smoothing", note: "Smooth block and object silhouettes.", kind: "choice", choices: pick(["none", "Off"], ["fsaa", "MSAA"], ["ssaa", "SSAA"]) },
    { key: "fsaa", label: "Smoothing samples", note: "More samples improve edges at a performance cost.", kind: "choice", choices: pick(["2", "2×"], ["4", "4×"], ["8", "8×"], ["16", "16×"]), requires: { key: "antialiasing", value: "fsaa" } },
    { key: "fxaa", label: "Fast edge smoothing", note: "Apply a fast post-processing edge pass.", kind: "toggle", requires: yes("enable_post_processing") },
    { key: "connected_glass", label: "Connected glass", note: "Hide shared faces between compatible glass.", kind: "toggle" },
    { key: "translucent_liquids", label: "Translucent liquids", note: "Render scenery through water and liquids.", kind: "toggle" },
  ] },
  { id: "post", title: "Post-processing", group: "Effects", note: "Camera-like effects applied after rendering the world.", options: [
    { key: "enable_post_processing", label: "Post-processing", note: "Enable the effects pipeline.", kind: "toggle" },
    { key: "enable_bloom", label: "Bloom", note: "Add glow around bright light.", kind: "toggle", requires: yes("enable_post_processing") },
    { key: "enable_volumetric_lighting", label: "Volumetric light", note: "Draw shafts of light through the atmosphere.", kind: "toggle", requires: yes("enable_bloom") },
    { key: "tone_mapping", label: "Filmic tone mapping", note: "Compress highlights and deepen mid-range contrast.", kind: "toggle", requires: yes("enable_post_processing") },
    { key: "enable_auto_exposure", label: "Automatic exposure", note: "Adapt brightness between dark and bright places.", kind: "toggle", requires: yes("enable_post_processing") },
    { key: "exposure_compensation", label: "Exposure bias", note: "Favour a brighter or darker image.", kind: "slider", min: -1, max: 1, step: .1, format: v => `${v > 0 ? "+" : ""}${v.toFixed(1)} EV`, requires: yes("enable_auto_exposure") },
    { key: "debanding", label: "Debanding", note: "Dither gradients to reduce visible bands.", kind: "toggle", requires: yes("enable_post_processing") },
  ] },
  { id: "animation", title: "Animation & water", group: "Effects", note: "Movement in vegetation, clouds, water, and local effects.", options: [
    { key: "enable_3d_clouds", label: "3D clouds", note: "Give clouds volume instead of a flat layer.", kind: "toggle" },
    { key: "soft_clouds", label: "Soft cloud shading", note: "Smooth lighting across cloud faces.", kind: "toggle", requires: yes("enable_3d_clouds") },
    { key: "enable_waving_leaves", label: "Moving leaves", note: "Animate leaves in the wind.", kind: "toggle" },
    { key: "enable_waving_plants", label: "Moving plants", note: "Animate grasses and plants.", kind: "toggle" },
    { key: "enable_waving_water", label: "Moving water", note: "Animate liquid surfaces.", kind: "toggle" },
    { key: "water_wave_height", label: "Wave strength", note: "Set the height of animated waves.", kind: "slider", min: 0, max: 4, step: .25, requires: yes("enable_waving_water") },
    { key: "enable_water_reflections", label: "Water reflections", note: "Reflect the world in animated water; dynamic shadows are also required.", kind: "toggle", requires: yes("enable_waving_water") },
    { key: "enable_dig_particles", label: "Digging particles", note: "Show fragments when blocks are dug.", kind: "toggle" },
  ] },
  { id: "performance", title: "Advanced performance", group: "Advanced", note: "Compatibility controls for difficult hardware.", options: [
    { key: "performance_tradeoffs", label: "Performance trade-offs", note: "Allow minor visual glitches to reduce work.", kind: "toggle" },
    { key: "occlusion_culler", label: "Occlusion culling", note: "Modern is preferred; compatible is a fallback.", kind: "choice", choices: pick(["bfs", "Modern"], ["loops", "Compatible"]) },
    { key: "enable_raytraced_culling", label: "Ray-traced culling", note: "Avoid drawing terrain hidden by terrain.", kind: "toggle" },
  ] },
] },
{ id: "controls", title: "Controls", pages: [
  { id: "keybindings", title: "Keybindings", group: "Input", note: "Assign keys for movement, actions, and screens.", bindings: true, options: [] },
  { id: "camera", title: "Camera & mouse", group: "Input", note: "How the view responds to input.", options: [
    { key: "mouse_sensitivity", label: "Mouse sensitivity", note: "How quickly the camera turns.", kind: "slider", min: .05, max: 1, step: .01, format: pct },
    { key: "invert_mouse", label: "Invert vertical look", note: "Move the mouse down to look up.", kind: "toggle" },
    { key: "fov", label: "Field of view", note: "The width of your first-person view.", kind: "slider", min: 45, max: 160, step: 1, format: v => `${v}°`, terms: "camera view" },
    { key: "camera_smoothing", label: "Camera smoothing", note: "Ease camera turns instead of following immediately.", kind: "slider", min: 0, max: 1, step: .05, format: pct },
    { key: "view_bobbing_amount", label: "View bobbing", note: "Camera movement while walking; zero turns it off.", kind: "slider", min: 0, max: 2, step: .1, format: pct, terms: "motion walking accessibility" },
    { key: "arm_inertia", label: "Hand inertia", note: "Let held items lag slightly behind camera movement.", kind: "toggle" },
  ] },
  { id: "movement", title: "Movement", group: "Gameplay", note: "Choose how movement shortcuts behave.", options: [
    { key: "doubletap_jump", label: "Double-tap to fly", note: "Double-tap jump to fly where allowed.", kind: "toggle" },
    { key: "toggle_sneak_key", label: "Toggle sneak", note: "Press once to sneak until pressed again.", kind: "toggle" },
    { key: "toggle_aux1_key", label: "Toggle sprint", note: "Press once to sprint until pressed again.", kind: "toggle" },
    { key: "autojump", label: "Automatic jump", note: "Step automatically over one-block obstacles.", kind: "toggle" },
    { key: "always_fly_fast", label: "Always fly fast", note: "Use fast flight whenever available.", kind: "toggle" },
    { key: "aux1_descends", label: "Sprint descends", note: "Use sprint to descend while flying or swimming.", kind: "toggle" },
  ] },
  { id: "interaction", title: "Interaction", group: "Gameplay", note: "Mouse wheel and held-button behaviour.", options: [
    { key: "enable_hotbar_mouse_wheel", label: "Wheel selects hotbar", note: "Use the mouse wheel for hotbar slots.", kind: "toggle" },
    { key: "invert_hotbar_mouse_wheel", label: "Invert hotbar wheel", note: "Reverse hotbar selection direction.", kind: "toggle", requires: yes("enable_hotbar_mouse_wheel") },
    { key: "safe_dig_and_place", label: "Safe dig and place", note: "Prevent repeated actions while holding a button.", kind: "toggle" },
    { key: "repeat_place_time", label: "Placement repeat delay", note: "Time between held-button placements.", kind: "slider", min: .16, max: 2, step: .02, format: v => `${v.toFixed(2)} s`, requires: { key: "safe_dig_and_place", value: false } },
  ] },
  { id: "controller", title: "Controller", group: "Devices", note: "Tune analogue camera movement and stick dead zones.", options: [
    { key: "joystick_frustum_sensitivity", label: "Look sensitivity", note: "Controller camera rotation speed.", kind: "slider", min: 1, max: 720, step: 5, format: v => `${v}°/s` },
    { key: "joystick_inner_deadzone", label: "Inner dead zone", note: "Ignore small movement around stick centre.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
    { key: "joystick_outer_deadzone", label: "Outer dead zone", note: "Reach maximum input before the stick edge.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  ] },
] },
{ id: "audio", title: "Audio", pages: [{ id: "levels", title: "Volume", group: "Sound", note: "Sound and music levels for active and background play.", options: [
  { key: "sound_volume", label: "Master volume", note: "Overall game sound level.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "music_volume", label: "Music volume", note: "Menu and future in-world music level.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "sound_volume_unfocused", label: "Background volume", note: "Sound multiplier while out of focus.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "mute_sound", label: "Mute all sound", note: "Turn off all game audio.", kind: "toggle" },
] }] },
{ id: "interface", title: "Interface", pages: [
  { id: "scale", title: "Scale & motion", group: "Interface", note: "Size and movement of interface elements.", options: [
    { key: "gui_scaling", label: "Interface size", note: "Scale native in-world dialogs and text.", kind: "slider", min: .5, max: 2, step: .05, format: pct },
    { key: "hud_scaling", label: "HUD size", note: "Scale hotbar and in-world indicators.", kind: "slider", min: .5, max: 2, step: .05, format: pct },
    { key: "smooth_scrolling", label: "Smooth scrolling", note: "Animate scrolling in native lists.", kind: "toggle" },
    { key: "inventory_items_animations", label: "Animated inventory items", note: "Animate item models in native inventories.", kind: "toggle" },
  ] },
  { id: "hud", title: "HUD & targeting", group: "In game", note: "Persistent information and object targeting.", options: [
    { key: "hud_hotbar_max_width", label: "Hotbar width", note: "Maximum screen fraction used by the hotbar.", kind: "slider", min: .25, max: 1, step: .05, format: pct },
    { key: "show_nametag_backgrounds", label: "Nametag backgrounds", note: "Back player names for contrast.", kind: "toggle" },
    { key: "node_highlighting", label: "Target highlight", note: "How the block under the crosshair is identified.", kind: "choice", choices: pick(["box", "Outline"], ["halo", "Halo"], ["none", "Off"]) },
    { key: "crosshair_alpha", label: "Crosshair opacity", note: "Visibility of the centre crosshair.", kind: "slider", min: 0, max: 255, step: 5, format: v => `${Math.round(v / 255 * 100)}%` },
  ] },
  { id: "chat", title: "Chat", group: "In game", note: "Size and history of the chat log.", options: [
    { key: "chat_font_size", label: "Chat text size", note: "Zero follows the interface font size.", kind: "slider", min: 0, max: 36, step: 1, format: v => v ? `${v} pt` : "Automatic" },
    { key: "recent_chat_messages", label: "Recent messages", note: "Messages retained on the HUD.", kind: "slider", min: 2, max: 20, step: 1 },
  ] },
  { id: "feedback", title: "Visual feedback", group: "In game", note: "Control potentially distracting moment-to-moment feedback.", options: [
    { key: "hurt_flash_enabled", label: "Damage flash", note: "Flash the screen when you take damage.", kind: "toggle", terms: "flashing accessibility" },
  ] },
] },
{ id: "hud", title: "HUD", pages: [
  { id: "map", title: "Map", group: "Widgets", note: "A terrain map centred on you in Terra Firma worlds.", options: [
    { key: "widget_map", label: "Show map", note: "Show terrain around you.", kind: "toggle" },
    { key: "widget_map_corner", label: "Map position", note: "Choose the corner where it sits.", kind: "slider", min: 0, max: 3, step: 1, format: corner, requires: yes("widget_map") },
    { key: "widget_map_scale", label: "Map size", note: "Choose its size on screen.", kind: "slider", min: 1, max: 3, step: 1, format: v => ["", "Small", "Medium", "Large"][Math.round(v)]!, requires: yes("widget_map") },
    { key: "widget_map_shape", label: "Map shape", note: "Use a square or circular frame.", kind: "slider", min: 0, max: 1, step: 1, format: v => v ? "Circle" : "Square", requires: yes("widget_map") },
    { key: "widget_map_rotate", label: "Follow heading", note: "Keep your direction at the top.", kind: "toggle", requires: yes("widget_map") },
    { key: "widget_map_zoom", label: "Map zoom", note: "Choose how much ground fits.", kind: "slider", min: 1, max: 5, step: 1, format: v => ["", "Far", "Wide", "Normal", "Close", "Near"][Math.round(v)]!, requires: yes("widget_map") },
  ] },
  { id: "world-controls", title: "World controls", group: "Widgets", note: "Build and time controls when permitted.", options: [
    { key: "widget_world_controls", label: "Show world controls", note: "Show controls while paused.", kind: "toggle" },
    { key: "widget_world_controls_corner", label: "Controls position", note: "Choose the corner where they sit.", kind: "slider", min: 0, max: 3, step: 1, format: corner, requires: yes("widget_world_controls") },
  ] },
] },
]

export const GRAPHICS_PRESETS = [
  { id: "performance", label: "Performance", note: "Short view, minimal effects.", values: { viewing_range: 100, far_view: false, far_view_detail: 1, far_view_max: 2000, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, enable_3d_clouds: false, smooth_lighting: false, mip_map: false, anisotropic_filter: false, antialiasing: "none", fxaa: false, leaves_style: "opaque", enable_waving_leaves: false, enable_waving_plants: false, enable_waving_water: false } },
  { id: "balanced", label: "Balanced", note: "Clear distance, modest detail.", values: { viewing_range: 200, far_view: true, far_view_detail: 2, far_view_max: 16000, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, enable_3d_clouds: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "none", fxaa: true, leaves_style: "simple", enable_waving_leaves: false, enable_waving_plants: false, enable_waving_water: false } },
  { id: "high", label: "High", note: "Long views and full lighting.", values: { viewing_range: 350, far_view: true, far_view_detail: 3, far_view_max: 32000, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, enable_3d_clouds: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "fsaa", fsaa: "2", fxaa: true, leaves_style: "fancy", enable_waving_leaves: true, enable_waving_plants: true, enable_waving_water: true } },
  { id: "ultra", label: "Ultra", note: "Maximum horizon and detail.", values: { viewing_range: 600, far_view: true, far_view_detail: 4, far_view_max: 64000, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, enable_3d_clouds: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "fsaa", fsaa: "4", fxaa: true, leaves_style: "fancy", enable_waving_leaves: true, enable_waving_plants: true, enable_waving_water: true } },
] as const
