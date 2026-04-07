cmake_minimum_required(VERSION 3.16)
project(ClusterChatServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
option(BUILD_TESTS "Build unit tests" ON)

# 配置最终可执行文件的输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

set(MUDUO_ROOT "" CACHE PATH "Muduo installation prefix")
set(MYSQL_ROOT "" CACHE PATH "MySQL client installation prefix")
set(HIREDIS_ROOT "" CACHE PATH "hiredis installation prefix")

find_path(MUDUO_INCLUDE_DIR
    NAMES muduo/net/TcpServer.h
    HINTS
    ${MUDUO_ROOT}/include
    /usr/local/include
    /usr/include
)
find_library(MUDUO_NET_LIBRARY
    NAMES muduo_net
    HINTS
    ${MUDUO_ROOT}/lib
    ${MUDUO_ROOT}/lib64
    /usr/local/lib
    /usr/local/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
    /usr/lib
)
find_library(MUDUO_BASE_LIBRARY
    NAMES muduo_base
    HINTS
    ${MUDUO_ROOT}/lib
    ${MUDUO_ROOT}/lib64
    /usr/local/lib
    /usr/local/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
    /usr/lib
)

find_path(MYSQL_INCLUDE_DIR
    NAMES mysql/mysql.h
    HINTS
    ${MYSQL_ROOT}/include
    /usr/include
    /usr/local/include
)
find_library(MYSQLCLIENT_LIBRARY
    NAMES mysqlclient mariadb
    HINTS
    ${MYSQL_ROOT}/lib
    ${MYSQL_ROOT}/lib64
    ${MYSQL_ROOT}/lib/x86_64-linux-gnu
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib
    /usr/local/lib64
    /usr/lib64
    /usr/lib
)

find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h
    HINTS
    ${HIREDIS_ROOT}/include
    /usr/include
    /usr/local/include
)
find_library(HIREDIS_LIBRARY
    NAMES hiredis
    HINTS
    ${HIREDIS_ROOT}/lib
    ${HIREDIS_ROOT}/lib64
    ${HIREDIS_ROOT}/lib/x86_64-linux-gnu
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib
    /usr/local/lib64
    /usr/lib64
    /usr/lib
)

if(NOT MUDUO_INCLUDE_DIR OR NOT MUDUO_NET_LIBRARY OR NOT MUDUO_BASE_LIBRARY)
    message(FATAL_ERROR
        "Muduo not found. Please install muduo first. "
        "See README.md -> 依赖安装（Ubuntu 24.04）."
    )
endif()

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQLCLIENT_LIBRARY)
    message(FATAL_ERROR
        "MySQL client development files not found. "
        "Please install default-libmysqlclient-dev or libmariadb-dev-compat. "
        "See README.md -> 依赖安装（Ubuntu 24.04）."
    )
endif()

if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR
        "hiredis not found. Please install libhiredis-dev. "
        "See README.md -> 依赖安装（Ubuntu 24.04）."
    )
endif()

# 配置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/modal)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/include/server/config)
include_directories(${PROJECT_SOURCE_DIR}/include/server/security)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
include_directories(${MUDUO_INCLUDE_DIR})
include_directories(${MYSQL_INCLUDE_DIR})
include_directories(${HIREDIS_INCLUDE_DIR})

# 加载子目录
add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(tests/cpp)
endif()
