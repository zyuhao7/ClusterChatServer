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

---

## 目录结构

```text
ClusterChatServer/
├── CMakeLists.txt
├── README.md
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

数据库连接配置在：

```text
include/server/db/db.h
```

默认配置：

- MySQL 地址：`127.0.0.1`
- 用户名：`root`
- 密码：`123456`
- 数据库：`chat`

如果你的本机配置不同，请先修改这里。

### 初始化数据库

仓库根目录已提供初始化脚本：

```bash
mysql -uroot -p < chat.sql
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
include/server/db/db.h
```

中的数据库用户名和密码。

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

## 运行程序

### 启动服务器

```bash
./bin/chat_server 127.0.0.1 9120
```

再开一个终端启动第二个实例：

```bash
./bin/chat_server 127.0.0.1 9121
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
./bin/chat_server 127.0.0.1 9120
./bin/chat_server 127.0.0.1 9121
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
- `include/server/db/db.h` 里的用户名密码不对

---


