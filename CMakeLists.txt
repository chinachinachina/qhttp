# CMake最低版本要求
cmake_minimum_required (VERSION 2.8)

# 项目信息
project (http_kv)

# 设置gdb调试版本
set(CMAKE_CXX_FLAGS "-O0 -ggdb")

# 指定生成目标
add_executable(run run.cc server.cc epoll_event.cc)