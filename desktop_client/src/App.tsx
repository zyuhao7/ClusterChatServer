import { useMemo, useState } from "react"

import { login, logout, queryDirectHistory, queryGroupHistory, searchUsers, setNickname, setPresence } from "./lib/bridge"
import { createProtocolSummary } from "./lib/protocol"
import { isTauriRuntime } from "./lib/tauri"
import { useSessionStore } from "./features/session/store"

export default function App() {
    const store = useSessionStore()
    const [pending, setPending] = useState<string | null>(null)
    const protocolSummary = useMemo(() => createProtocolSummary(), [])

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

    return (
        <div className="app-shell">
            <aside className="panel sidebar">
                <div>
                    <p className="eyebrow">ClusterChat Desktop</p>
                    <h1>Protocol Console</h1>
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
                                    store.setLastResponse(JSON.stringify(response, null, 2))
                                })
                            }
                            type="button"
                        >
                            Logout
                        </button>
                    </div>
                    <p className="muted">Current user: {store.loggedInUserName || "not logged in"}</p>
                </section>
            </aside>

            <main className="panel content">
                <section className="hero">
                    <div>
                        <p className="eyebrow">Live Bridge</p>
                        <h2>{store.loggedInUserName || "No active session"}</h2>
                        <p className="muted">This view uses real Tauri commands backed by the chat TCP protocol.</p>
                    </div>
                    <div className="badge-cluster">
                        <span className="badge">presence: {store.presence}</span>
                        <span className="badge">{pending ? `running ${pending}` : "idle"}</span>
                    </div>
                </section>

                <section className="action-grid">
                    <article className="card-section">
                        <h3>Profile Actions</h3>
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
                            <span>New nickname</span>
                            <input value={store.loggedInUserName} onChange={(event) => store.setField("loggedInUserName", event.target.value)} />
                        </label>
                        <button onClick={() => runAction("set nickname", async () => {
                            const response = await setNickname(store.loggedInUserName)
                            store.setLastResponse(JSON.stringify(response, null, 2))
                        })} type="button">Update Nickname</button>
                    </article>

                    <article className="card-section">
                        <h3>Search Users</h3>
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
                                    <span>blocked: {String(user.has_blocked)}</span>
                                </article>
                            ))}
                        </div>
                    </article>

                    <article className="card-section">
                        <h3>History</h3>
                        <label className="field">
                            <span>Direct target ID</span>
                            <input value={store.historyTargetId} onChange={(event) => store.setField("historyTargetId", event.target.value)} />
                        </label>
                        <label className="field">
                            <span>Group ID</span>
                            <input value={store.historyGroupId} onChange={(event) => store.setField("historyGroupId", event.target.value)} />
                        </label>
                        <label className="field">
                            <span>Order</span>
                            <select value={store.historyOrder} onChange={(event) => store.setField("historyOrder", event.target.value as "asc" | "desc") }>
                                <option value="desc">desc</option>
                                <option value="asc">asc</option>
                            </select>
                        </label>
                        <div className="button-row">
                            <button onClick={() => runAction("direct history", async () => {
                                const { response, history } = await queryDirectHistory(Number(store.historyTargetId), 20, 0, store.historyOrder)
                                store.setHistoryEntries(history)
                                store.setLastResponse(JSON.stringify(response, null, 2))
                            })} type="button">Load Direct</button>
                            <button onClick={() => runAction("group history", async () => {
                                const { response, history } = await queryGroupHistory(Number(store.historyGroupId), 20, 0, store.historyOrder)
                                store.setHistoryEntries(history)
                                store.setLastResponse(JSON.stringify(response, null, 2))
                            })} type="button">Load Group</button>
                        </div>
                        <div className="timeline">
                            {store.historyEntries.map((entry) => (
                                <article key={entry.id} className="message-card">
                                    <div className="message-head">
                                        <strong>{entry.sender_id}</strong>
                                        <span>{entry.created_at}</span>
                                    </div>
                                    <p>{entry.message}</p>
                                </article>
                            ))}
                        </div>
                    </article>
                </section>
            </main>

            <aside className="panel inspector">
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
