import { createApp } from "vue"
import App from "./App.vue"
import { Bridge } from "./bridge"

const bridge = Bridge()
const app = createApp(App, { bridge: bridge })
app.mount("#app")

if (bridge.connected) bridge.send({ kind: "ready" })
