export type PresenceState = "online" | "offline" | "busy"

export interface ProtocolEnvelope {
    version: number
    msgid: number
    request_id: string
}

export interface LoginRequest extends ProtocolEnvelope {
    id: number
    password: string
}

export interface SearchUserRequest extends ProtocolEnvelope {
    id: number
    keyword: string
    limit: number
    offset: number
}

export const CHAT_PROTOCOL_VERSION = 1

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
            ],
        },
        null,
        2,
    )
}
