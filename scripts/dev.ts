// The dev loop: rebuild the engine, serve the UI with hot reload, and run the
// game against the dev server. Extra arguments go to luanti (e.g. --go).

const root = `${import.meta.dir}/..`
const ui = `${root}/ui`
const devUrl = "http://localhost:5173"

async function run(cmd: string[], cwd: string) {
    const code = await Bun.spawn(cmd, { cwd: cwd, stdio: ["inherit", "inherit", "inherit"] }).exited
    if (code !== 0) throw new Error(`${cmd.join(" ")} exited with ${code}`)
}

async function waitFor(url: string, timeoutMs: number) {
    const deadline = Date.now() + timeoutMs
    while (Date.now() < deadline) {
        const up = await fetch(url).then(() => true, () => false)
        if (up) return
        await Bun.sleep(200)
    }
    throw new Error(`UI dev server did not come up at ${url} within ${timeoutMs}ms`)
}

async function launchGame(args: string[]) {
    const deadline = Date.now() + 120_000
    const binary = `${root}/bin/luanti`
    while (true) {
        try {
            return Bun.spawn(["./bin/luanti", "--shell-url", devUrl, ...args], {
                cwd: root,
                stdio: ["inherit", "inherit", "inherit"],
            })
        } catch (error) {
            // Another terminal may be linking the same executable. Linux
            // refuses to exec it until the writer closes the file.
            if (error instanceof Error && "code" in error) {
                const linking = error.code === "ETXTBSY" ||
                    (error.code === "EACCES" && Bun.file(binary).size === 0)
                if (linking) {
                    if (Date.now() >= deadline)
                        throw new Error(`Timed out waiting for ${binary} to finish linking`, { cause: error })
                    await Bun.sleep(250)
                    continue
                }
            }
            throw error
        }
    }
}

await run(["cmake", "--build", "build", "-j"], root)
if (!(await Bun.file(`${ui}/node_modules/.bin/vite`).exists())) await run(["bun", "install"], ui)

const vite = Bun.spawn(["bun", "run", "dev"], { cwd: ui, stdio: ["ignore", "inherit", "inherit"] })
let game: ReturnType<typeof Bun.spawn> | null = null
const stop = () => {
    game?.kill("SIGINT") // lets the engine shut Chromium down cleanly
    vite.kill()
}
process.on("SIGINT", stop)
process.on("SIGTERM", stop)

try {
    await waitFor(devUrl, 15_000)
    game = await launchGame(process.argv.slice(2))
    process.exitCode = await game.exited
} finally {
    stop()
}
