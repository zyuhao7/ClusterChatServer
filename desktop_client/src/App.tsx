import { ChangeEvent, DragEvent, useEffect, useMemo, useState } from "react"

import {
    addFriend,
    createGroup,
    fetchUserProfile,
    joinGroup,
    kickGroupMember,
    login,
    logout,
    muteGroupMember,
    queryDirectHistory,
    queryGroupHistory,
    searchUsers,
    sendDirectMessage,
    sendGroupMessage,
    sessionInfo,
    setGroupAnnouncement,
    setNickname,
    setPresence,
    subscribeProtocolEvents,
    uploadAttachmentWithProgress,
    uploadAvatar,
} from "./lib/bridge"
import { createProtocolSummary, directSessionId, groupSessionId, type AttachmentPayload } from "./lib/protocol"
import { isTauriRuntime } from "./lib/tauri"
import { useSessionStore } from "./features/session/store"

interface AttachmentDraft {
    id: string
    file: File
    previewUrl: string
    progress: number
    error: string
}

function formatBytes(size: number) {
    if (size < 1024) {
        return `${size} B`
    }
    if (size < 1024 * 1024) {
        return `${(size / 1024).toFixed(1)} KB`
    }
    return `${(size / (1024 * 1024)).toFixed(1)} MB`
}

function attachmentIcon(kind: "image" | "file", mime: string) {
    if (kind === "image") {
        return "IMG"
    }
    if (mime.includes("pdf")) {
        return "PDF"
    }
    if (mime.includes("zip") || mime.includes("compressed")) {
        return "ZIP"
    }
    if (mime.includes("text")) {
        return "TXT"
    }
    return "FILE"
}

function toDraft(file: File): AttachmentDraft {
    return {
        id: `${file.name}-${file.size}-${Date.now()}-${Math.random().toString(16).slice(2)}`,
        file,
        previewUrl: file.type.startsWith("image/") ? URL.createObjectURL(file) : "",
        progress: 0,
        error: "",
    }
}

export default function App() {
    const store = useSessionStore()
    const [pending, setPending] = useState<string | null>(null)
    const [sessionFilter, setSessionFilter] = useState("")
    const [memberFilter, setMemberFilter] = useState("")
    const [attachmentFilter, setAttachmentFilter] = useState<"all" | "images" | "files">("all")
    const [attachmentSearch, setAttachmentSearch] = useState("")
    const [newGroupName, setNewGroupName] = useState("")
    const [newGroupDesc, setNewGroupDesc] = useState("")
    const [joinGroupId, setJoinGroupId] = useState("")
    const [announcementDraft, setAnnouncementDraft] = useState("")
    const [muteTargetId, setMuteTargetId] = useState("")
    const [muteMinutes, setMuteMinutes] = useState("10")
    const [kickTargetId, setKickTargetId] = useState("")
    const [avatarFile, setAvatarFile] = useState<File | null>(null)
    const [avatarPreviewUrl, setAvatarPreviewUrl] = useState("")
    const [avatarFileName, setAvatarFileName] = useState("")
    const [attachmentDrafts, setAttachmentDrafts] = useState<AttachmentDraft[]>([])
    const [isDragActive, setIsDragActive] = useState(false)
    const [lightboxAttachment, setLightboxAttachment] = useState<AttachmentPayload | null>(null)
    const [cachedAttachments, setCachedAttachments] = useState<Record<string, string>>({})
    const [downloadStates, setDownloadStates] = useState<Record<string, string>>({})

    const protocolSummary = useMemo(() => createProtocolSummary(), [])
    const sortedSessions = useMemo(() => {
        const filtered = [...store.sessions].filter((session) => {
            if (!sessionFilter.trim()) {
                return true
            }
            const keyword = sessionFilter.toLowerCase()
            return session.title.toLowerCase().includes(keyword) || session.subtitle.toLowerCase().includes(keyword)
        })

        return filtered.sort((a, b) => {
            const aPinned = store.pinnedSessionIds.includes(a.sessionId)
            const bPinned = store.pinnedSessionIds.includes(b.sessionId)
            if (aPinned !== bPinned) {
                return aPinned ? -1 : 1
            }
            return b.latestTimestamp.localeCompare(a.latestTimestamp)
        })
    }, [sessionFilter, store.pinnedSessionIds, store.sessions])
    const sessionGroups = useMemo(() => ({
        pinned: sortedSessions.filter((session) => store.pinnedSessionIds.includes(session.sessionId)),
        direct: sortedSessions.filter((session) => session.kind === "direct" && !store.pinnedSessionIds.includes(session.sessionId)),
        group: sortedSessions.filter((session) => session.kind === "group" && !store.pinnedSessionIds.includes(session.sessionId)),
    }), [sortedSessions, store.pinnedSessionIds])
    const recentContacts = useMemo(
        () => sortedSessions.filter((session) => session.kind === "direct").slice(0, 5),
        [sortedSessions],
    )
    const selectedSession = sortedSessions.find((session) => session.sessionId === store.selectedSessionId) ?? null
    const currentTimelineRaw = selectedSession ? store.timelines[selectedSession.sessionId] ?? [] : []
    const directProfile = selectedSession?.kind === "direct" ? store.friends[selectedSession.rawId] ?? null : null
    const groupProfile = selectedSession?.kind === "group" ? store.groups[selectedSession.rawId] ?? null : null
    const filteredMembers = useMemo(() => {
        if (!groupProfile) {
            return []
        }
        if (!memberFilter.trim()) {
            return groupProfile.users
        }
        const keyword = memberFilter.toLowerCase()
        return groupProfile.users.filter((member) => member.name.toLowerCase().includes(keyword) || String(member.id).includes(keyword))
    }, [groupProfile, memberFilter])
    const currentTimeline = useMemo(() => {
        return currentTimelineRaw.filter((entry) => {
            if (attachmentFilter === "images" && entry.attachment?.kind !== "image") {
                return false
            }
            if (attachmentFilter === "files" && entry.attachment?.kind !== "file") {
                return false
            }
            if (attachmentSearch.trim()) {
                const keyword = attachmentSearch.toLowerCase()
                const haystack = entry.attachment ? `${entry.attachment.name} ${entry.attachment.mime}`.toLowerCase() : entry.body.toLowerCase()
                return haystack.includes(keyword)
            }
            return attachmentFilter === "all" ? true : Boolean(entry.attachment)
        })
    }, [attachmentFilter, attachmentSearch, currentTimelineRaw])
    const attachmentLibrary = useMemo(() => {
        return Object.values(store.timelines)
            .flat()
            .filter((entry) => entry.attachment)
            .map((entry) => ({
                id: entry.id,
                sessionId: entry.sessionId,
                author: entry.author,
                timestamp: entry.timestamp,
                attachment: entry.attachment!,
            }))
            .sort((a, b) => b.timestamp.localeCompare(a.timestamp))
    }, [store.timelines])
    const groupStats = useMemo(() => {
        if (!groupProfile) {
            return null
        }
        const adminCount = groupProfile.users.filter((user) => user.role === "admin").length
        const mutedCount = groupProfile.users.filter((user) => Boolean(user.muted_until)).length
        return {
            memberCount: groupProfile.users.length,
            adminCount,
            mutedCount,
        }
    }, [groupProfile])

    function renderSessionSection(title: string, sessions: typeof sortedSessions) {
        if (sessions.length === 0) {
            return null
        }
        return (
            <section className="session-section">
                <p className="eyebrow">{title}</p>
                <div className="session-list">
                    {sessions.map((session) => (
                        <button
                            key={session.sessionId}
                            className={store.selectedSessionId === session.sessionId ? "session session-active" : "session"}
                            onClick={() => store.setSelectedSessionId(session.sessionId)}
                            type="button"
                        >
                            <div className="session-head-row">
                                <span className="session-title">{session.title}</span>
                                <div className="button-row compact-row">
                                    {store.pinnedSessionIds.includes(session.sessionId) ? <span className="badge">Pinned</span> : null}
                                    {session.unreadCount > 0 ? <span className="badge badge-unread">{session.unreadCount}</span> : null}
                                </div>
                            </div>
                            <span className="session-meta">{session.kind} · {session.subtitle}</span>
                            {session.latestMessage ? <span className="session-preview">{session.latestMessage}</span> : null}
                        </button>
                    ))}
                </div>
            </section>
        )
    }

    useEffect(() => {
        if (!isTauriRuntime()) {
            return
        }

        void sessionInfo().then((info) => {
            useSessionStore.getState().setLastResponse(JSON.stringify(info, null, 2))
        })

        let disposed = false
        let cleanup: (() => void) | null = null

        void subscribeProtocolEvents((event) => {
            if (!disposed) {
                useSessionStore.getState().applyProtocolEvent(event)
            }
        }).then((unlisten) => {
            if (disposed) {
                void unlisten()
            } else {
                cleanup = unlisten
            }
        })

        return () => {
            disposed = true
            cleanup?.()
        }
    }, [])

    useEffect(() => {
        if (groupProfile) {
            setAnnouncementDraft(groupProfile.announcement ?? "")
        }
    }, [groupProfile?.id, groupProfile?.announcement])

    useEffect(() => {
        if (!store.loggedInUserId) {
            return
        }

        void fetchUserProfile(store.host, store.loggedInUserId)
            .then((profile) => {
                const avatarUrl = profile.avatar_url ? `http://${store.host}:8010${profile.avatar_url}` : ""
                store.setLoggedInUserAvatarUrl(avatarUrl)
            })
            .catch(() => undefined)
    }, [store.host, store.loggedInUserId])

    useEffect(() => {
        if (!selectedSession || selectedSession.kind !== "direct") {
            return
        }

        void fetchUserProfile(store.host, selectedSession.rawId)
            .then((profile) => {
                const avatarUrl = profile.avatar_url ? `http://${store.host}:8010${profile.avatar_url}` : ""
                useSessionStore.getState().updateFriendProfile(selectedSession.rawId, avatarUrl)
            })
            .catch(() => undefined)
    }, [store.host, selectedSession?.kind, selectedSession?.rawId])

    async function runAction(label: string, action: () => Promise<void>) {
        setPending(label)
        try {
            await action()
        } catch (error) {
            store.setLastResponse(error instanceof Error ? error.message : String(error))
        } finally {
            setPending(null)
        }
    }

    function addDraftFiles(files: FileList | File[]) {
        const nextDrafts = Array.from(files).map(toDraft)
        if (nextDrafts.length === 0) {
            return
        }
        setAttachmentDrafts((current) => [...current, ...nextDrafts])
        store.setLastResponse(`Selected ${nextDrafts.length} attachment(s)`) 
    }

    function handleAvatarChange(event: ChangeEvent<HTMLInputElement>) {
        const file = event.target.files?.[0]
        if (!file) {
            return
        }
        setAvatarFile(file)
        setAvatarFileName(file.name)
        setAvatarPreviewUrl(URL.createObjectURL(file))
        store.setLastResponse(`Selected avatar draft: ${file.name}`)
    }

    function handleAttachmentChange(event: ChangeEvent<HTMLInputElement>) {
        if (!event.target.files) {
            return
        }
        addDraftFiles(event.target.files)
        event.target.value = ""
    }

    function removeAttachmentDraft(draftId: string) {
        setAttachmentDrafts((current) => current.filter((draft) => draft.id !== draftId))
    }

    function updateDraft(draftId: string, updater: (draft: AttachmentDraft) => AttachmentDraft) {
        setAttachmentDrafts((current) => current.map((draft) => (draft.id === draftId ? updater(draft) : draft)))
    }

    function handleDragOver(event: DragEvent<HTMLDivElement>) {
        event.preventDefault()
        setIsDragActive(true)
    }

    function handleDragLeave(event: DragEvent<HTMLDivElement>) {
        event.preventDefault()
        setIsDragActive(false)
    }

    function handleDrop(event: DragEvent<HTMLDivElement>) {
        event.preventDefault()
        setIsDragActive(false)
        addDraftFiles(Array.from(event.dataTransfer.files))
    }

    async function cacheAttachment(attachment: AttachmentPayload) {
        const cached = cachedAttachments[attachment.url]
        if (cached) {
            return cached
        }

        setDownloadStates((current) => ({ ...current, [attachment.url]: "downloading" }))
        const response = await fetch(attachment.url)
        if (!response.ok) {
            setDownloadStates((current) => ({ ...current, [attachment.url]: "error" }))
            throw new Error(`Download failed: ${attachment.name}`)
        }
        const blob = await response.blob()
        const objectUrl = URL.createObjectURL(blob)
        setCachedAttachments((current) => ({ ...current, [attachment.url]: objectUrl }))
        setDownloadStates((current) => ({ ...current, [attachment.url]: "cached" }))
        return objectUrl
    }

    async function sendCurrentMessage(retryOnlyFailed = false) {
        if (!selectedSession) {
            return
        }

        const timestamp = new Date().toISOString().replace("T", " ").slice(0, 19)
        const text = store.composerText.trim()
        if (!retryOnlyFailed && text) {
            if (selectedSession.kind === "direct") {
                const response = await sendDirectMessage(selectedSession.rawId, text)
                store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", text, timestamp, Number(response.message_id ?? 0))
                store.setLastResponse(JSON.stringify(response, null, 2))
            } else {
                const response = await sendGroupMessage(selectedSession.rawId, text)
                store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", text, timestamp, Number(response.message_id ?? 0))
                store.setLastResponse(JSON.stringify(response, null, 2))
            }
        }

        const drafts = attachmentDrafts.filter((draft) => !retryOnlyFailed || draft.error)
        for (const draft of drafts) {
            updateDraft(draft.id, (item) => ({ ...item, error: "", progress: 0 }))
            try {
                const uploaded = await uploadAttachmentWithProgress(store.host, draft.file, (progress) => {
                    updateDraft(draft.id, (item) => ({ ...item, progress }))
                })
                const payload = JSON.stringify(uploaded)
                if (selectedSession.kind === "direct") {
                    const response = await sendDirectMessage(selectedSession.rawId, payload)
                    store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", payload, timestamp, Number(response.message_id ?? 0))
                    store.setLastResponse(JSON.stringify(response, null, 2))
                } else {
                    const response = await sendGroupMessage(selectedSession.rawId, payload)
                    store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", payload, timestamp, Number(response.message_id ?? 0))
                    store.setLastResponse(JSON.stringify(response, null, 2))
                }
                removeAttachmentDraft(draft.id)
            } catch (error) {
                updateDraft(draft.id, (item) => ({
                    ...item,
                    error: error instanceof Error ? error.message : String(error),
                    progress: 0,
                }))
                throw error
            }
        }
    }

    return (
        <div className="app-shell">
            <aside className="panel sidebar">
                <div>
                    <p className="eyebrow">ClusterChat Desktop</p>
                    <h1>Live Sessions</h1>
                    <p className="muted">
                        {isTauriRuntime() ? "Running inside Tauri shell" : "Running in browser preview mode"}
                    </p>
                </div>

                <section>
                    <h2>Server</h2>
                    <label className="field">
                        <span>Host</span>
                        <input value={store.host} onChange={(event) => store.setField("host", event.target.value)} />
                    </label>
                    <label className="field">
                        <span>Port</span>
                        <input value={store.port} onChange={(event) => store.setField("port", Number(event.target.value) || 0)} />
                    </label>
                </section>

                <section>
                    <h2>Login</h2>
                    <label className="field">
                        <span>User ID</span>
                        <input value={store.userIdInput} onChange={(event) => store.setField("userIdInput", event.target.value)} />
                    </label>
                    <label className="field">
                        <span>Password</span>
                        <input type="password" value={store.password} onChange={(event) => store.setField("password", event.target.value)} />
                    </label>
                    <div className="button-row">
                        <button
                            onClick={() =>
                                runAction("login", async () => {
                                    const response = await login(store.host, store.port, Number(store.userIdInput), store.password)
                                    store.setLoggedInUser(response.id, response.name)
                                    store.setPresence("online")
                                    store.bootstrapFromLogin(response)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            Login
                        </button>
                        <button
                            onClick={() =>
                                runAction("logout", async () => {
                                    const response = await logout()
                                    store.resetSession()
                                    setAvatarFile(null)
                                    setAvatarPreviewUrl("")
                                    setAvatarFileName("")
                                    setAttachmentDrafts([])
                                    setLightboxAttachment(null)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            Logout
                        </button>
                    </div>
                </section>

                <section>
                    <h2>Sessions</h2>
                    <label className="field">
                        <span>Search sessions</span>
                        <input value={sessionFilter} onChange={(event) => setSessionFilter(event.target.value)} placeholder="Search session title or subtitle" />
                    </label>
                    {sortedSessions.length === 0 ? <p className="muted">No sessions yet.</p> : null}
                    {renderSessionSection("Pinned", sessionGroups.pinned)}
                    {renderSessionSection("Direct", sessionGroups.direct)}
                    {renderSessionSection("Groups", sessionGroups.group)}
                </section>

                <section className="card-section">
                    <p className="eyebrow">Recent Contacts</p>
                    <div className="recent-contact-list">
                        {recentContacts.length === 0 ? <p className="muted">No recent direct sessions.</p> : null}
                        {recentContacts.map((session) => (
                            <button key={session.sessionId} className="recent-contact" onClick={() => store.setSelectedSessionId(session.sessionId)} type="button">
                                <strong>{session.title}</strong>
                                <span>{session.subtitle}</span>
                            </button>
                        ))}
                    </div>
                </section>

                <section className="card-section">
                    <p className="eyebrow">Create Or Join Group</p>
                    <label className="field">
                        <span>Group name</span>
                        <input value={newGroupName} onChange={(event) => setNewGroupName(event.target.value)} />
                    </label>
                    <label className="field">
                        <span>Group description</span>
                        <input value={newGroupDesc} onChange={(event) => setNewGroupDesc(event.target.value)} />
                    </label>
                    <button onClick={() => runAction("create group", async () => {
                        const response = await createGroup(newGroupName, newGroupDesc)
                        store.upsertGroupSession(Number(response.groupid), newGroupName || `Group ${response.groupid}`, newGroupDesc)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">Create Group</button>
                    <label className="field">
                        <span>Join group id</span>
                        <input value={joinGroupId} onChange={(event) => setJoinGroupId(event.target.value)} />
                    </label>
                    <button onClick={() => runAction("join group", async () => {
                        const response = await joinGroup(Number(joinGroupId))
                        store.upsertGroupSession(Number(joinGroupId), `Group ${joinGroupId}`, "joined group")
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">Join Group</button>
                </section>
            </aside>

            <main className="panel content">
                <section className="hero">
                    <div>
                        <p className="eyebrow">Current Session</p>
                        <h2>{selectedSession?.title || store.loggedInUserName || "No active session"}</h2>
                        <p className="muted">
                            {selectedSession?.subtitle || "Login and select a session to see live timeline updates."}
                        </p>
                    </div>
                    <div className="badge-cluster">
                        <span className="badge">presence: {store.presence}</span>
                        <span className="badge">{pending ? `running ${pending}` : "idle"}</span>
                    </div>
                </section>

                <section className="timeline timeline-scroll">
                    <div className="timeline-toolbar">
                        <div className="button-row compact-row">
                            <button className={attachmentFilter === "all" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("all")} type="button">All</button>
                            <button className={attachmentFilter === "images" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("images")} type="button">Images</button>
                            <button className={attachmentFilter === "files" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("files")} type="button">Files</button>
                        </div>
                        <input value={attachmentSearch} onChange={(event) => setAttachmentSearch(event.target.value)} placeholder="Filter attachment history" />
                    </div>
                    {currentTimeline.length === 0 ? <p className="muted">No messages for the selected session yet.</p> : null}
                    {currentTimeline.map((entry) => (
                        <article key={entry.id} className={entry.recalled ? "message-card message-recalled" : "message-card"}>
                            <div className="message-head">
                                <strong>{entry.author}</strong>
                                <span>{entry.timestamp}</span>
                            </div>
                            {entry.attachment ? (
                                <div className="attachment-card">
                                    <div className="attachment-head">
                                        <span className="attachment-icon">{attachmentIcon(entry.attachment.kind, entry.attachment.mime)}</span>
                                        <div className="attachment-meta">
                                            <strong>{entry.attachment.name}</strong>
                                            <span>{entry.attachment.mime} · {formatBytes(entry.attachment.size)}</span>
                                        </div>
                                    </div>
                                    {entry.attachment.kind === "image" ? (
                                        <img
                                            alt={entry.attachment.name}
                                            className="attachment-image clickable"
                                            onClick={() => setLightboxAttachment(entry.attachment ?? null)}
                                            src={cachedAttachments[entry.attachment.url] ?? entry.attachment.url}
                                        />
                                    ) : null}
                                    <div className="button-row compact-row">
                                        <button
                                            onClick={() =>
                                                runAction("download attachment", async () => {
                                                    const url = await cacheAttachment(entry.attachment!)
                                                    window.open(url, "_blank", "noopener,noreferrer")
                                                })
                                            }
                                            type="button"
                                        >
                                            {downloadStates[entry.attachment.url] === "cached" ? "Open Cached" : "Download"}
                                        </button>
                                        {entry.attachment.kind === "image" ? (
                                            <button onClick={() => setLightboxAttachment(entry.attachment ?? null)} type="button">Preview</button>
                                        ) : null}
                                    </div>
                                    {downloadStates[entry.attachment.url] === "downloading" ? <span className="muted">Downloading...</span> : null}
                                    {downloadStates[entry.attachment.url] === "error" ? <span className="error-text">Download failed</span> : null}
                                </div>
                            ) : (
                                <p>{entry.body}</p>
                            )}
                        </article>
                    ))}
                </section>

                <section
                    className={isDragActive ? "composer-box composer-box-active" : "composer-box"}
                    onDragLeave={handleDragLeave}
                    onDragOver={handleDragOver}
                    onDrop={handleDrop}
                >
                    <textarea
                        className="composer-input"
                        placeholder={selectedSession ? "Type a message or drop files here..." : "Select a session first"}
                        value={store.composerText}
                        onChange={(event) => store.setField("composerText", event.target.value)}
                    />
                    <div className="button-row">
                        <label className="button-like">
                            <span>Choose Attachments</span>
                            <input className="hidden-input" multiple type="file" onChange={handleAttachmentChange} />
                        </label>
                        <button
                            disabled={!selectedSession || (!store.composerText.trim() && attachmentDrafts.length === 0)}
                            onClick={() =>
                                runAction("send message", async () => {
                                    await sendCurrentMessage()
                                })
                            }
                            type="button"
                        >
                            Send Message
                        </button>
                        <button onClick={() => store.setField("composerText", "")} type="button">Clear</button>
                        <button
                            disabled={!selectedSession || attachmentDrafts.every((draft) => !draft.error)}
                            onClick={() =>
                                runAction("retry attachment", async () => {
                                    await sendCurrentMessage(true)
                                })
                            }
                            type="button"
                        >
                            Retry Failed
                        </button>
                    </div>
                    {attachmentDrafts.length > 0 ? (
                        <div className="draft-grid">
                            {attachmentDrafts.map((draft) => (
                                <div key={draft.id} className="draft-card">
                                    {draft.previewUrl ? <img alt={draft.file.name} className="draft-image" src={draft.previewUrl} /> : null}
                                    <strong>{draft.file.name}</strong>
                                    <span>{formatBytes(draft.file.size)}</span>
                                    <span>{draft.progress > 0 ? `Upload ${draft.progress}%` : "Waiting"}</span>
                                    {draft.error ? <span className="error-text">{draft.error}</span> : null}
                                    <button onClick={() => removeAttachmentDraft(draft.id)} type="button">Remove</button>
                                </div>
                            ))}
                        </div>
                    ) : null}
                </section>
            </main>

            <aside className="panel inspector">
                <section className="card-section">
                    <p className="eyebrow">Profile Actions</p>
                    <div className="detail-card">
                        <strong>{store.loggedInUserName || "Guest"}</strong>
                        <span>User ID: {store.loggedInUserId ?? "-"}</span>
                        <span>Presence: {store.presence}</span>
                        <span>Server: {store.host}:{store.port}</span>
                    </div>
                    <div className="button-row">
                        <button onClick={() => runAction("set busy", async () => {
                            const response = await setPresence("busy")
                            store.setPresence("busy")
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Set Busy</button>
                        <button onClick={() => runAction("set online", async () => {
                            const response = await setPresence("online")
                            store.setPresence("online")
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Set Online</button>
                    </div>
                    <label className="field">
                        <span>Nickname</span>
                        <input value={store.loggedInUserName} onChange={(event) => store.setField("loggedInUserName", event.target.value)} />
                    </label>
                    <button onClick={() => runAction("set nickname", async () => {
                        const response = await setNickname(store.loggedInUserName)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">Update Nickname</button>
                    <label className="field">
                        <span>Avatar draft</span>
                        <input type="file" accept="image/*" onChange={handleAvatarChange} />
                    </label>
                    <button
                        disabled={!store.loggedInUserId || !avatarFile}
                        onClick={() =>
                            runAction("upload avatar", async () => {
                                if (!avatarFile || !store.loggedInUserId) {
                                    return
                                }
                                const profile = await uploadAvatar(store.host, store.loggedInUserId, avatarFile)
                                const avatarUrl = profile.avatar_url ? `http://${store.host}:8010${profile.avatar_url}` : ""
                                store.setLoggedInUserAvatarUrl(avatarUrl)
                                store.setLastResponse(JSON.stringify(profile, null, 2))
                            })
                        }
                        type="button"
                    >
                        Upload Avatar
                    </button>
                    {store.loggedInUserAvatarUrl ? <img alt="current avatar" className="avatar-preview" src={store.loggedInUserAvatarUrl} /> : null}
                    {avatarPreviewUrl ? <img alt="avatar preview" className="avatar-preview" src={avatarPreviewUrl} /> : null}
                    {avatarFileName ? <p className="muted">Selected file: {avatarFileName}</p> : null}
                    <button
                        disabled={!avatarFile && !avatarPreviewUrl}
                        onClick={() => {
                            setAvatarFile(null)
                            setAvatarPreviewUrl("")
                            setAvatarFileName("")
                        }}
                        type="button"
                    >
                        Reset Avatar Draft
                    </button>
                </section>

                <section className="card-section">
                    <p className="eyebrow">Session Card</p>
                    {!selectedSession ? <p className="muted">Select a session to view details.</p> : null}
                    {directProfile ? (
                        <div className="detail-card">
                            <strong>{directProfile.name}</strong>
                            {directProfile.avatar_url ? <img alt="friend avatar" className="avatar-preview avatar-preview-small" src={directProfile.avatar_url} /> : null}
                            <span>ID: {directProfile.id}</span>
                            <span>State: {directProfile.state}</span>
                            {selectedSession ? (
                                <button onClick={() => store.togglePinnedSession(selectedSession.sessionId)} type="button">
                                    {store.pinnedSessionIds.includes(selectedSession.sessionId) ? "Unpin Session" : "Pin Session"}
                                </button>
                            ) : null}
                            <button onClick={() => runAction("add friend", async () => {
                                const response = await addFriend(directProfile.id)
                                store.markFriend({
                                    id: directProfile.id,
                                    name: directProfile.name,
                                    state: directProfile.state,
                                    is_friend: true,
                                    has_blocked: false,
                                    blocked_by_target: false,
                                })
                                store.setLastResponse(JSON.stringify(response, null, 2))
                            })} type="button">Add Friend</button>
                        </div>
                    ) : null}
                    {groupProfile ? (
                        <div className="detail-card">
                            <strong>{groupProfile.groupname}</strong>
                            <span>ID: {groupProfile.id}</span>
                            <span>{groupProfile.groupdesc}</span>
                            {groupProfile.announcement ? <span>Announcement: {groupProfile.announcement}</span> : null}
                            <span>Members: {groupStats?.memberCount ?? groupProfile.users.length}</span>
                            <span>Admins: {groupStats?.adminCount ?? 0}</span>
                            <span>Muted: {groupStats?.mutedCount ?? 0}</span>
                            {selectedSession ? (
                                <button onClick={() => store.togglePinnedSession(selectedSession.sessionId)} type="button">
                                    {store.pinnedSessionIds.includes(selectedSession.sessionId) ? "Unpin Session" : "Pin Session"}
                                </button>
                            ) : null}
                        </div>
                    ) : null}
                </section>

                <section className="card-section">
                    <p className="eyebrow">Search User</p>
                    <label className="field">
                        <span>Keyword</span>
                        <input value={store.searchKeyword} onChange={(event) => store.setField("searchKeyword", event.target.value)} />
                    </label>
                    <button onClick={() => runAction("search users", async () => {
                        const { response, users } = await searchUsers(store.searchKeyword)
                        store.setSearchResults(users)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">Search</button>
                    <div className="result-list">
                        {store.searchResults.map((user) => (
                            <article key={user.id} className="result-card">
                                <strong>{user.name}</strong>
                                <span>{user.state}</span>
                                <span>friend: {String(user.is_friend)}</span>
                                <div className="button-row compact-row">
                                    <button onClick={() => store.upsertSearchSession(user)} type="button">Open Session</button>
                                    <button onClick={() => runAction("add friend", async () => {
                                        const response = await addFriend(user.id)
                                        store.markFriend(user)
                                        store.setLastResponse(JSON.stringify(response, null, 2))
                                    })} type="button">Add Friend</button>
                                </div>
                            </article>
                        ))}
                    </div>
                </section>

                {groupProfile ? (
                    <section className="card-section">
                        <p className="eyebrow">Group Moderation</p>
                        <label className="field">
                            <span>Announcement</span>
                            <textarea value={announcementDraft} onChange={(event) => setAnnouncementDraft(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("set announcement", async () => {
                            const response = await setGroupAnnouncement(groupProfile.id, announcementDraft)
                            store.updateGroupAnnouncement(groupProfile.id, announcementDraft)
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Update Announcement</button>
                        <label className="field">
                            <span>Filter members</span>
                            <input value={memberFilter} onChange={(event) => setMemberFilter(event.target.value)} placeholder="Search by name or ID" />
                        </label>
                        <div className="member-list">
                            {filteredMembers.map((member) => (
                                <div key={member.id} className="detail-card member-card member-card-interactive">
                                    <strong>{member.name}</strong>
                                    <span>ID: {member.id}</span>
                                    <span>Role: {member.role}</span>
                                    {member.muted_until ? <span>Muted until: {member.muted_until}</span> : null}
                                    <div className="button-row compact-row">
                                        <button onClick={() => setMuteTargetId(String(member.id))} type="button">Pick For Mute</button>
                                        <button onClick={() => setKickTargetId(String(member.id))} type="button">Pick For Kick</button>
                                    </div>
                                </div>
                            ))}
                        </div>
                        <label className="field">
                            <span>Mute target ID</span>
                            <input value={muteTargetId} onChange={(event) => setMuteTargetId(event.target.value)} />
                        </label>
                        <label className="field">
                            <span>Minutes</span>
                            <input value={muteMinutes} onChange={(event) => setMuteMinutes(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("mute member", async () => {
                            const response = await muteGroupMember(groupProfile.id, Number(muteTargetId), Number(muteMinutes))
                            store.updateGroupMemberMute(groupProfile.id, Number(muteTargetId), String(response.muted_until ?? ""))
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Mute Member</button>
                        <label className="field">
                            <span>Kick target ID</span>
                            <input value={kickTargetId} onChange={(event) => setKickTargetId(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("kick member", async () => {
                            const response = await kickGroupMember(groupProfile.id, Number(kickTargetId))
                            store.removeGroupMember(groupProfile.id, Number(kickTargetId))
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Kick Member</button>
                    </section>
                ) : null}

                <section className="card-section">
                    <p className="eyebrow">History Sync</p>
                    <label className="field">
                        <span>Order</span>
                        <select value={store.historyOrder} onChange={(event) => store.setField("historyOrder", event.target.value as "asc" | "desc") }>
                            <option value="desc">desc</option>
                            <option value="asc">asc</option>
                        </select>
                    </label>
                    <div className="button-row">
                        <button
                            disabled={!selectedSession || selectedSession.kind !== "direct"}
                            onClick={() =>
                                runAction("direct history", async () => {
                                    if (!selectedSession) {
                                        return
                                    }
                                    const { response, history } = await queryDirectHistory(selectedSession.rawId, 20, 0, store.historyOrder)
                                    store.replaceHistory(directSessionId(selectedSession.rawId), history)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            Sync Direct
                        </button>
                        <button
                            disabled={!selectedSession || selectedSession.kind !== "group"}
                            onClick={() =>
                                runAction("group history", async () => {
                                    if (!selectedSession) {
                                        return
                                    }
                                    const { response, history } = await queryGroupHistory(selectedSession.rawId, 20, 0, store.historyOrder)
                                    store.replaceHistory(groupSessionId(selectedSession.rawId), history)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            Sync Group
                        </button>
                    </div>
                </section>

                <section>
                    <p className="eyebrow">Protocol Layer</p>
                    <pre className="protocol-box">{protocolSummary}</pre>
                </section>
                <section>
                    <p className="eyebrow">Last Response</p>
                    <pre className="protocol-box">{store.lastResponse || "No response yet."}</pre>
                </section>
                <section className="card-section">
                    <p className="eyebrow">Download Manager</p>
                    <div className="download-list">
                        {attachmentLibrary.length === 0 ? <p className="muted">No attachment history yet.</p> : null}
                        {attachmentLibrary.slice(0, 8).map((item) => (
                            <div key={item.id} className="download-item">
                                <div>
                                    <strong>{item.attachment.name}</strong>
                                    <div className="muted small-text">{item.author} · {item.timestamp}</div>
                                </div>
                                <div className="button-row compact-row">
                                    <span className="badge">{downloadStates[item.attachment.url] ?? "remote"}</span>
                                    <button
                                        onClick={() =>
                                            runAction("download attachment", async () => {
                                                const url = await cacheAttachment(item.attachment)
                                                window.open(url, "_blank", "noopener,noreferrer")
                                            })
                                        }
                                        type="button"
                                    >
                                        {cachedAttachments[item.attachment.url] ? "Open" : "Cache"}
                                    </button>
                                </div>
                            </div>
                        ))}
                    </div>
                </section>
            </aside>

            {lightboxAttachment ? (
                <div className="lightbox" onClick={() => setLightboxAttachment(null)} role="presentation">
                    <div className="lightbox-content" onClick={(event) => event.stopPropagation()} role="presentation">
                        <div className="button-row">
                            <strong>{lightboxAttachment.name}</strong>
                            <button onClick={() => setLightboxAttachment(null)} type="button">Close</button>
                        </div>
                        <img alt={lightboxAttachment.name} className="lightbox-image" src={cachedAttachments[lightboxAttachment.url] ?? lightboxAttachment.url} />
                    </div>
                </div>
            ) : null}
        </div>
    )
}
