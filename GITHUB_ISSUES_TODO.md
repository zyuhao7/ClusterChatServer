# ClusterChatServer GitHub Issues 风格 TODO

以下内容适合直接拆成 GitHub Issues。

---

## Issue 1
**Title:** build: add `.gitignore` for generated artifacts  
**Labels:** `build`, `maintenance`, `good first issue`  
**Priority:** P0

### Description
忽略构建产物和本地工具文件，避免误提交。

### Acceptance Criteria
- [ ] 忽略 `bin/`
- [ ] 忽略 `build/`
- [ ] 忽略 example 可执行文件
- [ ] 忽略本地临时工具文件

---

## Issue 2
**Title:** feat(config): load database and redis settings from config file  
**Labels:** `enhancement`, `backend`, `config`  
**Priority:** P0

### Description
将数据库、Redis、监听端口等配置从源码中抽离。

### Acceptance Criteria
- [ ] 新增配置文件
- [ ] 支持 MySQL 配置
- [ ] 支持 Redis 配置
- [ ] 支持服务监听配置
- [ ] README 增加配置说明

---

## Issue 3
**Title:** feat(db): improve schema constraints for groups and offline messages  
**Labels:** `database`, `enhancement`  
**Priority:** P0

### Description
完善表约束，避免脏数据。

### Acceptance Criteria
- [ ] `groupuser` 增加联合主键
- [ ] `offlinemessage` 增加主键和时间戳
- [ ] 关键字段增加索引

---

## Issue 4
**Title:** feat(message): persist chat history with pagination support  
**Labels:** `backend`, `message`, `database`  
**Priority:** P0

### Description
新增历史消息表，并支持分页查询。

### Acceptance Criteria
- [ ] 新增消息历史表
- [ ] 一对一消息入库
- [ ] 群消息入库
- [ ] 提供分页查询接口或查询函数

---

## Issue 5
**Title:** feat(protocol): unify ack payload and error code conventions  
**Labels:** `protocol`, `enhancement`  
**Priority:** P1

### Description
统一所有响应消息结构。

### Acceptance Criteria
- [ ] 所有业务返回 ACK
- [ ] 统一 `errno` / `errmsg`
- [ ] 增加 `request_id`

---

## Issue 6
**Title:** feat(server): add stricter validation for group operations  
**Labels:** `backend`, `group`, `validation`  
**Priority:** P1

### Description
完善群组权限逻辑。

### Acceptance Criteria
- [ ] 创建群校验
- [ ] 加群校验
- [ ] 退群逻辑
- [ ] 群管理员角色

---

## Issue 7
**Title:** feat(security): hash user passwords instead of storing plaintext  
**Labels:** `security`, `backend`, `database`  
**Priority:** P1

### Description
将密码明文改为哈希存储。

### Acceptance Criteria
- [ ] 注册时哈希
- [ ] 登录时校验哈希
- [ ] README 更新

---

## Issue 8
**Title:** feat(client): support read/unread and message recall states  
**Labels:** `client`, `message`, `enhancement`  
**Priority:** P2

### Description
增强消息状态管理。

### Acceptance Criteria
- [ ] 已读状态
- [ ] 未读状态
- [ ] 消息撤回标记

---

## Issue 9
**Title:** test: add unit tests for models and service logic  
**Labels:** `test`, `backend`  
**Priority:** P1

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

### Description
新增 Python 管理后台骨架。

### Acceptance Criteria
- [ ] FastAPI 项目结构
- [ ] 健康检查接口
- [ ] 用户查询接口
- [ ] 好友关系查询接口
- [ ] 群组查询接口
- [ ] 离线消息查询接口

---

## Issue 12
**Title:** feat(admin): add admin APIs for user moderation and audit  
**Labels:** `python`, `admin`, `security`  
**Priority:** P2

### Description
管理后台支持封禁、审计、统计。

### Acceptance Criteria
- [ ] 封禁用户
- [ ] 审计日志
- [ ] 管理员操作记录

---

## Issue 13
**Title:** feat(deploy): add Docker Compose stack for chat server, mysql, redis and nginx  
**Labels:** `devops`, `docker`, `deployment`  
**Priority:** P0

### Description
提供一键部署。

### Acceptance Criteria
- [ ] `Dockerfile`
- [ ] `docker-compose.yml`
- [ ] 启动说明

---

## Issue 14
**Title:** feat(observability): expose metrics for Prometheus and Grafana  
**Labels:** `monitoring`, `observability`  
**Priority:** P2

### Description
增加监控能力。

### Acceptance Criteria
- [ ] 在线人数指标
- [ ] 消息吞吐量指标
- [ ] MySQL 耗时指标
- [ ] Redis 延迟指标

---

## Issue 15
**Title:** feat(ai): integrate Python moderation or chatbot service  
**Labels:** `python`, `ai`, `experimental`  
**Priority:** P3

### Description
为系统增加更高级的 AI 能力。

### Acceptance Criteria
- [ ] 敏感词审核
- [ ] 机器人回复
- [ ] 聊天摘要

