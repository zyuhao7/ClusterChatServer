# admin_service

这是一个为 `ClusterChatServer` 准备的 **Python FastAPI 管理后台骨架**。

目标：
- 查询用户
- 查询好友关系
- 查询群组
- 查询离线消息
- 提供健康检查接口

当前版本是**骨架工程**，重点是：
- 目录结构清晰
- 配置方式清晰
- 接口可扩展

---

## 目录结构

```text
admin_service/
├── README.md
├── requirements.txt
├── .env.example
└── app/
    ├── main.py
    ├── core/
    ├── db/
    ├── models/
    ├── routers/
    └── schemas/
```

---

## 依赖安装

建议使用虚拟环境：

```bash
cd admin_service
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

---

## 配置

复制环境变量模板：

```bash
cp .env.example .env
```

默认配置示例：

```env
APP_NAME=ClusterChat Admin Service
APP_HOST=127.0.0.1
APP_PORT=8010
DEBUG=true

MYSQL_HOST=127.0.0.1
MYSQL_PORT=3306
MYSQL_USER=root
MYSQL_PASSWORD=123456
MYSQL_DATABASE=chat

ADMIN_TOKEN=change-me
REDIS_HOST=127.0.0.1
REDIS_PORT=6379
MODERATION_SENSITIVE_WORDS=spam,ad,scam
```

---

## 启动

```bash
cd /home/xh/ClusterChatServer
source admin_service/.venv/bin/activate
uvicorn admin_service.app.main:app --reload --host 127.0.0.1 --port 8010
```

---

## 快速验证

```bash
curl -sf http://127.0.0.1:8010/health
curl -sf http://127.0.0.1:8010/metrics | sed -n '1,10p'
curl -sf -H "X-Admin-Token: change-me" \
  -X POST http://127.0.0.1:8010/api/v1/admin/users/1/ban \
  -H "Content-Type: application/json" \
  -d '{"reason":"manual-check"}'
```

运行测试：

```bash
cd /home/xh/ClusterChatServer
source admin_service/.venv/bin/activate
pytest tests/integration/test_admin_service.py -q
```

---

## 当前接口

- `GET /`
- `GET /health`
- `GET /health/db`
- `GET /metrics`
- `GET /api/v1/users` (`state=online|offline|banned`)
- `GET /api/v1/users/{user_id}`
- `GET /api/v1/friends`
- `GET /api/v1/groups`
- `GET /api/v1/offline-messages`
- `GET /api/v1/history`
- `POST /api/v1/admin/users/{user_id}/ban`
- `POST /api/v1/admin/users/{user_id}/unban`
- `GET /api/v1/admin/audit-logs`
- `GET /api/v1/admin/operation-logs`
- `POST /api/v1/ai/moderate`
- `POST /api/v1/ai/chatbot`
- `POST /api/v1/ai/summary`

---

## 运行说明

- 推荐从仓库根目录启动，而不是进入 `admin_service/app` 子目录
- `.env` 放在 `admin_service/.env`
- 启动前请确保：
  - MySQL 已启动
  - `chat` 数据库已初始化
  - `.env` 中数据库配置正确
  - 调用 `/api/v1/admin/*` 时提供请求头 `X-Admin-Token`

如果暂时只想验证服务已启动成功，可先访问：

- `GET /`
- `GET /health`

如果要验证数据库可访问，可请求：

- `GET /health/db`

---

## 后续建议

- 对接前端管理页面
- 扩展更细粒度 RBAC
- 把 AI mock 流程替换为真实模型服务
