import { invoke } from "@tauri-apps/api/core"
import { listen } from "@tauri-apps/api/event"

import type { HistoryEntry, LoginResponsePayload, ProtocolPushEvent, SearchUserEntry, UserProfile } from "./protocol"

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

export async function addFriend(friendId: number) {
    const payload = await invoke<BridgeResponse>("add_friend", { friendId })
    return assertOk(payload)
}

export async function createGroup(groupName: string, groupDesc: string) {
    const payload = await invoke<BridgeResponse>("create_group", { groupName, groupDesc })
    return assertOk(payload)
}

export async function joinGroup(groupId: number) {
    const payload = await invoke<BridgeResponse>("join_group", { groupId })
    return assertOk(payload)
}

export async function setGroupAnnouncement(groupId: number, announcement: string) {
    const payload = await invoke<BridgeResponse>("set_group_announcement", { groupId, announcement })
    return assertOk(payload)
}

export async function muteGroupMember(groupId: number, targetId: number, minutes: number) {
    const payload = await invoke<BridgeResponse>("mute_group_member", { groupId, targetId, minutes })
    return assertOk(payload)
}

export async function kickGroupMember(groupId: number, targetId: number) {
    const payload = await invoke<BridgeResponse>("kick_group_member", { groupId, targetId })
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

function adminBaseUrl(host: string) {
    return `http://${host}:8010`
}

export async function fetchUserProfile(host: string, userId: number) {
    const response = await fetch(`${adminBaseUrl(host)}/api/v1/users/${userId}`)
    if (!response.ok) {
        throw new Error(`Failed to load user profile ${userId}`)
    }
    return response.json() as Promise<UserProfile>
}

export async function uploadAvatar(host: string, userId: number, file: File) {
    const form = new FormData()
    form.append("avatar", file)
    const response = await fetch(`${adminBaseUrl(host)}/api/v1/users/${userId}/avatar`, {
        method: "POST",
        body: form,
    })
    if (!response.ok) {
        const detail = await response.text()
        throw new Error(detail || "Avatar upload failed")
    }
    return response.json() as Promise<UserProfile>
}

export async function uploadAttachment(host: string, file: File) {
    return uploadAttachmentWithProgress(host, file)
}

export function uploadAttachmentWithProgress(host: string, file: File, onProgress?: (progress: number) => void) {
    return new Promise<{ kind: "image" | "file"; url: string; name: string; mime: string; size: number }>((resolve, reject) => {
        const xhr = new XMLHttpRequest()
        const form = new FormData()
        form.append("attachment", file)

        xhr.open("POST", `${adminBaseUrl(host)}/api/v1/uploads/attachments`)
        xhr.upload.onprogress = (event) => {
            if (event.lengthComputable && onProgress) {
                onProgress(Math.round((event.loaded / event.total) * 100))
            }
        }
        xhr.onload = () => {
            if (xhr.status >= 200 && xhr.status < 300) {
                try {
                    resolve(JSON.parse(xhr.responseText) as { kind: "image" | "file"; url: string; name: string; mime: string; size: number })
                } catch (error) {
                    reject(error)
                }
                return
            }
            reject(new Error(xhr.responseText || "Attachment upload failed"))
        }
        xhr.onerror = () => reject(new Error("Attachment upload failed"))
        xhr.send(form)
    })
}
