import { createApp } from "vue"
import App from "./App.vue"
import { Bridge } from "./bridge"
import { Keyboard } from "./bridge/keyboard"
import { BRIDGE } from "./bridge/use"
import { GameStore, STORE } from "./game/store"
import { LAB, LabFlow } from "./lab/flow"
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
