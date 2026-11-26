# 要求 CMake 的最低版本
cmake_minimum_required(VERSION 3.10)

# 项目名称和使用的语言
project(MyAVProject LANGUAGES CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果是 Debug 构建，加上调试信息；如果是 Release，加上 -O2 优化
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 🌟 核心魔法：把所有可执行文件统一输出到根目录的 bin 文件夹
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 找到多线程库（任务 1 多线程 server 需要）
find_package(Threads REQUIRED)

# --- 1. 编译服务端（多线程版） ---
add_executable(server src/server.cpp)
target_link_libraries(server PRIVATE Threads::Threads)

# --- 2. 编译客户端 ---
add_executable(client src/client.cpp)
target_link_libraries(client PRIVATE Threads::Threads)

# --- 3. 编译 PCM 布局测试 ---
# 注意：截图里源码叫 pcm_layout_demo.cpp，但我们编译出来的程序叫 pcm_demo
add_executable(pcm_demo src/pcm_layout_demo.cpp)

# --- 4. 编译 epoll 服务端（新增） ---
add_executable(epoll_server src/epoll_server.cpp)

# 寻找 FFmpeg 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)

# 添加解封装的可执行文件
add_executable(demux_demo src/demux_demo.cpp)

# 链接 FFmpeg 库
target_include_directories(demux_demo PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(demux_demo PRIVATE ${FFMPEG_LIBRARIES})