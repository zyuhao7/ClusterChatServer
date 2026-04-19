import type { PresenceState } from "./protocol"

export interface SessionCard {
    id: string
    title: string
    kind: "direct" | "group"
    presence: PresenceState
    description: string
}

export interface TimelineEntry {
    id: string
    author: string
    timestamp: string
    body: string
}

export const mockSessions: SessionCard[] = [
    {
        id: "direct-alice",
        title: "Alice",
        kind: "direct",
        presence: "busy",
        description: "Direct chat with nickname, profile, and status editing entry points.",
    },
    {
        id: "group-ops",
        title: "Ops Command",
        kind: "group",
        presence: "online",
        description: "Group moderation panel with role, mute, kick, and announcement hooks.",
    },
]

export const mockTimeline: Record<string, TimelineEntry[]> = {
    "direct-alice": [
        {
            id: "m-1",
            author: "Alice",
            timestamp: "2026-04-19 20:10",
            body: "Desktop client phase 1 should make nickname and status flows visible.",
        },
        {
            id: "m-2",
            author: "You",
            timestamp: "2026-04-19 20:12",
            body: "We can keep the protocol stable and move avatars into a richer profile page later.",
        },
    ],
    "group-ops": [
        {
            id: "g-1",
            author: "System",
            timestamp: "2026-04-19 21:00",
            body: "Creators can now promote administrators; admins can only downgrade non-creators.",
        },
        {
            id: "g-2",
            author: "Ops Bot",
            timestamp: "2026-04-19 21:05",
            body: "Next up: mute, kick, announcement, avatar upload, and media transport.",
        },
    ],
}
