import { createApp } from "vue"
import App from "./App.vue"
import { Bridge } from "./bridge/index.ts"
import { Keyboard } from "./bridge/keyboard.ts"
import { BRIDGE } from "./bridge/use.ts"
import { GameStore, STORE } from "./game/store.ts"
import { LAB, LabFlow } from "./lab/flow.ts"
import "./theme.css"

const bridge = Bridge()
const store = GameStore({ bridge: bridge })
const lab = LabFlow({ bridge: bridge, store: store })

const app = createApp(App)
app.provide(BRIDGE, bridge)
app.provide(STORE, store)
app.provide(LAB, lab)
app.mount("#app")

if (bridge.connected) {
    Keyboard({ bridge: bridge })
    bridge.send("ready", {})
}
