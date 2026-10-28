import { onBeforeUnmount, ref, watch, type Ref } from "vue"
import type { BridgeHandle, PlanetariumBody, PlanetariumFlight, PlanetariumMarker, PlanetariumPlace, PlanetariumViewport } from "../bridge"

/**
 * The native Planetarium (src/shell/planetarium.h) under a part of the page:
 * shown while `active`, drawn where `element` is (followed as it moves or
 * resizes), the body swapped in when it changes (the old one stays on
 * screen until the new one is ready), markers kept up to date. Hidden when
 * inactive or unmounted. `problem` holds the last refusal.
 */
export function usePlanetarium(opts: {
    bridge: BridgeHandle
    element: Ref<HTMLElement | null>
    active: Ref<boolean>
    body: Ref<PlanetariumBody>
    markers?: Ref<PlanetariumMarker[]>
    /** Drawn over the whole window, under the page (the menu's universe), not just `element`. */
    whole?: boolean
    /** What the page covers of the window's sides (page pixels: Dock's `covers`): the planet is framed in the rest. */
    frame?: Ref<{ left: number; right: number }>
    /** Where the camera starts, the first time it's shown. */
    start?: () => (PlanetariumPlace & { altitude?: number }) | null
}) {
    const problem = ref<string | null>(null)
    let shown = false
    let started = false
    const fail = (what: string) => (error: unknown) => {
        problem.value = error instanceof Error ? error.message : String(error)
        console.error(what, error)
    }
    const WHOLE: PlanetariumViewport = { whole: true }
    const viewport = (): PlanetariumViewport => {
        if (opts.whole) return WHOLE
        const rect = opts.element.value?.getBoundingClientRect()
        return rect && rect.width > 0 && rect.height > 0 ? { x: rect.left, y: rect.top, width: rect.width, height: rect.height } : WHOLE
    }
    function show() {
        if (!opts.active.value || !opts.element.value) return hide()
        const focus = started ? undefined : opts.start?.() ?? undefined
        started ||= !!focus
        shown = true
        problem.value = null
        void opts.bridge.send("planetarium.show", {
            body: opts.body.value, viewport: viewport(), markers: opts.markers?.value ?? [], ...(focus ? { focus } : {}),
        }).catch(fail("planetarium.show"))
        framed()
    }
    function hide() {
        if (!shown) return
        shown = false
        void opts.bridge.send("planetarium.hide", {}).catch(fail("planetarium.hide"))
    }
    /** Over a place on the body, or to another body of its system (by id). */
    function focus(place: (PlanetariumPlace & { altitude?: number }) | { body: string }) {
        if (shown) void opts.bridge.send("planetarium.focus", place).catch(fail("planetarium.focus"))
    }
    /** Eases the camera there over `seconds`: to a body (by id), over a place, at an altitude (blocks). */
    function fly(flight: PlanetariumFlight) {
        if (shown) void opts.bridge.send("planetarium.fly", flight).catch(fail("planetarium.fly"))
    }
    const follow = () => { if (shown) void opts.bridge.send("planetarium.viewport", viewport()).catch(fail("planetarium.viewport")) }
    const framed = () => {
        if (shown && opts.frame) void opts.bridge.send("planetarium.frame", opts.frame.value).catch(fail("planetarium.frame"))
    }
    if (opts.frame) watch(opts.frame, framed, { deep: true })
    const resized = new ResizeObserver(follow)
    watch(opts.element, (el, was) => {
        if (was) resized.unobserve(was)
        if (el) resized.observe(el)
        show()
    }, { immediate: true })
    window.addEventListener("resize", follow)
    watch([opts.active, () => JSON.stringify(opts.body.value)], show)
    if (opts.markers)
        watch(opts.markers, (markers) => {
            if (shown) void opts.bridge.send("planetarium.markers", { markers }).catch(fail("planetarium.markers"))
        }, { deep: true })
    onBeforeUnmount(() => {
        resized.disconnect()
        window.removeEventListener("resize", follow)
        hide()
    })
    return { problem, focus, fly }
}
