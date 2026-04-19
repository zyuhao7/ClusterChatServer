import { ChangeEvent, useEffect, useMemo, useState } from "react"

import {
    addFriend,
    createGroup,
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
} from "./lib/bridge"
import { createProtocolSummary, directSessionId, groupSessionId } from "./lib/protocol"
import { isTauriRuntime } from "./lib/tauri"
import { useSessionStore } from "./features/session/store"

export default function App() {
    const store = useSessionStore()
    const [pending, setPending] = useState<string | null>(null)
    const [newGroupName, setNewGroupName] = useState("")
    const [newGroupDesc, setNewGroupDesc] = useState("")
    const [joinGroupId, setJoinGroupId] = useState("")
    const [announcementDraft, setAnnouncementDraft] = useState("")
    const [muteTargetId, setMuteTargetId] = useState("")
    const [muteMinutes, setMuteMinutes] = useState("10")
    const [kickTargetId, setKickTargetId] = useState("")
    const [avatarPreviewUrl, setAvatarPreviewUrl] = useState("")
    const [avatarFileName, setAvatarFileName] = useState("")

    const protocolSummary = useMemo(() => createProtocolSummary(), [])
    const selectedSession = store.sessions.find((session) => session.sessionId === store.selectedSessionId) ?? null
    const currentTimeline = selectedSession ? store.timelines[selectedSession.sessionId] ?? [] : []
    const directProfile = selectedSession?.kind === "direct" ? store.friends[selectedSession.rawId] ?? null : null
    const groupProfile = selectedSession?.kind === "group" ? store.groups[selectedSession.rawId] ?? null : null

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

    function handleAvatarChange(event: ChangeEvent<HTMLInputElement>) {
        const file = event.target.files?.[0]
        if (!file) {
            return
        }
        setAvatarFileName(file.name)
        setAvatarPreviewUrl(URL.createObjectURL(file))
        store.setLastResponse(`Selected avatar draft: ${file.name}`)
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
                                    setAvatarPreviewUrl("")
                                    setAvatarFileName("")
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
                    <div className="session-list">
                        {store.sessions.length === 0 ? <p className="muted">No sessions yet.</p> : null}
                        {store.sessions.map((session) => (
                            <button
                                key={session.sessionId}
                                className={store.selectedSessionId === session.sessionId ? "session session-active" : "session"}
                                onClick={() => store.setSelectedSessionId(session.sessionId)}
                                type="button"
                            >
                                <div className="session-head-row">
                                    <span className="session-title">{session.title}</span>
                                    {session.unreadCount > 0 ? <span className="badge badge-unread">{session.unreadCount}</span> : null}
                                </div>
                                <span className="session-meta">{session.kind} · {session.subtitle}</span>
                                {session.latestMessage ? <span className="session-preview">{session.latestMessage}</span> : null}
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
                    {currentTimeline.length === 0 ? <p className="muted">No messages for the selected session yet.</p> : null}
                    {currentTimeline.map((entry) => (
                        <article key={entry.id} className={entry.recalled ? "message-card message-recalled" : "message-card"}>
                            <div className="message-head">
                                <strong>{entry.author}</strong>
                                <span>{entry.timestamp}</span>
                            </div>
                            <p>{entry.body}</p>
                        </article>
                    ))}
                </section>

                <section className="composer-box">
                    <textarea
                        className="composer-input"
                        placeholder={selectedSession ? "Type a message..." : "Select a session first"}
                        value={store.composerText}
                        onChange={(event) => store.setField("composerText", event.target.value)}
                    />
                    <div className="button-row">
                        <button
                            disabled={!selectedSession || !store.composerText.trim()}
                            onClick={() =>
                                runAction("send message", async () => {
                                    if (!selectedSession) {
                                        return
                                    }
                                    const text = store.composerText.trim()
                                    const timestamp = new Date().toISOString().replace("T", " ").slice(0, 19)
                                    if (selectedSession.kind === "direct") {
                                        const response = await sendDirectMessage(selectedSession.rawId, text)
                                        store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", text, timestamp, Number(response.message_id ?? 0))
                                        store.setLastResponse(JSON.stringify(response, null, 2))
                                    } else {
                                        const response = await sendGroupMessage(selectedSession.rawId, text)
                                        store.appendLocalMessage(selectedSession.sessionId, store.loggedInUserName || "me", text, timestamp, Number(response.message_id ?? 0))
                                        store.setLastResponse(JSON.stringify(response, null, 2))
                                    }
                                })
                            }
                            type="button"
                        >
                            Send Message
                        </button>
                        <button onClick={() => store.setField("composerText", "")} type="button">Clear</button>
                    </div>
                </section>
            </main>

            <aside className="panel inspector">
                <section className="card-section">
                    <p className="eyebrow">Profile Actions</p>
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
                    {avatarPreviewUrl ? <img alt="avatar preview" className="avatar-preview" src={avatarPreviewUrl} /> : null}
                    {avatarFileName ? <p className="muted">Selected file: {avatarFileName}</p> : null}
                </section>

                <section className="card-section">
                    <p className="eyebrow">Session Card</p>
                    {!selectedSession ? <p className="muted">Select a session to view details.</p> : null}
                    {directProfile ? (
                        <div className="detail-card">
                            <strong>{directProfile.name}</strong>
                            <span>ID: {directProfile.id}</span>
                            <span>State: {directProfile.state}</span>
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
                            <span>Members: {groupProfile.users.length}</span>
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
                        <div className="member-list">
                            {groupProfile.users.map((member) => (
                                <div key={member.id} className="detail-card member-card">
                                    <strong>{member.name}</strong>
                                    <span>ID: {member.id}</span>
                                    <span>Role: {member.role}</span>
                                    {member.muted_until ? <span>Muted until: {member.muted_until}</span> : null}
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
            </aside>
        </div>
    )
}
