# ClusterChatServer

C++ 实现的集群聊天服务器，支持多服务器负载均衡与跨服务器通信。

## 项目介绍

本项目基于：

- **网络库**：muduo
- **序列化**：nlohmann/json
- **消息队列/跨服转发**：Redis
- **数据库**：MySQL
- **负载均衡**：nginx
- **构建工具**：CMake

当前仓库已经补齐：

- CMake 依赖检测与链接配置
- 客户端/服务端输出文件名与 README 一致
- `example/muduo_client.cc`
- 示例 `makefile`
- 缺失的标准头文件
- `.gitignore` 已覆盖构建产物、Python 虚拟环境与本地临时文件
- `admin_service` 已具备基础管理接口骨架

当前版本新增：

- 统一配置文件：`config/server.conf`
- 数据库迁移脚本：`db/migrations/`
- 消息历史、已读/撤回基础字段
- 统一 ACK 结构、协议 `version` 与 `request_id`
- 统一错误码常量，服务端/客户端/验证脚本使用同一套编号
- 离线消息条数限制，可配置保留最近 N 条
- 聊天消息基于 `request_id` 做幂等，避免重复投递
- 服务启动/退出时会恢复在线用户状态，降低异常退出残留影响
- 群组操作补充权限边界校验，避免非法创建群或修改群主角色
- 支持私聊/群聊历史消息分页查询
- 支持私聊/群聊历史消息按时间升序或降序查询
- 支持搜索用户、黑名单增删与黑名单拦截发消息/加好友
- 群组权限校验、退群、角色调整
- bcrypt 密码哈希与迁移脚本
- 管理后台审计/封禁接口
- Prometheus `/metrics`
- AI mock 能力：敏感词审核、机器人回复、聊天摘要
- Docker Compose / Prometheus / Grafana 部署骨架

---

## 目录结构

```text
ClusterChatServer/
├── CMakeLists.txt
├── README.md
├── admin_service/              # Python FastAPI 管理后台骨架
├── example/
│   ├── makefile
│   ├── muduo_Server.cc
│   ├── muduo_client.cc
│   └── testJson.cc
├── include/
├── src/
├── test/
└── thirdparty/
    └── json.hpp
```

---

## 依赖安装（Ubuntu 24.04）

### 1）安装系统依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake g++ git \
  libboost-dev \
  libhiredis-dev \
  default-libmysqlclient-dev \
  mysql-server \
  redis-server \
  nginx
```

如果 `apt` 下载很慢或失败，可以切换到清华等国内镜像源后再执行以上命令。

### 1.1）Ubuntu 24.04 切换到清华源

Ubuntu 24.04 默认通常使用：

```text
/etc/apt/sources.list.d/ubuntu.sources
```

先备份原配置：

```bash
sudo cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.bak
```

写入清华源（普通仓库走清华，安全更新走官方源，较稳妥）：

```bash
sudo tee /etc/apt/sources.list.d/ubuntu.sources > /dev/null <<'EOF'
Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
Suites: noble noble-updates noble-backports
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://security.ubuntu.com/ubuntu/
Suites: noble-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
```

更新索引：

```bash
sudo apt update
```

如果你想连 `security` 也切到清华，可以改成：

```bash
sudo tee /etc/apt/sources.list.d/ubuntu.sources > /dev/null <<'EOF'
Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/ubuntu/
Suites: noble noble-updates noble-backports noble-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
```

恢复官方源：

```bash
sudo mv /etc/apt/sources.list.d/ubuntu.sources.bak /etc/apt/sources.list.d/ubuntu.sources
sudo apt update
```

### 2）下载并安装 muduo

Ubuntu 24.04 的默认软件源里通常没有现成的 muduo 开发包，需要单独下载源码编译。

```bash
cd /tmp
git clone https://github.com/chenshuo/muduo.git
cd muduo
# Ubuntu 24.04 / GCC 13 下如遇 muduo 因 Werror 报错，可先去掉 CMakeLists.txt 里的 -Werror
./build.sh
./build.sh install
```

`./build.sh install` 默认会把 muduo 安装到类似下面的目录：

```text
/tmp/release-install-cpp11
```

如果你想把 muduo 安装到系统目录，也可以手动拷贝：

```bash
sudo cp -r /tmp/release-install-cpp11/include/muduo /usr/local/include/
sudo cp -d /tmp/release-install-cpp11/lib/libmuduo_*.so* /usr/local/lib/
sudo ldconfig
```

如果不拷贝到 `/usr/local`，构建本项目时请显式指定 muduo 安装前缀：

```bash
cmake -S . -B build -DMUDUO_ROOT=/tmp/release-install-cpp11
```

### 3）启动基础服务

```bash
sudo systemctl enable --now mysql
sudo systemctl enable --now redis-server
sudo systemctl enable --now nginx
```

---

## 需要的库信息

本项目服务端最终会链接以下库：

- `muduo_net`
- `muduo_base`
- `mysqlclient`（或 `mariadb` 兼容库）
- `hiredis`
- `pthread`

头文件依赖主要包括：

- `muduo/net/TcpServer.h`
- `muduo/net/TcpClient.h`
- `muduo/net/EventLoop.h`
- `mysql/mysql.h`
- `hiredis/hiredis.h`
- `thirdparty/json.hpp`

---

## 数据库配置

数据库连接配置默认从以下文件读取：

```text
config/server.conf
```

默认配置：

- MySQL 地址：`127.0.0.1`
- 用户名：`root`
- 密码：`123456`
- 数据库：`chat`

如果你的本机配置不同，请先修改该配置文件。

### 服务启动参数

```bash
./bin/chat_server --config config/server.conf
./bin/chat_server --config config/server.conf 127.0.0.1 9120
```

### 初始化数据库

仓库根目录已提供初始化脚本：

```bash
mysql -uroot -p < chat.sql
```

已有数据库升级可按顺序执行：

```bash
mysql -uroot -p chat < db/migrations/001_constraints_and_indexes.sql
mysql -uroot -p chat < db/migrations/002_message_history_and_admin_tables.sql
mysql -uroot -p chat < db/migrations/003_password_hashing_prep.sql
mysql -uroot -p chat < db/migrations/004_user_blacklist.sql
```

如果在 Ubuntu 上执行时出现：

```text
ERROR 1698 (28000): Access denied for user 'root'@'localhost'
```

通常是因为 Ubuntu 默认把 MySQL `root` 配置成了 `auth_socket` 认证，而不是密码登录。

此时可以直接使用：

```bash
sudo mysql < chat.sql
```

如果你希望继续使用 `mysql -uroot -p`，可以先执行：

```bash
sudo mysql
```

进入 MySQL 后查看认证方式：

```sql
SELECT user, host, plugin FROM mysql.user;
```

如果 `root@localhost` 使用的是 `auth_socket`，可以改成密码认证：

```sql
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '你的新密码';
FLUSH PRIVILEGES;
```

退出后再执行：

```bash
mysql -uroot -p < chat.sql
```

更推荐的做法是单独创建项目用户，而不是直接修改 root：

```bash
sudo mysql
```

```sql
CREATE DATABASE IF NOT EXISTS chat;
CREATE USER 'chatuser'@'localhost' IDENTIFIED BY 'chatpass';
GRANT ALL PRIVILEGES ON chat.* TO 'chatuser'@'localhost';
FLUSH PRIVILEGES;
```

然后导入：

```bash
mysql -uchatuser -p chat < chat.sql
```

如果使用项目用户，记得同步修改：

```text
config/server.conf
```

中的数据库用户名和密码。

密码哈希一次性迁移脚本：

```bash
python3 scripts/migrate_passwords.py --user root --database chat
```

回滚建议见：

```text
db/MIGRATIONS.md
```

---

## 编译项目

### 使用系统目录中的 muduo

```bash
cmake -S . -B build
cmake --build build -j
```

### 使用自定义 muduo 安装目录

```bash
cmake -S . -B build -DMUDUO_ROOT=/tmp/release-install-cpp11
cmake --build build -j
```

如果 MySQL/hiredis 也是下载后解压到自定义目录，而不是系统安装，可继续追加：

```bash
cmake -S . -B build \
  -DMUDUO_ROOT=/tmp/release-install-cpp11 \
  -DMYSQL_ROOT=/你的/mysql前缀 \
  -DHIREDIS_ROOT=/你的/hiredis前缀
cmake --build build -j
```

编译完成后可执行文件输出到：

```text
bin/chat_server
bin/chat_client
```

---

## Python 管理后台：admin_service

仓库还提供了一个独立的 **Python FastAPI 管理后台骨架**：

```text
admin_service/
```

它不替代当前 C++ 聊天服务，而是作为辅助管理与查询模块使用，适合后续做：

- 用户查询
- 好友关系查询
- 群组查询
- 离线消息查询
- 健康检查
- 后续扩展为管理后台 / 运营后台 / 审计接口

### 目录结构

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

### 安装依赖

```bash
cd admin_service
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### 配置环境变量

```bash
cd admin_service
cp .env.example .env
```

默认示例：

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

### 启动管理后台

```bash
cd /home/xh/ClusterChatServer
source admin_service/.venv/bin/activate
uvicorn admin_service.app.main:app --reload --host 127.0.0.1 --port 8010
```

### 当前接口

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

### 说明

- 当前 `admin_service` 是骨架版本，重点在于提供可扩展结构
- 推荐从仓库根目录启动，而不是进入 `admin_service/app` 子目录启动
- 如果 VS Code / Pylance 提示找不到 `sqlalchemy`、`fastapi` 等模块，通常是因为：
  - 还没有安装 `requirements.txt`
  - 或 VS Code 没切换到 `admin_service/.venv` 解释器

建议在 VS Code 中选择：

```text
Python: Select Interpreter -> admin_service/.venv/bin/python
```

管理员接口需要请求头：

```text
X-Admin-Token: <ADMIN_TOKEN>
```

---

## 运行程序

### 启动服务器

```bash
./bin/chat_server --config config/server.conf 127.0.0.1 9120
```

再开一个终端启动第二个实例：

```bash
./bin/chat_server --config config/server.conf 127.0.0.1 9121
```

### nginx TCP 负载均衡示例

### 配置步骤

Ubuntu 24.04 默认的 `/etc/nginx/nginx.conf` 中：

```nginx
http {
    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites-enabled/*;
}
```

也就是说 `/etc/nginx/conf.d/*.conf` 是在 `http {}` 中加载的，**不能直接把 `stream {}` 写进 `conf.d`**，否则会报：

```text
"stream" directive is not allowed here
```

因此建议按下面步骤配置。

#### 1）安装 stream 模块

```bash
sudo apt update
sudo apt install -y nginx libnginx-mod-stream
```

#### 2）创建 stream 配置目录

```bash
sudo mkdir -p /etc/nginx/stream-conf.d
```

#### 3）修改 `/etc/nginx/nginx.conf`

执行：

```bash
sudo vim /etc/nginx/nginx.conf
```

在 `events { ... }` 后、`http { ... }` 前加入：

```nginx
include /etc/nginx/stream-conf.d/*.conf;
```

修改后大致如下：

```nginx
user www-data;
worker_processes auto;
pid /run/nginx.pid;
error_log /var/log/nginx/error.log;
include /etc/nginx/modules-enabled/*.conf;

events {
    worker_connections 768;
}

include /etc/nginx/stream-conf.d/*.conf;

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    include /etc/nginx/conf.d/*.conf;
    include /etc/nginx/sites-enabled/*;
}
```

#### 4）创建聊天服务的 TCP 负载均衡配置

执行：

```bash
sudo vim /etc/nginx/stream-conf.d/chat.conf
```

写入：

```nginx
stream {
    upstream chat_servers {
        server 127.0.0.1:9120 weight=1;
        server 127.0.0.1:9121 weight=1;
    }

    server {
        listen 8000;
        proxy_pass chat_servers;
        proxy_connect_timeout 5s;
        proxy_timeout 1h;
    }
}
```

#### 5）检查配置

```bash
sudo nginx -t
```

如果输出类似：

```text
nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
nginx: configuration file /etc/nginx/nginx.conf test is successful
```

说明配置正确。

#### 6）重载 nginx

```bash
sudo nginx -t
sudo systemctl reload nginx
```

如果 nginx 尚未启动：

```bash
sudo systemctl enable --now nginx
```

#### 7）启动后端聊天服务

```bash
./bin/chat_server --config config/server.conf 127.0.0.1 9120
./bin/chat_server --config config/server.conf 127.0.0.1 9121
```

### 启动客户端

```bash
./bin/chat_client 127.0.0.1 8000
```

如果不走 nginx，也可以直接连某台服务端：

```bash
./bin/chat_client 127.0.0.1 9120
```

---

## Docker 与观测

一键启动：

```bash
docker compose up --build -d
```

或：

```bash
bash scripts/run_stack.sh
```

默认暴露端口：

- chat_server: `9120`
- nginx stream: `8000`
- admin_service: `8010`
- Prometheus: `9090`
- Grafana: `3000`

---

## 测试与快速自检

### C++ smoke test

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j --target cluster_chat_unit
./build/cluster_chat_unit
```

### Python 集成测试（admin_service）

```bash
cd /home/xh/ClusterChatServer
source admin_service/.venv/bin/activate
pytest tests/integration/test_admin_service.py -q
```

### 脚本化健康检查

```bash
bash scripts/check_admin_service.sh
bash scripts/check_admin_metrics.sh
python3 scripts/check_server_roundtrip.py
```

---

## example 目录使用说明

### 编译 example

```bash
cd example
make
```

生成：

- `muduo_server`
- `muduo_client`
- `testJson`

如果 muduo 不在 `/usr/local`，可以这样编译：

```bash
make MUDUO_PREFIX=/tmp/release-install-cpp11

# 如果系统没有安装 boost 头文件，可额外指定 boost 头文件目录
make MUDUO_PREFIX=/tmp/release-install-cpp11 BOOST_INC=/usr/include
```

### 运行 muduo 示例

先启动服务端：

```bash
cd example
./muduo_server
```

再启动客户端：

```bash
cd example
./muduo_client 127.0.0.1 9120
```

输入任意文本后回车即可看到服务端回显；输入 `quit` 退出。

---

## 常见问题

### 1）`fatal error: muduo/net/TcpServer.h: No such file or directory`

说明 muduo 没安装，或者没被 CMake 找到。

优先检查：

```bash
ls /usr/local/include/muduo/net/TcpServer.h
ls /usr/local/lib/libmuduo_net.so
```

如果 muduo 安装在自定义目录，使用：

```bash
cmake -S . -B build -DMUDUO_ROOT=/你的/muduo/安装目录
```

### 2）找不到 `mysql/mysql.h`

安装：

```bash
sudo apt install -y default-libmysqlclient-dev
```

### 3）找不到 `hiredis/hiredis.h`

安装：

```bash
sudo apt install -y libhiredis-dev
```

### 4）客户端能连上，登录/注册失败

通常不是网络问题，而是：

- MySQL 没启动
- `chat` 数据库/表没创建
- `config/server.conf` 里的数据库用户名密码不对

---
