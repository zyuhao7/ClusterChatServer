# ClusterChat Desktop Client

This directory contains a Tauri + React + TypeScript desktop client skeleton for `ClusterChatServer`.

## Goals

- provide a modern desktop shell for login, session list, and chat views
- support profile editing, status switching, user search, and future avatar upload flows
- keep protocol integration isolated from the UI layer

## Planned Stack

- Tauri 2
- React 18
- TypeScript
- Vite
- Zustand for local UI state
- TanStack Query for async server state

## Project Structure

```text
desktop_client/
├── package.json
├── src/
│   ├── App.tsx
│   ├── main.tsx
│   ├── styles.css
│   ├── features/
│   │   └── session/
│   │       └── store.ts
│   └── lib/
│       ├── mock-data.ts
│       ├── protocol.ts
│       └── tauri.ts
└── src-tauri/
    ├── Cargo.toml
    ├── build.rs
    ├── tauri.conf.json
    └── src/
        └── main.rs
```

## Prerequisites

- Node.js 18+
- Rust toolchain with `cargo`
- Tauri system dependencies for your OS

## Next Steps

1. install frontend dependencies with `npm install`
2. install Rust if not already available
3. run `npm run tauri:dev`
4. replace mock data and placeholder bridge calls with real protocol integration
