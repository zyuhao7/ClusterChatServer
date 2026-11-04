import type { PresenceState } from "./protocol"

export interface SessionCard {
    id: string
    title: string
    kind: "direct" | "group"
    presence: PresenceState
    description: string
}

export interface TimelineEntry {
    id: string
    author: string
    timestamp: string
    body: string
}

export const mockSessions: SessionCard[] = [
    {
        id: "direct-alice",
        title: "Alice",
        kind: "direct",
        presence: "busy",
        description: "单聊入口，支持昵称、资料和状态编辑。",
    },
    {
        id: "group-ops",
        title: "运维指挥群",
        kind: "group",
        presence: "online",
        description: "群管理面板，提供角色、禁言、踢人和公告设置。",
    },
]

export const mockTimeline: Record<string, TimelineEntry[]> = {
    "direct-alice": [
        {
            id: "m-1",
            author: "Alice",
            timestamp: "2026-04-19 20:10",
            body: "桌面端第一阶段需要先让昵称和状态流程可见。",
        },
        {
            id: "m-2",
            author: "我",
            timestamp: "2026-04-19 20:12",
            body: "协议可以保持稳定，头像等能力后续再放进更完整的个人资料页。",
        },
    ],
    "group-ops": [
        {
            id: "g-1",
            author: "系统",
            timestamp: "2026-04-19 21:00",
            body: "群主可以提升管理员；管理员只能降级非群主成员。",
        },
        {
            id: "g-2",
            author: "运维机器人",
            timestamp: "2026-04-19 21:05",
            body: "接下来：禁言、踢人、公告、头像上传和媒体传输。",
        },
    ],
}
