# Tauri + React Desktop Client Plan

## Purpose

This document defines the initial desktop client direction for `ClusterChatServer`.

The server is already growing into a more complete IM backend with:

- protocol versioning
- request id based idempotency
- shared error codes
- history pagination and ordering
- moderation primitives such as blacklist, user state, and group role controls

The desktop client should translate those backend capabilities into a modern operator-facing and user-facing application.

## Recommended Stack

- Tauri 2
- React 18
- TypeScript
- Vite
- Zustand
- TanStack Query
- Tailwind CSS
- shadcn/ui

## Why Tauri + React

### Strengths

- desktop shell is lightweight compared with Electron
- React has stronger ecosystem support for complex stateful UI flows
- profile pages, avatar upload, search panels, and moderation drawers are much easier than in a CLI
- the current server protocol can remain language-agnostic while the desktop client evolves independently

### Tradeoffs

- introduces a mixed Rust + TypeScript toolchain
- requires a clear frontend/backend bridge boundary
- local storage, reconnect logic, and asset caching need explicit design

## Scope Split

### Phase 1

- login and session bootstrap
- direct and group conversation list
- message timeline rendering
- search user
- update nickname
- switch presence state between `online` and `busy`
- query history with pagination and order

### Phase 2

- avatar upload and profile panel
- image and file message support
- group admin management panel
- blacklist management panel
- unread and recall indicators in the timeline

### Phase 3

- mute, kick, announcement, and other advanced group moderation tools
- reconnect strategy and offline cache
- desktop notifications
- settings, diagnostics, and logs

## Suggested Frontend Architecture

```text
desktop_client/src/
├── app/
│   ├── providers/
│   └── router/
├── features/
│   ├── auth/
│   ├── chat/
│   ├── groups/
│   ├── profile/
│   └── search/
├── lib/
│   ├── bridge/
│   ├── protocol/
│   ├── query/
│   └── utils/
├── components/
└── pages/
```

## Bridge Strategy

The desktop client should not embed protocol details directly into view components.

Use three layers:

1. protocol DTOs and request builders
2. transport and Tauri bridge wrappers
3. UI queries and commands bound to screens

This keeps the server protocol reusable if a future mobile or web client is added.

## Avatar Recommendation

Avatar is not a good fit for the current terminal client. A modern desktop UI should own it.

Recommended avatar flow:

1. open file picker from Tauri
2. preview and crop image in React
3. upload metadata or file path through a dedicated backend endpoint
4. cache avatar URL or local path in desktop state

## Immediate Next Steps

1. install Rust toolchain and Tauri prerequisites on the development machine
2. run `npm install` under `desktop_client/`
3. verify `npm run typecheck`
4. verify `npm run tauri:dev`
5. replace mock timeline and session data with protocol backed commands
