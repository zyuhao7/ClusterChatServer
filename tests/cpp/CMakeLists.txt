cmake_minimum_required(VERSION 3.16)
project(ClusterChatServerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(
    cluster_chat_unit
    smoke_test.cc
    db_fixture.cc
    ../../src/server/config/appconfig.cc
    ../../src/server/db/db.cc
    ../../src/server/modal/usermodal.cc
    ../../src/server/modal/messagehistorymodal.cc
    ../../src/server/security/password.cc
)

target_compile_definitions(
    cluster_chat_unit
    PRIVATE
    CLUSTER_CHAT_SOURCE_DIR="${REPO_ROOT}"
)

target_include_directories(
    cluster_chat_unit
    PRIVATE
    ../../include/server/config
    ../../include/server/security
)

target_link_libraries(
    cluster_chat_unit
    PRIVATE
    ${MYSQLCLIENT_LIBRARY}
    ${MUDUO_BASE_LIBRARY}
    crypt
    Threads::Threads
)
