import { invoke } from "@tauri-apps/api/core"
import { listen } from "@tauri-apps/api/event"

import type { HistoryEntry, LoginResponsePayload, ProtocolPushEvent, SearchUserEntry } from "./protocol"

interface BridgeResponse {
    version: number
    msgid: number
    request_id: string
    errno: number
    errmsg: string
    [key: string]: unknown
}

function assertOk<T extends BridgeResponse>(payload: T) {
    if (payload.errno !== 0) {
        throw new Error(payload.errmsg || `Protocol error ${payload.errno}`)
    }
    return payload
}

export async function login(host: string, port: number, userId: number, password: string) {
    const payload = await invoke<LoginResponsePayload & BridgeResponse>("login", { host, port, userId, password })
    return assertOk(payload)
}

export async function logout() {
    const payload = await invoke<BridgeResponse>("logout")
    return assertOk(payload)
}

export async function setNickname(name: string) {
    const payload = await invoke<BridgeResponse>("set_nickname", { name })
    return assertOk(payload)
}

export async function sendDirectMessage(targetId: number, message: string) {
    const payload = await invoke<BridgeResponse>("send_direct_message", { targetId, message })
    return assertOk(payload)
}

export async function sendGroupMessage(groupId: number, message: string) {
    const payload = await invoke<BridgeResponse>("send_group_message", { groupId, message })
    return assertOk(payload)
}

export async function setPresence(state: "online" | "busy") {
    const payload = await invoke<BridgeResponse>("set_presence", { stateName: state })
    return assertOk(payload)
}

export async function searchUsers(keyword: string, limit = 20, offset = 0) {
    const payload = await invoke<BridgeResponse>("search_users", { keyword, limit, offset })
    const response = assertOk(payload)
    const users = Array.isArray(response.users)
        ? response.users.map((item) => JSON.parse(String(item)) as SearchUserEntry)
        : []
    return { response, users }
}

export async function queryDirectHistory(targetId: number, limit = 20, offset = 0, order: "asc" | "desc" = "desc") {
    const payload = await invoke<BridgeResponse>("query_direct_history", { targetId, limit, offset, order })
    const response = assertOk(payload)
    const history = Array.isArray(response.history)
        ? response.history.map((item) => JSON.parse(String(item)) as HistoryEntry)
        : []
    return { response, history }
}

export async function queryGroupHistory(groupId: number, limit = 20, offset = 0, order: "asc" | "desc" = "desc") {
    const payload = await invoke<BridgeResponse>("query_group_history", { groupId, limit, offset, order })
    const response = assertOk(payload)
    const history = Array.isArray(response.history)
        ? response.history.map((item) => JSON.parse(String(item)) as HistoryEntry)
        : []
    return { response, history }
}

export async function sessionInfo() {
    return invoke<Record<string, unknown>>("session_info")
}

export async function subscribeProtocolEvents(handler: (event: ProtocolPushEvent) => void) {
    return listen<ProtocolPushEvent>("protocol-event", (event) => handler(event.payload))
}
