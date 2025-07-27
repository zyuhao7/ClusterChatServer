# ClusterChatServer

C++ 实现的集群聊天服务器，支持多服务器负载均衡与跨服务器通信。

## 项目介绍
本项目是一个基于C++的高性能集群聊天服务器，采用模块化设计思想，实现了完整的即时通讯功能。系统支持多服务器部署，并通过nginx实现TCP负载均衡，确保高可用性和可扩展性。

## 功能特性
- 用户注册与登录认证
- 好友管理（添加/删除好友）
- 群组管理（创建/加入/退出群组）
- 一对一即时聊天
- 群组聊天
- 离线消息存储与同步
- 跨服务器通信支持
- nginx TCP负载均衡

## 技术栈
- **网络库**：muduo网络库
- **数据序列化**：Json
- **负载均衡**：nginx
- **缓存/消息队列**：Redis
- **数据库**：MySQL
- **构建工具**：CMake
- **版本控制**：GitHub

## 项目结构
```
ClusterChatServer-master/
├── CMakeLists.txt               # 项目主构建配置文件
├── README.md                    # 项目说明文档
├── example/                     # 示例代码目录
│   ├── Server/                  # 服务器示例代码
│   ├── makefile                 # 示例代码编译脚本
│   ├── muduo_Server.cc          # Muduo服务器实现示例
│   ├── muduo_client.cc          # Muduo客户端实现示例
│   └── testJson.cc              # JSON功能测试代码
├── include/                     # 公共头文件目录
│   ├── public.hpp               # 全局公共定义
│   └── server/                  # 服务器相关头文件
├── src/                         # 源代码目录
│   ├── CMakeLists.txt           # 源代码构建配置
│   ├── client/                  # 客户端源代码
│   └── server/                  # 服务器源代码
├── test/                        # 测试代码目录
│   ├── TestJson/                # JSON测试模块
│   └── TestMuduo/               # Muduo网络库测试模块
└── thirdparty/                  # 第三方依赖库
    └── json.hpp                 # JSON序列化/反序列化库
```
## 构建与安装
### 环境要求
- C++11及以上编译器
- muduo网络库
- Redis服务器
- MySQL数据库
- nginx
- CMake 3.0+

### 编译步骤
```bash
# 创建构建目录
mkdir build && cd build

# 生成Makefile
cmake ..

# 编译项目
make
```

## 配置说明
### nginx负载均衡配置
```nginx
stream {
    upstream chat_servers {
        server 127.0.0.1:9120 weight=1;
        server 127.0.0.1:9121 weight=1;
        # 添加更多服务器...
    }

    server {
        listen 8000;
        proxy_pass chat_servers;
    }
}
```

## 使用示例
### 启动服务器
```bash
# 启动服务器实例1
./bin/chat_server 127.0.0.1 9120

# 启动服务器实例2
./bin/chat_server 127.0.0.1 9121

# 启动nginx负载均衡
nginx -c /path/to/nginx.conf
```

### 启动客户端
```bash
./bin/chat_client 127.0.0.1 8000
```

## 开发目标
1. 掌握服务器的网络I/O模块、业务模块、数据模块分层设计思想
2. 熟悉C++ muduo网络库的编程及实现原理
3. 掌握Json数据格式的序列化与反序列化
4. 理解nginx配置部署及TCP负载均衡原理
5. 掌握Redis在缓存和消息队列场景的应用
6. 熟练使用CMake构建自动化编译环境
7. 学习GitHub项目管理与协作流程

