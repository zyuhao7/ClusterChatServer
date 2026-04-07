# ClusterChatServer GitHub Issues 风格 TODO

以下内容适合直接拆成 GitHub Issues。

---

## Issue 1
**Title:** build: add `.gitignore` for generated artifacts  
**Labels:** `build`, `maintenance`, `good first issue`  
**Priority:** P0
**Status:** 已完成

### Description
忽略构建产物和本地工具文件，避免误提交。

### Acceptance Criteria
- [x] 忽略 `bin/`
- [x] 忽略 `build/`
- [x] 忽略 example 可执行文件
- [x] 忽略本地临时工具文件

---

## Issue 2
**Title:** feat(config): load database and redis settings from config file  
**Labels:** `enhancement`, `backend`, `config`  
**Priority:** P0
**Status:** 已完成

### Description
将数据库、Redis、监听端口等配置从源码中抽离。

### Acceptance Criteria
- [x] 新增配置文件
- [x] 支持 MySQL 配置
- [x] 支持 Redis 配置
- [x] 支持服务监听配置
- [x] README 增加配置说明

---

## Issue 3
**Title:** feat(db): improve schema constraints for groups and offline messages  
**Labels:** `database`, `enhancement`  
**Priority:** P0
**Status:** 已完成

### Description
完善表约束，避免脏数据。

### Acceptance Criteria
- [x] `groupuser` 增加联合主键
- [x] `offlinemessage` 增加主键和时间戳
- [x] 关键字段增加索引

---

## Issue 4
**Title:** feat(message): persist chat history with pagination support  
**Labels:** `backend`, `message`, `database`  
**Priority:** P0
**Status:** 已完成

### Description
新增历史消息表，并支持分页查询。

### Acceptance Criteria
- [x] 新增消息历史表
- [x] 一对一消息入库
- [x] 群消息入库
- [x] 提供分页查询接口或查询函数

---

## Issue 5
**Title:** feat(protocol): unify ack payload and error code conventions  
**Labels:** `protocol`, `enhancement`  
**Priority:** P1
**Status:** 已完成

### Description
统一所有响应消息结构。

### Acceptance Criteria
- [x] 所有业务返回 ACK
- [x] 统一 `errno` / `errmsg`
- [x] 增加 `request_id`

---

## Issue 6
**Title:** feat(server): add stricter validation for group operations  
**Labels:** `backend`, `group`, `validation`  
**Priority:** P1
**Status:** 已完成

### Description
完善群组权限逻辑。

### Acceptance Criteria
- [x] 创建群校验
- [x] 加群校验
- [x] 退群逻辑
- [x] 群管理员角色

---

## Issue 7
**Title:** feat(security): hash user passwords instead of storing plaintext  
**Labels:** `security`, `backend`, `database`  
**Priority:** P1
**Status:** 已完成

### Description
将密码明文改为哈希存储。

### Acceptance Criteria
- [x] 注册时哈希
- [x] 登录时校验哈希
- [x] README 更新

---

## Issue 8
**Title:** feat(client): support read/unread and message recall states  
**Labels:** `client`, `message`, `enhancement`  
**Priority:** P2
**Status:** 已完成

### Description
增强消息状态管理。

### Acceptance Criteria
- [x] 已读状态
- [x] 未读状态
- [x] 消息撤回标记

---

## Issue 9
**Title:** test: add unit tests for models and service logic  
**Labels:** `test`, `backend`  
**Priority:** P1
**Status:** 部分完成（已从占位脚手架升级为可执行测试二进制，当前覆盖配置解析与密码安全；`UserModel/FriendModel/GroupModel/ChatService` 专项测试仍待补齐）

### Description
增加 C++ 单元测试。

### Acceptance Criteria
- [ ] UserModel 测试
- [ ] FriendModel 测试
- [ ] GroupModel 测试
- [ ] ChatService 关键逻辑测试

---

## Issue 10
**Title:** test(e2e): add Python integration tests for login and messaging flows  
**Labels:** `test`, `python`, `integration`  
**Priority:** P1
**Status:** 部分完成（已补 `admin_service` 可执行集成测试样例；登录、注册、加好友、群聊主流程端到端仍待补齐）

### Description
使用 Python 编写集成测试驱动。

### Acceptance Criteria
- [ ] 登录流程测试
- [ ] 注册流程测试
- [ ] 添加好友测试
- [ ] 群聊测试

---

## Issue 11
**Title:** feat(admin): scaffold Python FastAPI admin service  
**Labels:** `python`, `fastapi`, `admin`, `enhancement`  
**Priority:** P0
**Status:** 已完成

### Description
新增 Python 管理后台骨架。

### Acceptance Criteria
- [x] FastAPI 项目结构
- [x] 健康检查接口
- [x] 用户查询接口
- [x] 好友关系查询接口
- [x] 群组查询接口
- [x] 离线消息查询接口

---

## Issue 12
**Title:** feat(admin): add admin APIs for user moderation and audit  
**Labels:** `python`, `admin`, `security`  
**Priority:** P2
**Status:** 已完成

### Description
管理后台支持封禁、审计、统计。

### Acceptance Criteria
- [x] 封禁用户
- [x] 审计日志
- [x] 管理员操作记录

---

## Issue 13
**Title:** feat(deploy): add Docker Compose stack for chat server, mysql, redis and nginx  
**Labels:** `devops`, `docker`, `deployment`  
**Priority:** P0
**Status:** 已完成（部署文件已落地，运行验证进行中）

### Description
提供一键部署。

### Acceptance Criteria
- [x] `Dockerfile`
- [x] `docker-compose.yml`
- [x] 启动说明

---

## Issue 14
**Title:** feat(observability): expose metrics for Prometheus and Grafana  
**Labels:** `monitoring`, `observability`  
**Priority:** P2
**Status:** 已完成（指标与模板已落地，运行验证待随部署一起确认）

### Description
增加监控能力。

### Acceptance Criteria
- [x] 在线人数指标
- [x] 消息吞吐量指标
- [x] MySQL 耗时指标
- [x] Redis 延迟指标

---

## Issue 15
**Title:** feat(ai): integrate Python moderation or chatbot service  
**Labels:** `python`, `ai`, `experimental`  
**Priority:** P3
**Status:** 已完成（当前为 mock/本地异步实现）

### Description
为系统增加更高级的 AI 能力。

### Acceptance Criteria
- [x] 敏感词审核
- [x] 机器人回复
- [x] 聊天摘要
