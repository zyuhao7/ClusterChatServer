import { create } from "zustand"

interface SessionState {
    selectedSessionId: string
    setSelectedSessionId: (sessionId: string) => void
}

export const useSessionStore = create<SessionState>((set) => ({
    selectedSessionId: "direct-alice",
    setSelectedSessionId: (sessionId) => set({ selectedSessionId: sessionId }),
}))
