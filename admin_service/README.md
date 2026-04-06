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
```

---

## 启动

```bash
cd admin_service
source .venv/bin/activate
uvicorn app.main:app --reload --host 127.0.0.1 --port 8010
```

---

## 当前接口

- `GET /`
- `GET /health`
- `GET /api/v1/users`
- `GET /api/v1/users/{user_id}`
- `GET /api/v1/friends`
- `GET /api/v1/groups`
- `GET /api/v1/offline-messages`

---

## 后续建议

- 增加管理员鉴权
- 增加封禁用户接口
- 增加统计接口
- 增加 Prometheus 指标
- 对接前端管理页面

