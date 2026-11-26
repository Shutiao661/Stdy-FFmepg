cmake_minimum_required(VERSION 3.10)
project(MyAVProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 多线程库
find_package(Threads REQUIRED)

# FFmpeg 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswresample)

# --- 网络部分 (任务1/2) ---
add_executable(server src/server.cpp)
target_link_libraries(server PRIVATE Threads::Threads)

add_executable(client src/client.cpp)
target_link_libraries(client PRIVATE Threads::Threads)

add_executable(epoll_server src/epoll_server.cpp)
target_link_libraries(epoll_server PRIVATE Threads::Threads)

# --- FFmpeg 部分 (任务3/4/5) ---
add_executable(demux_demo src/demux_demo.cpp)
target_include_directories(demux_demo PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(demux_demo PRIVATE ${FFMPEG_LIBRARIES})

add_executable(read_packet_demo src/read_packet_demo.cpp)
target_include_directories(read_packet_demo PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(read_packet_demo PRIVATE ${FFMPEG_LIBRARIES})

add_executable(decode_video_demo src/decode_video_demo.cpp)
target_include_directories(decode_video_demo PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(decode_video_demo PRIVATE ${FFMPEG_LIBRARIES})

add_executable(decode_audio_demo src/decode_audio_demo.cpp)
target_include_directories(decode_audio_demo PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(decode_audio_demo PRIVATE ${FFMPEG_LIBRARIES})