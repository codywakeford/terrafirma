/**
 * The create screen's steps. Start is everything needed to play (most people
 * stop there); World is the system editor; Rules is how survival plays.
 */
export const TABS = [
    { id: "start", label: "Start" },
    { id: "world", label: "World" },
    { id: "rules", label: "Rules" },
] as const satisfies readonly { id: string; label: string }[]

export type TabId = (typeof TABS)[number]["id"]
