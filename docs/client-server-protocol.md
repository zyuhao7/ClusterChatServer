# Client-Server Protocol Guide

## Overview

This document summarizes the current application-level contract between chat clients and `ClusterChatServer`.

All requests and responses use JSON.

## Shared Envelope

Every client request must include:

```json
{
  "version": 1,
  "msgid": 1,
  "request_id": "req-123"
}
```

Every ACK response returns:

```json
{
  "version": 1,
  "msgid": 2,
  "request_id": "req-123",
  "errno": 0,
  "errmsg": ""
}
```

## Protocol Rules

- `version` is mandatory and must equal `1`
- `request_id` is mandatory for client initiated actions
- `request_id` is used for idempotency on message delivery paths
- `errno = 0` means success
- non-zero `errno` values use the constants defined in `include/public.hpp`

## Core Message IDs

### Authentication

- `1` `LOGIN_MSG`
- `2` `LOGIN_MSG_ACK`
- `3` `LOGINOUT_MSG`
- `4` `LOGINOUT_MSG_ACK`
- `5` `REG_MSG`
- `6` `REG_MSG_ACK`

### Direct Chat and Friends

- `7` `ONE_CHAT_MSG`
- `8` `ONE_CHAT_MSG_ACK`
- `9` `ADD_FRIEND_MSG`
- `10` `ADD_FRIEND_MSG_ACK`

### Groups

- `11` `CREATE_GROUP_MSG`
- `12` `CREATE_GROUP_MSG_ACK`
- `13` `ADD_GROUP_MSG`
- `14` `ADD_GROUP_MSG_ACK`
- `15` `GROUP_CHAT_MSG`
- `16` `GROUP_CHAT_MSG_ACK`
- `17` `LEAVE_GROUP_MSG`
- `18` `LEAVE_GROUP_MSG_ACK`
- `19` `SET_GROUP_ROLE_MSG`
- `20` `SET_GROUP_ROLE_MSG_ACK`

### Message State

- `21` `MARK_READ_MSG`
- `22` `MARK_READ_MSG_ACK`
- `23` `RECALL_MSG`
- `24` `RECALL_MSG_ACK`
- `25` `RECALL_NOTIFY_MSG`
- `26` `QUERY_HISTORY_MSG`
- `27` `QUERY_HISTORY_MSG_ACK`

### User Moderation and Profile

- `28` `SEARCH_USER_MSG`
- `29` `SEARCH_USER_MSG_ACK`
- `30` `ADD_BLACKLIST_MSG`
- `31` `ADD_BLACKLIST_MSG_ACK`
- `32` `REMOVE_BLACKLIST_MSG`
- `33` `REMOVE_BLACKLIST_MSG_ACK`
- `34` `SET_USER_STATE_MSG`
- `35` `SET_USER_STATE_MSG_ACK`
- `36` `SET_NICKNAME_MSG`
- `37` `SET_NICKNAME_MSG_ACK`

### Group Moderation

- `38` `SET_GROUP_ANNOUNCEMENT_MSG`
- `39` `SET_GROUP_ANNOUNCEMENT_MSG_ACK`
- `40` `MUTE_GROUP_MEMBER_MSG`
- `41` `MUTE_GROUP_MEMBER_MSG_ACK`
- `42` `KICK_GROUP_MEMBER_MSG`
- `43` `KICK_GROUP_MEMBER_MSG_ACK`

## Common Request Shapes

### Login

```json
{
  "version": 1,
  "msgid": 1,
  "request_id": "login-1",
  "id": 1001,
  "password": "secret"
}
```

### Direct Chat

```json
{
  "version": 1,
  "msgid": 7,
  "request_id": "chat-1",
  "id": 1001,
  "name": "alice",
  "toid": 1002,
  "msg": "hello",
  "time": "2026-04-19 20:00:00"
}
```

### Query History

```json
{
  "version": 1,
  "msgid": 26,
  "request_id": "history-1",
  "id": 1001,
  "targetid": 1002,
  "limit": 20,
  "offset": 0,
  "order": "desc"
}
```

For group history, replace `targetid` with `groupid`.

### Search User

```json
{
  "version": 1,
  "msgid": 28,
  "request_id": "search-1",
  "id": 1001,
  "keyword": "alice",
  "limit": 20,
  "offset": 0
}
```

### Set Presence State

```json
{
  "version": 1,
  "msgid": 34,
  "request_id": "state-1",
  "id": 1001,
  "state": "busy"
}
```

### Set Nickname

```json
{
  "version": 1,
  "msgid": 36,
  "request_id": "nickname-1",
  "id": 1001,
  "name": "alice-renamed"
}
```

### Set Group Announcement

```json
{
  "version": 1,
  "msgid": 38,
  "request_id": "announcement-1",
  "id": 1001,
  "groupid": 2001,
  "announcement": "maintenance at 22:00"
}
```

### Mute Group Member

```json
{
  "version": 1,
  "msgid": 40,
  "request_id": "mute-1",
  "id": 1001,
  "groupid": 2001,
  "targetid": 1002,
  "minutes": 10
}
```

### Kick Group Member

```json
{
  "version": 1,
  "msgid": 42,
  "request_id": "kick-1",
  "id": 1001,
  "groupid": 2001,
  "targetid": 1002
}
```

## Important Error Codes

### Authentication

- `4102` already online or busy
- `4103` banned

### Group Role and Moderation

- `4413` cannot change creator role
- `4418` admin cannot grant admin role
- `4419` group announcement cannot be empty
- `4422` member is muted
- `4424` kick permission denied

### User Features

- `4608` blacklist relation blocks action
- `4609` invalid user state
- `4611` nickname cannot be empty

## Desktop Client Integration Advice

- keep message id and error code constants in a generated or shared TypeScript file
- model ACK handling centrally instead of per component
- keep `request_id` generation in one client-side transport layer
- treat `errno != 0` as business failure, not transport failure
- keep history ordering, blacklist, nickname, and moderation actions in separate feature modules

## Avatar Upload Path

Avatar upload is handled by `admin_service`, not the TCP chat protocol.

- `POST /api/v1/users/{user_id}/avatar`
- multipart field name: `avatar`
- static file base: `/media/avatars/...`
