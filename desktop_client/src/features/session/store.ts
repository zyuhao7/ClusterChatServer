import { create } from "zustand"

import {
    buildBootstrapTimeline,
    buildSessionsFromLogin,
    directSessionId,
    FRIEND_STATE_NOTIFY_MSG,
    groupSessionId,
    parseFriends,
    parseGroups,
    RECALL_NOTIFY_MSG,
    resolveSessionId,
    sessionPreviewFromTimeline,
    timelineFromHistory,
    toTimelineItem,
    type FriendEntry,
    type GroupEntry,
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
    loggedInUserAvatarUrl: string
    presence: string
    searchKeyword: string
    historyOrder: "asc" | "desc"
    selectedSessionId: string | null
    composerText: string
    pinnedSessionIds: string[]
    sessions: SessionListItem[]
    timelines: Record<string, TimelineItem[]>
    searchResults: SearchUserEntry[]
    friends: Record<number, FriendEntry>
    groups: Record<number, GroupEntry>
    lastResponse: string
    setField: (field: string, value: string | number) => void
    setLoggedInUser: (userId: number, name: string) => void
    setLoggedInUserAvatarUrl: (avatarUrl: string) => void
    setPresence: (presence: string) => void
    setSearchResults: (results: SearchUserEntry[]) => void
    setLastResponse: (payload: string) => void
    setSelectedSessionId: (sessionId: string | null) => void
    togglePinnedSession: (sessionId: string) => void
    bootstrapFromLogin: (payload: LoginResponsePayload) => void
    replaceHistory: (sessionId: string, entries: HistoryEntry[]) => void
    applyProtocolEvent: (event: ProtocolPushEvent) => void
    upsertSearchSession: (user: SearchUserEntry) => void
    markFriend: (user: SearchUserEntry) => void
    updateFriendProfile: (userId: number, avatarUrl: string) => void
    upsertGroupSession: (groupId: number, name: string, desc: string) => void
    updateGroupAnnouncement: (groupId: number, announcement: string) => void
    updateGroupMemberMute: (groupId: number, targetId: number, mutedUntil: string) => void
    removeGroupMember: (groupId: number, targetId: number) => void
    appendLocalMessage: (sessionId: string, author: string, body: string, timestamp: string, messageId?: number) => void
    resetSession: () => void
}

const initialState = {
    host: "127.0.0.1",
    port: 9120,
    userIdInput: "",
    password: "",
    loggedInUserId: null,
    loggedInUserName: "",
    loggedInUserAvatarUrl: "",
    presence: "offline",
    searchKeyword: "",
    historyOrder: "desc" as const,
    selectedSessionId: null,
    composerText: "",
    pinnedSessionIds: [],
    sessions: [] as SessionListItem[],
    timelines: {} as Record<string, TimelineItem[]>,
    searchResults: [] as SearchUserEntry[],
    friends: {} as Record<number, FriendEntry>,
    groups: {} as Record<number, GroupEntry>,
    lastResponse: "",
}

function buildLookupMaps(payload: LoginResponsePayload) {
    const friends = Object.fromEntries(parseFriends(payload).map((friend) => [friend.id, friend]))
    const groups = Object.fromEntries(parseGroups(payload).map((group) => [group.id, group]))
    return { friends, groups }
}

function updateSessionMeta(sessions: SessionListItem[], sessionId: string, updater: (session: SessionListItem) => SessionListItem) {
    return sessions.map((session) => (session.sessionId === sessionId ? updater(session) : session))
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
    setLoggedInUserAvatarUrl: (loggedInUserAvatarUrl) => set(() => ({ loggedInUserAvatarUrl })),
    setPresence: (presence) => set(() => ({ presence })),
    setSearchResults: (searchResults) => set(() => ({ searchResults })),
    setLastResponse: (lastResponse) => set(() => ({ lastResponse })),
    setSelectedSessionId: (selectedSessionId) =>
        set((state) => ({
            selectedSessionId,
            sessions: selectedSessionId
                ? updateSessionMeta(state.sessions, selectedSessionId, (session) => ({ ...session, unreadCount: 0 }))
                : state.sessions,
        })),
    togglePinnedSession: (sessionId) =>
        set((state) => ({
            pinnedSessionIds: state.pinnedSessionIds.includes(sessionId)
                ? state.pinnedSessionIds.filter((item) => item !== sessionId)
                : [...state.pinnedSessionIds, sessionId],
        })),
    bootstrapFromLogin: (payload) =>
        set(() => {
            const sessions = buildSessionsFromLogin(payload)
            const timelineEntries = Object.fromEntries(buildBootstrapTimeline(payload))
            const sessionsWithPreview = sessions.map((session) => {
                const preview = sessionPreviewFromTimeline(timelineEntries[session.sessionId] ?? [])
                return { ...session, ...preview }
            })
            const { friends, groups } = buildLookupMaps(payload)
            return {
                sessions: sessionsWithPreview,
                timelines: timelineEntries,
                selectedSessionId: sessionsWithPreview[0]?.sessionId ?? null,
                friends,
                groups,
            }
        }),
    replaceHistory: (sessionId, entries) =>
        set((state) => {
            const timeline = timelineFromHistory(sessionId, entries)
            const preview = sessionPreviewFromTimeline(timeline)
            return {
                timelines: {
                    ...state.timelines,
                    [sessionId]: timeline,
                },
                sessions: updateSessionMeta(state.sessions, sessionId, (session) => ({ ...session, unreadCount: 0, ...preview })),
                selectedSessionId: sessionId,
            }
        }),
    applyProtocolEvent: (event) =>
        set((state) => {
            const sessionId = resolveSessionId(event)
            if (!sessionId) {
                if (event.msgid === FRIEND_STATE_NOTIFY_MSG && event.user_id) {
                    const friendId = event.user_id
                    const nextFriends = state.friends[friendId]
                        ? {
                            ...state.friends,
                            [friendId]: {
                                ...state.friends[friendId],
                                state: event.state ?? state.friends[friendId].state,
                            },
                        }
                        : state.friends
                    const directId = directSessionId(friendId)
                    const nextSessions = updateSessionMeta(state.sessions, directId, (session) => ({
                        ...session,
                        presence: event.state ?? session.presence,
                        subtitle: `friend · ${event.state ?? session.presence}`,
                    }))
                    return {
                        friends: nextFriends,
                        sessions: nextSessions,
                    }
                }
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
                    const sessions = state.sessions.map((session) => ({
                        ...session,
                        ...sessionPreviewFromTimeline(timelines[session.sessionId] ?? []),
                    }))
                    return { timelines, sessions }
                }
                return state
            }

            const timelineItem = toTimelineItem(event, sessionId)
            const nextTimelines = appendTimelineItem(state.timelines, timelineItem)
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
                        unreadCount: 0,
                        latestMessage: "",
                        latestTimestamp: "",
                    },
                ]
            }

            const preview = sessionPreviewFromTimeline(nextTimelines[sessionId] ?? [])
            sessions = updateSessionMeta(sessions, sessionId, (session) => ({
                ...session,
                ...preview,
                unreadCount: state.selectedSessionId === sessionId ? 0 : session.unreadCount + 1,
            }))

            return {
                sessions,
                timelines: nextTimelines,
                selectedSessionId: state.selectedSessionId ?? sessionId,
            }
        }),
    upsertSearchSession: (user) =>
        set((state) => {
            const sessionId = directSessionId(user.id)
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
                        unreadCount: 0,
                        latestMessage: "",
                        latestTimestamp: "",
                    },
                ],
                selectedSessionId: sessionId,
            }
        }),
    markFriend: (user) =>
        set((state) => ({
            friends: {
                ...state.friends,
                [user.id]: { id: user.id, name: user.name, state: user.state },
            },
            sessions: updateSessionMeta(state.sessions, directSessionId(user.id), (session) => ({
                ...session,
                subtitle: `friend · ${user.state}`,
            })),
        })),
    updateFriendProfile: (userId, avatarUrl) =>
        set((state) => ({
            friends: state.friends[userId]
                ? {
                    ...state.friends,
                    [userId]: {
                        ...state.friends[userId],
                        avatar_url: avatarUrl,
                    },
                }
                : state.friends,
        })),
    upsertGroupSession: (groupId, name, desc) =>
        set((state) => {
            const sessionId = groupSessionId(groupId)
            const sessions = state.sessions.some((session) => session.sessionId === sessionId)
                ? state.sessions
                : [
                    ...state.sessions,
                    {
                        sessionId,
                        rawId: groupId,
                        title: name,
                        kind: "group" as const,
                        presence: "group",
                        subtitle: desc || "group",
                        unreadCount: 0,
                        latestMessage: "",
                        latestTimestamp: "",
                    },
                ]
            return {
                sessions,
                groups: {
                    ...state.groups,
                    [groupId]: {
                        id: groupId,
                        groupname: name,
                        groupdesc: desc,
                        users: state.groups[groupId]?.users ?? [],
                        announcement: state.groups[groupId]?.announcement,
                    },
                },
                selectedSessionId: sessionId,
            }
        }),
    updateGroupAnnouncement: (groupId, announcement) =>
        set((state) => ({
            groups: state.groups[groupId]
                ? {
                    ...state.groups,
                    [groupId]: {
                        ...state.groups[groupId],
                        announcement,
                    },
                }
                : state.groups,
            sessions: updateSessionMeta(state.sessions, groupSessionId(groupId), (session) => ({
                ...session,
                subtitle: announcement,
            })),
        })),
    updateGroupMemberMute: (groupId, targetId, mutedUntil) =>
        set((state) => ({
            groups: state.groups[groupId]
                ? {
                    ...state.groups,
                    [groupId]: {
                        ...state.groups[groupId],
                        users: state.groups[groupId].users.map((user) =>
                            user.id === targetId ? { ...user, muted_until: mutedUntil } : user,
                        ),
                    },
                }
                : state.groups,
        })),
    removeGroupMember: (groupId, targetId) =>
        set((state) => ({
            groups: state.groups[groupId]
                ? {
                    ...state.groups,
                    [groupId]: {
                        ...state.groups[groupId],
                        users: state.groups[groupId].users.filter((user) => user.id !== targetId),
                    },
                }
                : state.groups,
        })),
    appendLocalMessage: (sessionId, author, body, timestamp, messageId) =>
        set((state) => {
            const item: TimelineItem = {
                id: `${sessionId}-${messageId ?? Date.now()}`,
                sessionId,
                author,
                body,
                timestamp,
                messageId,
            }
            const timelines = appendTimelineItem(state.timelines, item)
            const preview = sessionPreviewFromTimeline(timelines[sessionId] ?? [])
            return {
                timelines,
                composerText: "",
                sessions: updateSessionMeta(state.sessions, sessionId, (session) => ({ ...session, ...preview })),
            }
        }),
    resetSession: () =>
        set(() => ({
            ...initialState,
            host: initialState.host,
            port: initialState.port,
        })),
}))
