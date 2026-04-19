import { useMemo } from "react"

import { mockSessions, mockTimeline } from "./lib/mock-data"
import { createProtocolSummary } from "./lib/protocol"
import { isTauriRuntime } from "./lib/tauri"
import { useSessionStore } from "./features/session/store"

export default function App() {
    const selectedSessionId = useSessionStore((state) => state.selectedSessionId)
    const setSelectedSessionId = useSessionStore((state) => state.setSelectedSessionId)
    const selectedSession = mockSessions.find((item) => item.id === selectedSessionId) ?? mockSessions[0]

    const protocolSummary = useMemo(() => createProtocolSummary(), [])

    return (
        <div className="app-shell">
            <aside className="panel sidebar">
                <div>
                    <p className="eyebrow">ClusterChat Desktop</p>
                    <h1>Operator Console</h1>
                    <p className="muted">
                        {isTauriRuntime() ? "Running inside Tauri shell" : "Running in browser preview mode"}
                    </p>
                </div>

                <section>
                    <h2>Sessions</h2>
                    <div className="session-list">
                        {mockSessions.map((session) => (
                            <button
                                key={session.id}
                                className={session.id === selectedSession.id ? "session session-active" : "session"}
                                onClick={() => setSelectedSessionId(session.id)}
                                type="button"
                            >
                                <span className="session-title">{session.title}</span>
                                <span className="session-meta">{session.kind} · {session.presence}</span>
                            </button>
                        ))}
                    </div>
                </section>
            </aside>

            <main className="panel content">
                <section className="hero">
                    <div>
                        <p className="eyebrow">Current Focus</p>
                        <h2>{selectedSession.title}</h2>
                        <p className="muted">{selectedSession.description}</p>
                    </div>
                    <div className="badge-cluster">
                        <span className="badge">{selectedSession.kind}</span>
                        <span className="badge">{selectedSession.presence}</span>
                    </div>
                </section>

                <section className="timeline">
                    {mockTimeline[selectedSession.id].map((message) => (
                        <article key={message.id} className="message-card">
                            <div className="message-head">
                                <strong>{message.author}</strong>
                                <span>{message.timestamp}</span>
                            </div>
                            <p>{message.body}</p>
                        </article>
                    ))}
                </section>
            </main>

            <aside className="panel inspector">
                <section>
                    <p className="eyebrow">Client Plan</p>
                    <h2>Phase 1 Scope</h2>
                    <ul className="plain-list">
                        <li>Login and session bootstrap</li>
                        <li>Conversation list and chat timeline</li>
                        <li>Search user and profile editing</li>
                        <li>Status and nickname actions</li>
                    </ul>
                </section>

                <section>
                    <p className="eyebrow">Protocol Layer</p>
                    <pre className="protocol-box">{protocolSummary}</pre>
                </section>
            </aside>
        </div>
    )
}
