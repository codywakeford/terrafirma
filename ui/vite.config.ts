import { defineConfig } from "vite"
import vue from "@vitejs/plugin-vue"

// Relative base so the built app loads from file:// inside the engine.
export default defineConfig({
    base: "./",
    plugins: [vue()],
})
