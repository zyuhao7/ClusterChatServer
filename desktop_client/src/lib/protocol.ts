export type PresenceState = "online" | "offline" | "busy"

export interface ProtocolEnvelope {
    version: number
    msgid: number
    request_id: string
}

export interface SearchUserEntry {
    id: number
    name: string
    state: PresenceState | string
    is_friend: boolean
    has_blocked: boolean
    blocked_by_target: boolean
}

export interface UserProfile {
    id: number
    name: string
    state: PresenceState | string
    avatar_url?: string
}

export interface HistoryEntry {
    id: number
    sender_id: number
    receiver_id?: number
    group_id?: number
    message: string
    read_state: string
    recalled: number
    created_at: string
}

export interface FriendEntry {
    id: number
    name: string
    state: PresenceState | string
    avatar_url?: string
}

export interface GroupMemberEntry extends FriendEntry {
    role: string
    muted_until?: string
}

export interface GroupEntry {
    id: number
    groupname: string
    groupdesc: string
    announcement?: string
    users: GroupMemberEntry[]
}

export interface LoginResponsePayload {
    id: number
    name: string
    friends?: string[]
    groups?: string[]
    offlinemsg?: string[]
    errno: number
    errmsg: string
    request_id: string
}

export interface SessionListItem {
    sessionId: string
    rawId: number
    title: string
    kind: "direct" | "group"
    presence: string
    subtitle: string
    unreadCount: number
    latestMessage: string
    latestTimestamp: string
}

export interface TimelineItem {
    id: string
    sessionId: string
    author: string
    body: string
    timestamp: string
    recalled?: boolean
    messageId?: number
    attachment?: AttachmentPayload
}

export interface AttachmentPayload {
    kind: "image" | "file"
    url: string
    name: string
    mime: string
    size: number
}

export interface ProtocolPushEvent {
    version: number
    msgid: number
    request_id?: string
    id?: number
    name?: string
    toid?: number
    groupid?: number
    msg?: string
    time?: string
    message_id?: number
    read_state?: string
    operator_id?: number
    user_id?: number
    state?: string
}

export const CHAT_PROTOCOL_VERSION = 1
export const ONE_CHAT_MSG = 7
export const GROUP_CHAT_MSG = 15
export const RECALL_NOTIFY_MSG = 25
export const FRIEND_STATE_NOTIFY_MSG = 44
export const ONE_CHAT_MSG_ACK = 8
export const GROUP_CHAT_MSG_ACK = 16

export function directSessionId(userId: number) {
    return `direct-${userId}`
}

export function groupSessionId(groupId: number) {
    return `group-${groupId}`
}

export function parseFriends(payload: LoginResponsePayload) {
    return (payload.friends ?? []).map((item) => JSON.parse(item) as FriendEntry)
}

export function parseGroups(payload: LoginResponsePayload) {
    return (payload.groups ?? []).map((item) => {
        const group = JSON.parse(item) as Omit<GroupEntry, "users"> & { users: string[] }
        return {
            ...group,
            users: (group.users ?? []).map((user) => JSON.parse(user) as GroupMemberEntry),
        } satisfies GroupEntry
    })
}

export function buildSessionsFromLogin(payload: LoginResponsePayload) {
    const sessions: SessionListItem[] = []

    for (const friend of parseFriends(payload)) {
        sessions.push({
            sessionId: directSessionId(friend.id),
            rawId: friend.id,
            title: friend.name,
            kind: "direct",
            presence: friend.state,
            subtitle: `friend · ${friend.state}`,
            unreadCount: 0,
            latestMessage: "",
            latestTimestamp: "",
        })
    }

    for (const group of parseGroups(payload)) {
        sessions.push({
            sessionId: groupSessionId(group.id),
            rawId: group.id,
            title: group.groupname,
            kind: "group",
            presence: "group",
            subtitle: group.announcement || group.groupdesc || "group",
            unreadCount: 0,
            latestMessage: "",
            latestTimestamp: "",
        })
    }

    return sessions
}

function parseOfflineMessages(payload: LoginResponsePayload) {
    return (payload.offlinemsg ?? []).map((item) => JSON.parse(item) as ProtocolPushEvent)
}

export function timelineFromHistory(sessionId: string, history: HistoryEntry[]) {
    return history.map((entry) => toTimelineItem({
        msgid: entry.group_id ? GROUP_CHAT_MSG : ONE_CHAT_MSG,
        message_id: entry.id,
        id: entry.sender_id,
        groupid: entry.group_id,
        msg: entry.message,
        time: entry.created_at,
        version: CHAT_PROTOCOL_VERSION,
    }, sessionId, `${entry.sender_id}`)) satisfies TimelineItem[]
}

export function sessionPreviewFromTimeline(items: TimelineItem[]) {
    const latest = items[items.length - 1]
    if (!latest) {
        return { latestMessage: "", latestTimestamp: "" }
    }
    return {
        latestMessage: latest.body,
        latestTimestamp: latest.timestamp,
    }
}

export function buildBootstrapTimeline(payload: LoginResponsePayload) {
    const timeline = new Map<string, TimelineItem[]>()

    for (const event of parseOfflineMessages(payload)) {
        const sessionId = resolveSessionId(event)
        if (!sessionId) {
            continue
        }
        const items = timeline.get(sessionId) ?? []
        items.push(toTimelineItem(event, sessionId))
        timeline.set(sessionId, items)
    }

    return timeline
}

export function resolveSessionId(event: ProtocolPushEvent) {
    if (event.msgid === ONE_CHAT_MSG && event.id) {
        return directSessionId(event.id)
    }
    if (event.msgid === GROUP_CHAT_MSG && event.groupid) {
        return groupSessionId(event.groupid)
    }
    return null
}

function parseAttachmentPayload(raw: string): AttachmentPayload | null {
    try {
        const parsed = JSON.parse(raw) as Partial<AttachmentPayload>
        if ((parsed.kind === "image" || parsed.kind === "file") && typeof parsed.url === "string" && typeof parsed.name === "string") {
            return {
                kind: parsed.kind,
                url: parsed.url,
                name: parsed.name,
                mime: parsed.mime ?? "application/octet-stream",
                size: parsed.size ?? 0,
            }
        }
    } catch {
        return null
    }
    return null
}

export function toTimelineItem(event: ProtocolPushEvent, sessionId: string, authorOverride?: string): TimelineItem {
    const rawBody = event.msg || (event.msgid === RECALL_NOTIFY_MSG ? "message recalled" : "")
    const attachment = rawBody ? parseAttachmentPayload(rawBody) : null
    return {
        id: `${sessionId}-${event.message_id ?? event.request_id ?? Date.now()}`,
        sessionId,
        author: authorOverride || event.name || String(event.id ?? event.operator_id ?? "system"),
        body: attachment ? attachment.name : rawBody,
        timestamp: event.time || new Date().toISOString(),
        recalled: event.msgid === RECALL_NOTIFY_MSG,
        messageId: event.message_id,
        attachment: attachment ?? undefined,
    }
}

export function createProtocolSummary() {
    return JSON.stringify(
        {
            version: CHAT_PROTOCOL_VERSION,
            requests: [
                "LOGIN_MSG",
                "ONE_CHAT_MSG",
                "GROUP_CHAT_MSG",
                "QUERY_HISTORY_MSG",
                "SEARCH_USER_MSG",
                "SET_USER_STATE_MSG",
                "SET_NICKNAME_MSG",
            ],
            guarantees: [
                "request_id based idempotency",
                "shared error code contract",
                "history order asc|desc",
                "blacklist aware messaging",
                "desktop tauri event bridge",
            ],
        },
        null,
        2,
    )
}
