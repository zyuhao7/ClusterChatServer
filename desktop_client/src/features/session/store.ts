import { create } from "zustand"

import {
    buildBootstrapTimeline,
    buildSessionsFromLogin,
    RECALL_NOTIFY_MSG,
    resolveSessionId,
    timelineFromHistory,
    toTimelineItem,
    type HistoryEntry,
    type LoginResponsePayload,
    type ProtocolPushEvent,
    type SearchUserEntry,
    type SessionListItem,
    type TimelineItem,
} from "../../lib/protocol"

interface SessionStoreState {
    host: string
    port: number
    userIdInput: string
    password: string
    loggedInUserId: number | null
    loggedInUserName: string
    presence: string
    searchKeyword: string
    historyTargetId: string
    historyGroupId: string
    historyOrder: "asc" | "desc"
    selectedSessionId: string | null
    sessions: SessionListItem[]
    timelines: Record<string, TimelineItem[]>
    searchResults: SearchUserEntry[]
    lastResponse: string
    setField: (field: string, value: string | number) => void
    setLoggedInUser: (userId: number, name: string) => void
    setPresence: (presence: string) => void
    setSearchResults: (results: SearchUserEntry[]) => void
    setLastResponse: (payload: string) => void
    setSelectedSessionId: (sessionId: string | null) => void
    bootstrapFromLogin: (payload: LoginResponsePayload) => void
    replaceHistory: (sessionId: string, entries: HistoryEntry[]) => void
    applyProtocolEvent: (event: ProtocolPushEvent) => void
    upsertSearchSession: (user: SearchUserEntry) => void
    resetSession: () => void
}

const initialState = {
    host: "127.0.0.1",
    port: 9120,
    userIdInput: "",
    password: "",
    loggedInUserId: null,
    loggedInUserName: "",
    presence: "offline",
    searchKeyword: "",
    historyTargetId: "",
    historyGroupId: "",
    historyOrder: "desc" as const,
    selectedSessionId: null,
    sessions: [] as SessionListItem[],
    timelines: {} as Record<string, TimelineItem[]>,
    searchResults: [] as SearchUserEntry[],
    lastResponse: "",
}

function appendTimelineItem(timelines: Record<string, TimelineItem[]>, item: TimelineItem) {
    const next = { ...timelines }
    const current = next[item.sessionId] ?? []
    next[item.sessionId] = [...current, item]
    return next
}

export const useSessionStore = create<SessionStoreState>((set) => ({
    ...initialState,
    setField: (field, value) => set(() => ({ [field]: value } as Partial<SessionStoreState>)),
    setLoggedInUser: (userId, name) => set(() => ({ loggedInUserId: userId, loggedInUserName: name })),
    setPresence: (presence) => set(() => ({ presence })),
    setSearchResults: (searchResults) => set(() => ({ searchResults })),
    setLastResponse: (lastResponse) => set(() => ({ lastResponse })),
    setSelectedSessionId: (selectedSessionId) => set(() => ({ selectedSessionId })),
    bootstrapFromLogin: (payload) =>
        set(() => {
            const sessions = buildSessionsFromLogin(payload)
            const timelineMap = Object.fromEntries(buildBootstrapTimeline(payload))
            return {
                sessions,
                timelines: timelineMap,
                selectedSessionId: sessions[0]?.sessionId ?? null,
            }
        }),
    replaceHistory: (sessionId, entries) =>
        set((state) => ({
            timelines: {
                ...state.timelines,
                [sessionId]: timelineFromHistory(sessionId, entries),
            },
            selectedSessionId: sessionId,
        })),
    applyProtocolEvent: (event) =>
        set((state) => {
            const sessionId = resolveSessionId(event)
            if (!sessionId) {
                if (event.msgid === RECALL_NOTIFY_MSG && event.message_id) {
                    const timelines = Object.fromEntries(
                        Object.entries(state.timelines).map(([key, items]) => [
                            key,
                            items.map((item) =>
                                item.messageId === event.message_id
                                    ? { ...item, recalled: true, body: "message recalled" }
                                    : item,
                            ),
                        ]),
                    )
                    return { timelines }
                }
                return state
            }

            const nextTimelines = appendTimelineItem(state.timelines, toTimelineItem(event, sessionId))
            let sessions = state.sessions
            if (!sessions.some((session) => session.sessionId === sessionId)) {
                sessions = [
                    ...sessions,
                    {
                        sessionId,
                        rawId: event.groupid ?? event.id ?? 0,
                        title: event.groupid ? `Group ${event.groupid}` : event.name || `User ${event.id}`,
                        kind: event.groupid ? "group" : "direct",
                        presence: event.groupid ? "group" : "online",
                        subtitle: event.groupid ? "incoming group message" : "incoming direct message",
                    },
                ]
            }

            return {
                sessions,
                timelines: nextTimelines,
                selectedSessionId: state.selectedSessionId ?? sessionId,
            }
        }),
    upsertSearchSession: (user) =>
        set((state) => {
            const sessionId = `direct-${user.id}`
            if (state.sessions.some((session) => session.sessionId === sessionId)) {
                return { selectedSessionId: sessionId }
            }
            return {
                sessions: [
                    ...state.sessions,
                    {
                        sessionId,
                        rawId: user.id,
                        title: user.name,
                        kind: "direct",
                        presence: user.state,
                        subtitle: `search result · ${user.state}`,
                    },
                ],
                selectedSessionId: sessionId,
            }
        }),
    resetSession: () =>
        set(() => ({
            ...initialState,
            host: initialState.host,
            port: initialState.port,
        })),
}))
