import { create } from "zustand"

import type { HistoryEntry, SearchUserEntry } from "../../lib/protocol"

interface SessionStoreState {
    host: string
    port: number
    userIdInput: string
    password: string
    loggedInUserId: number | null
    loggedInUserName: string
    presence: string
    searchKeyword: string
    historyTargetId: string
    historyGroupId: string
    historyOrder: "asc" | "desc"
    searchResults: SearchUserEntry[]
    historyEntries: HistoryEntry[]
    lastResponse: string
    setField: (field: string, value: string | number) => void
    setLoggedInUser: (userId: number, name: string) => void
    setPresence: (presence: string) => void
    setSearchResults: (results: SearchUserEntry[]) => void
    setHistoryEntries: (entries: HistoryEntry[]) => void
    setLastResponse: (payload: string) => void
    resetSession: () => void
}

const initialState = {
    host: "127.0.0.1",
    port: 9120,
    userIdInput: "",
    password: "",
    loggedInUserId: null,
    loggedInUserName: "",
    presence: "offline",
    searchKeyword: "",
    historyTargetId: "",
    historyGroupId: "",
    historyOrder: "desc" as const,
    searchResults: [] as SearchUserEntry[],
    historyEntries: [] as HistoryEntry[],
    lastResponse: "",
}

export const useSessionStore = create<SessionStoreState>((set) => ({
    ...initialState,
    setField: (field, value) => set(() => ({ [field]: value } as Partial<SessionStoreState>)),
    setLoggedInUser: (userId, name) => set(() => ({ loggedInUserId: userId, loggedInUserName: name })),
    setPresence: (presence) => set(() => ({ presence })),
    setSearchResults: (searchResults) => set(() => ({ searchResults })),
    setHistoryEntries: (historyEntries) => set(() => ({ historyEntries })),
    setLastResponse: (lastResponse) => set(() => ({ lastResponse })),
    resetSession: () =>
        set(() => ({
            ...initialState,
            host: initialState.host,
            port: initialState.port,
        })),
}))
