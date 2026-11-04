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
    setGroupProfile,
    setNickname,
    setPresence,
    subscribeProtocolEvents,
    uploadAttachmentWithProgress,
    uploadAvatar,
    updateUserProfile,
} from "./lib/bridge"
import { createProtocolSummary, directSessionId, groupSessionId, presenceLabel, roleLabel, sessionKindLabel, type AttachmentPayload } from "./lib/protocol"
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

function downloadStateLabel(state: string | undefined) {
    switch (state) {
        case "remote":
            return "未下载"
        case "downloading":
            return "下载中"
        case "error":
            return "下载失败"
        case "cached":
            return "已缓存"
        default:
            return state ?? "未下载"
    }
}

function yesNo(value: boolean) {
    return value ? "是" : "否"
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
    const [groupProfileName, setGroupProfileName] = useState("")
    const [groupProfileDesc, setGroupProfileDesc] = useState("")
    const [announcementDraft, setAnnouncementDraft] = useState("")
    const [muteTargetId, setMuteTargetId] = useState("")
    const [muteMinutes, setMuteMinutes] = useState("10")
    const [kickTargetId, setKickTargetId] = useState("")
    const [bioDraft, setBioDraft] = useState("")
    const [locationDraft, setLocationDraft] = useState("")
    const [avatarFile, setAvatarFile] = useState<File | null>(null)
    const [avatarPreviewUrl, setAvatarPreviewUrl] = useState("")
    const [avatarFileName, setAvatarFileName] = useState("")
    const [attachmentDrafts, setAttachmentDrafts] = useState<AttachmentDraft[]>([])
    const [isDragActive, setIsDragActive] = useState(false)
    const [lightboxAttachment, setLightboxAttachment] = useState<AttachmentPayload | null>(null)
    const [cachedAttachments, setCachedAttachments] = useState<Record<string, string>>({})
    const [downloadStates, setDownloadStates] = useState<Record<string, string>>({})
    const [downloadHistory, setDownloadHistory] = useState<Array<{ url: string; name: string; timestamp: string }>>([])

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
        unread: sortedSessions.filter((session) => session.unreadCount > 0 && !store.pinnedSessionIds.includes(session.sessionId)),
        direct: sortedSessions.filter((session) => session.kind === "direct" && session.unreadCount === 0 && !store.pinnedSessionIds.includes(session.sessionId)),
        group: sortedSessions.filter((session) => session.kind === "group" && session.unreadCount === 0 && !store.pinnedSessionIds.includes(session.sessionId)),
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
                                    {store.pinnedSessionIds.includes(session.sessionId) ? <span className="badge">已置顶</span> : null}
                                    {session.unreadCount > 0 ? <span className="badge badge-unread">{session.unreadCount}</span> : null}
                                </div>
                            </div>
                            <span className="session-meta">{sessionKindLabel(session.kind)} · {session.subtitle}</span>
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
            setGroupProfileName(groupProfile.groupname)
            setGroupProfileDesc(groupProfile.groupdesc)
        }
    }, [groupProfile?.id, groupProfile?.announcement, groupProfile?.groupdesc, groupProfile?.groupname])

    useEffect(() => {
        const pinned = localStorage.getItem("clusterchat.desktop.pinnedSessions")
        const downloads = localStorage.getItem("clusterchat.desktop.downloadHistory")
        if (pinned) {
            try {
                store.restorePinnedSessions(JSON.parse(pinned) as string[])
            } catch {
                // ignore invalid local state
            }
        }
        if (downloads) {
            try {
                setDownloadHistory(JSON.parse(downloads) as Array<{ url: string; name: string; timestamp: string }>)
            } catch {
                // ignore invalid local state
            }
        }
    }, [])

    useEffect(() => {
        localStorage.setItem("clusterchat.desktop.pinnedSessions", JSON.stringify(store.pinnedSessionIds))
    }, [store.pinnedSessionIds])

    useEffect(() => {
        localStorage.setItem("clusterchat.desktop.downloadHistory", JSON.stringify(downloadHistory))
    }, [downloadHistory])

    useEffect(() => {
        if (!store.loggedInUserId) {
            return
        }

        void fetchUserProfile(store.host, store.loggedInUserId)
            .then((profile) => {
                const avatarUrl = profile.avatar_url ? `http://${store.host}:8010${profile.avatar_url}` : ""
                store.setLoggedInUserAvatarUrl(avatarUrl)
                setBioDraft(profile.bio ?? "")
                setLocationDraft(profile.location ?? "")
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
        store.setLastResponse(`已选择 ${nextDrafts.length} 个附件`)
    }

    function handleAvatarChange(event: ChangeEvent<HTMLInputElement>) {
        const file = event.target.files?.[0]
        if (!file) {
            return
        }
        setAvatarFile(file)
        setAvatarFileName(file.name)
        setAvatarPreviewUrl(URL.createObjectURL(file))
        store.setLastResponse(`已选择头像草稿：${file.name}`)
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
            throw new Error(`下载失败：${attachment.name}`)
        }
        const blob = await response.blob()
        const objectUrl = URL.createObjectURL(blob)
        setCachedAttachments((current) => ({ ...current, [attachment.url]: objectUrl }))
        setDownloadStates((current) => ({ ...current, [attachment.url]: "cached" }))
        setDownloadHistory((current) => {
            const next = [
                {
                    url: attachment.url,
                    name: attachment.name,
                    timestamp: new Date().toISOString(),
                },
                ...current.filter((item) => item.url !== attachment.url),
            ]
            return next.slice(0, 20)
        })
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
                store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "我", text, timestamp, Number(response.message_id ?? 0))
                store.setLastResponse(JSON.stringify(response, null, 2))
            } else {
                const response = await sendGroupMessage(selectedSession.rawId, text)
                store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "我", text, timestamp, Number(response.message_id ?? 0))
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
                    store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "我", payload, timestamp, Number(response.message_id ?? 0))
                    store.setLastResponse(JSON.stringify(response, null, 2))
                } else {
                    const response = await sendGroupMessage(selectedSession.rawId, payload)
                    store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "我", payload, timestamp, Number(response.message_id ?? 0))
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
                    <p className="eyebrow">ClusterChat 桌面客户端</p>
                    <h1>实时会话</h1>
                    <p className="muted">
                        {isTauriRuntime() ? "运行于 Tauri 桌面容器" : "运行于浏览器预览模式"}
                    </p>
                </div>

                <section>
                    <h2>服务器</h2>
                    <label className="field">
                        <span>主机</span>
                        <input value={store.host} onChange={(event) => store.setField("host", event.target.value)} />
                    </label>
                    <label className="field">
                        <span>端口</span>
                        <input value={store.port} onChange={(event) => store.setField("port", Number(event.target.value) || 0)} />
                    </label>
                </section>

                <section>
                    <h2>登录</h2>
                    <label className="field">
                        <span>用户 ID</span>
                        <input value={store.userIdInput} onChange={(event) => store.setField("userIdInput", event.target.value)} />
                    </label>
                    <label className="field">
                        <span>密码</span>
                        <input type="password" value={store.password} onChange={(event) => store.setField("password", event.target.value)} />
                    </label>
                    <div className="button-row">
                        <button
                            onClick={() =>
                                runAction("登录", async () => {
                                    const response = await login(store.host, store.port, Number(store.userIdInput), store.password)
                                    store.setLoggedInUser(response.id, response.name)
                                    store.setPresence("online")
                                    store.bootstrapFromLogin(response)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            登录
                        </button>
                        <button
                            onClick={() =>
                                runAction("退出登录", async () => {
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
                            退出登录
                        </button>
                    </div>
                </section>

                <section>
                    <h2>会话</h2>
                    <label className="field">
                        <span>搜索会话</span>
                        <input value={sessionFilter} onChange={(event) => setSessionFilter(event.target.value)} placeholder="搜索会话标题或副标题" />
                    </label>
                    {sortedSessions.length === 0 ? <p className="muted">暂无会话。</p> : null}
                    {renderSessionSection("置顶", sessionGroups.pinned)}
                    {renderSessionSection("未读", sessionGroups.unread)}
                    {renderSessionSection("单聊", sessionGroups.direct)}
                    {renderSessionSection("群组", sessionGroups.group)}
                </section>

                <section className="card-section">
                    <p className="eyebrow">最近联系人</p>
                    <div className="recent-contact-list">
                        {recentContacts.length === 0 ? <p className="muted">暂无最近单聊。</p> : null}
                        {recentContacts.map((session) => (
                            <button key={session.sessionId} className="recent-contact" onClick={() => store.setSelectedSessionId(session.sessionId)} type="button">
                                <strong>{session.title}</strong>
                                <span>{session.subtitle}</span>
                            </button>
                        ))}
                    </div>
                </section>

                <section className="card-section">
                    <p className="eyebrow">创建或加入群组</p>
                    <label className="field">
                        <span>群名称</span>
                        <input value={newGroupName} onChange={(event) => setNewGroupName(event.target.value)} />
                    </label>
                    <label className="field">
                        <span>群简介</span>
                        <input value={newGroupDesc} onChange={(event) => setNewGroupDesc(event.target.value)} />
                    </label>
                    <button onClick={() => runAction("创建群组", async () => {
                        const response = await createGroup(newGroupName, newGroupDesc)
                        store.upsertGroupSession(Number(response.groupid), newGroupName || `群组 ${response.groupid}`, newGroupDesc)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">创建群组</button>
                    <label className="field">
                        <span>加入群组 ID</span>
                        <input value={joinGroupId} onChange={(event) => setJoinGroupId(event.target.value)} />
                    </label>
                    <button onClick={() => runAction("加入群组", async () => {
                        const response = await joinGroup(Number(joinGroupId))
                        store.upsertGroupSession(Number(joinGroupId), `群组 ${joinGroupId}`, "已加入群组")
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">加入群组</button>
                </section>
            </aside>

            <main className="panel content">
                <section className="hero">
                    <div>
                        <p className="eyebrow">当前会话</p>
                        <h2>{selectedSession?.title || store.loggedInUserName || "未选择会话"}</h2>
                        <p className="muted">
                            {selectedSession?.subtitle || "登录并选择会话后即可查看实时消息。"}
                        </p>
                    </div>
                    <div className="badge-cluster">
                        <span className="badge">状态：{presenceLabel(store.presence)}</span>
                        <span className="badge">{pending ? `进行中：${pending}` : "空闲"}</span>
                    </div>
                </section>

                <section className="timeline timeline-scroll">
                    <div className="timeline-toolbar">
                        <div className="button-row compact-row">
                            <button className={attachmentFilter === "all" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("all")} type="button">全部</button>
                            <button className={attachmentFilter === "images" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("images")} type="button">图片</button>
                            <button className={attachmentFilter === "files" ? "filter-button filter-button-active" : "filter-button"} onClick={() => setAttachmentFilter("files")} type="button">文件</button>
                        </div>
                        <input value={attachmentSearch} onChange={(event) => setAttachmentSearch(event.target.value)} placeholder="筛选附件记录" />
                    </div>
                    {currentTimeline.length === 0 ? <p className="muted">当前会话暂无消息。</p> : null}
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
                                                runAction("下载附件", async () => {
                                                    const url = await cacheAttachment(entry.attachment!)
                                                    window.open(url, "_blank", "noopener,noreferrer")
                                                })
                                            }
                                            type="button"
                                        >
                                            {downloadStates[entry.attachment.url] === "cached" ? "打开缓存" : "下载"}
                                        </button>
                                        {entry.attachment.kind === "image" ? (
                                            <button onClick={() => setLightboxAttachment(entry.attachment ?? null)} type="button">预览</button>
                                        ) : null}
                                    </div>
                                    {downloadStates[entry.attachment.url] === "downloading" ? <span className="muted">下载中...</span> : null}
                                    {downloadStates[entry.attachment.url] === "error" ? <span className="error-text">下载失败</span> : null}
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
                        placeholder={selectedSession ? "输入消息，或将文件拖到这里..." : "请先选择会话"}
                        value={store.composerText}
                        onChange={(event) => store.setField("composerText", event.target.value)}
                    />
                    <div className="button-row">
                        <label className="button-like">
                            <span>选择附件</span>
                            <input className="hidden-input" multiple type="file" onChange={handleAttachmentChange} />
                        </label>
                        <button
                            disabled={!selectedSession || (!store.composerText.trim() && attachmentDrafts.length === 0)}
                            onClick={() =>
                                runAction("发送消息", async () => {
                                    await sendCurrentMessage()
                                })
                            }
                            type="button"
                        >
                            发送消息
                        </button>
                        <button onClick={() => store.setField("composerText", "")} type="button">清空</button>
                        <button
                            disabled={!selectedSession || attachmentDrafts.every((draft) => !draft.error)}
                            onClick={() =>
                                runAction("重试附件", async () => {
                                    await sendCurrentMessage(true)
                                })
                            }
                            type="button"
                        >
                            重试失败项
                        </button>
                    </div>
                    {attachmentDrafts.length > 0 ? (
                        <div className="draft-grid">
                            {attachmentDrafts.map((draft) => (
                                <div key={draft.id} className="draft-card">
                                    {draft.previewUrl ? <img alt={draft.file.name} className="draft-image" src={draft.previewUrl} /> : null}
                                    <strong>{draft.file.name}</strong>
                                    <span>{formatBytes(draft.file.size)}</span>
                                    <span>{draft.progress > 0 ? `上传 ${draft.progress}%` : "等待中"}</span>
                                    {draft.error ? <span className="error-text">{draft.error}</span> : null}
                                    <button onClick={() => removeAttachmentDraft(draft.id)} type="button">移除</button>
                                </div>
                            ))}
                        </div>
                    ) : null}
                </section>
            </main>

            <aside className="panel inspector">
                <section className="card-section">
                    <p className="eyebrow">个人资料操作</p>
                    <div className="detail-card">
                        <strong>{store.loggedInUserName || "访客"}</strong>
                        <span>用户 ID：{store.loggedInUserId ?? "-"}</span>
                        <span>状态：{presenceLabel(store.presence)}</span>
                        <span>服务器：{store.host}:{store.port}</span>
                    </div>
                    <div className="button-row">
                        <button onClick={() => runAction("切换忙碌", async () => {
                            const response = await setPresence("busy")
                            store.setPresence("busy")
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">设为忙碌</button>
                        <button onClick={() => runAction("切换在线", async () => {
                            const response = await setPresence("online")
                            store.setPresence("online")
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">设为在线</button>
                    </div>
                    <label className="field">
                        <span>昵称</span>
                        <input value={store.loggedInUserName} onChange={(event) => store.setField("loggedInUserName", event.target.value)} />
                    </label>
                    <button onClick={() => runAction("更新昵称", async () => {
                        const response = await setNickname(store.loggedInUserName)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">更新昵称</button>
                    <label className="field">
                        <span>个人简介</span>
                        <textarea value={bioDraft} onChange={(event) => setBioDraft(event.target.value)} />
                    </label>
                    <label className="field">
                        <span>所在地</span>
                        <input value={locationDraft} onChange={(event) => setLocationDraft(event.target.value)} />
                    </label>
                    <button
                        disabled={!store.loggedInUserId}
                        onClick={() =>
                            runAction("保存资料", async () => {
                                if (!store.loggedInUserId) {
                                    return
                                }
                                const profile = await updateUserProfile(store.host, store.loggedInUserId, bioDraft, locationDraft)
                                store.setLastResponse(JSON.stringify(profile, null, 2))
                            })
                        }
                        type="button"
                    >
                        保存资料
                    </button>
                    <label className="field">
                        <span>头像草稿</span>
                        <input type="file" accept="image/*" onChange={handleAvatarChange} />
                    </label>
                    <button
                        disabled={!store.loggedInUserId || !avatarFile}
                        onClick={() =>
                            runAction("上传头像", async () => {
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
                        上传头像
                    </button>
                    {store.loggedInUserAvatarUrl ? <img alt="当前头像" className="avatar-preview" src={store.loggedInUserAvatarUrl} /> : null}
                    {avatarPreviewUrl ? <img alt="头像预览" className="avatar-preview" src={avatarPreviewUrl} /> : null}
                    {avatarFileName ? <p className="muted">已选择文件：{avatarFileName}</p> : null}
                    <button
                        disabled={!avatarFile && !avatarPreviewUrl}
                        onClick={() => {
                            setAvatarFile(null)
                            setAvatarPreviewUrl("")
                            setAvatarFileName("")
                        }}
                        type="button"
                    >
                        重置头像草稿
                    </button>
                </section>

                <section className="card-section">
                    <p className="eyebrow">会话卡片</p>
                    {!selectedSession ? <p className="muted">选择会话以查看详情。</p> : null}
                    {directProfile ? (
                        <div className="detail-card">
                            <strong>{directProfile.name}</strong>
                            {directProfile.avatar_url ? <img alt="好友头像" className="avatar-preview avatar-preview-small" src={directProfile.avatar_url} /> : null}
                            <span>ID：{directProfile.id}</span>
                            <span>状态：{presenceLabel(directProfile.state)}</span>
                            {selectedSession ? (
                                <button onClick={() => store.togglePinnedSession(selectedSession.sessionId)} type="button">
                                    {store.pinnedSessionIds.includes(selectedSession.sessionId) ? "取消置顶" : "置顶会话"}
                                </button>
                            ) : null}
                            <button onClick={() => runAction("添加好友", async () => {
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
                            })} type="button">添加好友</button>
                        </div>
                    ) : null}
                    {groupProfile ? (
                        <div className="detail-card">
                            <strong>{groupProfile.groupname}</strong>
                            <span>ID：{groupProfile.id}</span>
                            <span>{groupProfile.groupdesc}</span>
                            {groupProfile.announcement ? <span>公告：{groupProfile.announcement}</span> : null}
                            <span>成员：{groupStats?.memberCount ?? groupProfile.users.length}</span>
                            <span>管理员：{groupStats?.adminCount ?? 0}</span>
                            <span>已禁言：{groupStats?.mutedCount ?? 0}</span>
                            {selectedSession ? (
                                <button onClick={() => store.togglePinnedSession(selectedSession.sessionId)} type="button">
                                    {store.pinnedSessionIds.includes(selectedSession.sessionId) ? "取消置顶" : "置顶会话"}
                                </button>
                            ) : null}
                        </div>
                    ) : null}
                </section>

                <section className="card-section">
                    <p className="eyebrow">搜索用户</p>
                    <label className="field">
                        <span>关键词</span>
                        <input value={store.searchKeyword} onChange={(event) => store.setField("searchKeyword", event.target.value)} />
                    </label>
                    <button onClick={() => runAction("搜索用户", async () => {
                        const { response, users } = await searchUsers(store.searchKeyword)
                        store.setSearchResults(users)
                        store.setLastResponse(JSON.stringify(response, null, 2))
                    })} type="button">搜索</button>
                    <div className="result-list">
                        {store.searchResults.map((user) => (
                            <article key={user.id} className="result-card">
                                <strong>{user.name}</strong>
                                <span>{presenceLabel(user.state)}</span>
                                <span>好友：{yesNo(user.is_friend)}</span>
                                <div className="button-row compact-row">
                                    <button onClick={() => store.upsertSearchSession(user)} type="button">打开会话</button>
                                    <button onClick={() => runAction("添加好友", async () => {
                                        const response = await addFriend(user.id)
                                        store.markFriend(user)
                                        store.setLastResponse(JSON.stringify(response, null, 2))
                                    })} type="button">添加好友</button>
                                </div>
                            </article>
                        ))}
                    </div>
                </section>

                {groupProfile ? (
                    <section className="card-section">
                        <p className="eyebrow">群组管理</p>
                        <label className="field">
                            <span>群名称</span>
                            <input value={groupProfileName} onChange={(event) => setGroupProfileName(event.target.value)} />
                        </label>
                        <label className="field">
                            <span>群简介</span>
                            <textarea value={groupProfileDesc} onChange={(event) => setGroupProfileDesc(event.target.value)} />
                        </label>
                        <button
                            onClick={() =>
                                runAction("更新群资料", async () => {
                                    const response = await setGroupProfile(groupProfile.id, groupProfileName, groupProfileDesc)
                                    store.updateGroupProfile(groupProfile.id, groupProfileName, groupProfileDesc)
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            保存群资料
                        </button>
                        <label className="field">
                            <span>公告</span>
                            <textarea value={announcementDraft} onChange={(event) => setAnnouncementDraft(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("更新公告", async () => {
                            const response = await setGroupAnnouncement(groupProfile.id, announcementDraft)
                            store.updateGroupAnnouncement(groupProfile.id, announcementDraft)
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">更新公告</button>
                        <label className="field">
                            <span>筛选成员</span>
                            <input value={memberFilter} onChange={(event) => setMemberFilter(event.target.value)} placeholder="按昵称或 ID 搜索" />
                        </label>
                        <div className="member-list">
                            {filteredMembers.map((member) => (
                                <div key={member.id} className="detail-card member-card member-card-interactive">
                                    <strong>{member.name}</strong>
                                    <span>ID：{member.id}</span>
                                    <span>角色：{roleLabel(member.role)}</span>
                                    {member.muted_until ? <span>禁言至：{member.muted_until}</span> : null}
                                    <div className="button-row compact-row">
                                        <button onClick={() => setMuteTargetId(String(member.id))} type="button">选为禁言对象</button>
                                        <button onClick={() => setKickTargetId(String(member.id))} type="button">选为踢出对象</button>
                                    </div>
                                </div>
                            ))}
                        </div>
                        <label className="field">
                            <span>禁言对象 ID</span>
                            <input value={muteTargetId} onChange={(event) => setMuteTargetId(event.target.value)} />
                        </label>
                        <label className="field">
                            <span>分钟</span>
                            <input value={muteMinutes} onChange={(event) => setMuteMinutes(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("禁言成员", async () => {
                            const response = await muteGroupMember(groupProfile.id, Number(muteTargetId), Number(muteMinutes))
                            store.updateGroupMemberMute(groupProfile.id, Number(muteTargetId), String(response.muted_until ?? ""))
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">禁言成员</button>
                        <label className="field">
                            <span>踢出对象 ID</span>
                            <input value={kickTargetId} onChange={(event) => setKickTargetId(event.target.value)} />
                        </label>
                        <button onClick={() => runAction("踢出成员", async () => {
                            const response = await kickGroupMember(groupProfile.id, Number(kickTargetId))
                            store.removeGroupMember(groupProfile.id, Number(kickTargetId))
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">踢出成员</button>
                    </section>
                ) : null}

                <section className="card-section">
                    <p className="eyebrow">历史同步</p>
                    <label className="field">
                        <span>排序</span>
                        <select value={store.historyOrder} onChange={(event) => store.setField("historyOrder", event.target.value as "asc" | "desc") }>
                            <option value="desc">降序</option>
                            <option value="asc">升序</option>
                        </select>
                    </label>
                    <div className="button-row">
                        <button
                            disabled={!selectedSession || selectedSession.kind !== "direct"}
                            onClick={() =>
                                runAction("同步单聊历史", async () => {
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
                            同步单聊
                        </button>
                        <button
                            disabled={!selectedSession || selectedSession.kind !== "group"}
                            onClick={() =>
                                runAction("同步群聊历史", async () => {
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
                            同步群聊
                        </button>
                    </div>
                </section>

                <section>
                    <p className="eyebrow">协议层</p>
                    <pre className="protocol-box">{protocolSummary}</pre>
                </section>
                <section>
                    <p className="eyebrow">最近响应</p>
                    <pre className="protocol-box">{store.lastResponse || "暂无响应。"}</pre>
                </section>
                <section className="card-section">
                    <p className="eyebrow">下载管理</p>
                    <div className="download-list">
                        {attachmentLibrary.length === 0 && downloadHistory.length === 0 ? <p className="muted">暂无附件记录。</p> : null}
                        {attachmentLibrary.slice(0, 8).map((item) => (
                            <div key={item.id} className="download-item">
                                <div>
                                    <strong>{item.attachment.name}</strong>
                                    <div className="muted small-text">{item.author} · {item.timestamp}</div>
                                </div>
                                <div className="button-row compact-row">
                                    <span className="badge">{downloadStateLabel(downloadStates[item.attachment.url])}</span>
                                    <button
                                        onClick={() =>
                                            runAction("下载附件", async () => {
                                                const url = await cacheAttachment(item.attachment)
                                                window.open(url, "_blank", "noopener,noreferrer")
                                            })
                                        }
                                        type="button"
                                    >
                                        {cachedAttachments[item.attachment.url] ? "打开" : "缓存"}
                                    </button>
                                </div>
                            </div>
                        ))}
                        {attachmentLibrary.length === 0
                            ? downloadHistory.slice(0, 8).map((item) => (
                                <div key={item.url} className="download-item">
                                    <div>
                                        <strong>{item.name}</strong>
                                        <div className="muted small-text">{item.timestamp}</div>
                                    </div>
                                    <span className="badge">历史</span>
                                </div>
                            ))
                            : null}
                    </div>
                </section>
            </aside>

            {lightboxAttachment ? (
                <div className="lightbox" onClick={() => setLightboxAttachment(null)} role="presentation">
                    <div className="lightbox-content" onClick={(event) => event.stopPropagation()} role="presentation">
                        <div className="button-row">
                            <strong>{lightboxAttachment.name}</strong>
                            <button onClick={() => setLightboxAttachment(null)} type="button">关闭</button>
                        </div>
                        <img alt={lightboxAttachment.name} className="lightbox-image" src={cachedAttachments[lightboxAttachment.url] ?? lightboxAttachment.url} />
                    </div>
                </div>
            ) : null}
        </div>
    )
}
