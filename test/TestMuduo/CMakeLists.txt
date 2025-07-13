cmake_minimum_required(VERSION 3.0)
project(main)

#配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

#配置头文件搜索路径
# include_directories()

#配置库文件搜索路径
# link_directories()

#设置需要编译的源文件列表
set(SRC_LIST ./muduo_Server.cc)

#设置可执行文件最终存放的位置
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#指定路径下的所有源文件
# aux_source_directory(. SRC_LIST)

# 生成可执行文件
add_executable(server ${SRC_LIST})

# server程序链接的库文件
target_link_libraries(server muduo_net muduo_base pthread)