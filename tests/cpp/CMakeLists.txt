cmake_minimum_required(VERSION 3.16)
project(ClusterChatServerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    cluster_chat_unit
    smoke_test.cc
    ../../src/server/config/appconfig.cc
    ../../src/server/security/password.cc
)

target_include_directories(
    cluster_chat_unit
    PRIVATE
    ../../include/server/config
    ../../include/server/security
)

target_link_libraries(cluster_chat_unit PRIVATE crypt)
