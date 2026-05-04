cmake_minimum_required(VERSION 3.16)
project(campus_trade_cpp_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure Chinese string literals are compiled as UTF-8 on MSVC.
if (MSVC)
    add_compile_options(/utf-8)
endif()

# MySQL 安装路径（可通过 -DMYSQL_ROOT=... 覆盖）
set(MYSQL_ROOT "D:/apps/mysql" CACHE PATH "Path to MySQL installation root")
set(MYSQL_INCLUDE_DIR "${MYSQL_ROOT}/include")
if (WIN32)
    set(MYSQL_LIB "${MYSQL_ROOT}/lib/libmysql.lib")
else()
    set(MYSQL_LIB "${MYSQL_ROOT}/lib/libmysqlclient.so")
endif()

add_executable(campus_trade_web
    src/HttpServer.cpp
    src/HttpServerPages.cpp
    src/HttpServerStatic.cpp
    src/http_main.cpp
)

target_include_directories(campus_trade_web PRIVATE
    include
    "${MYSQL_INCLUDE_DIR}"
)

target_link_libraries(campus_trade_web PRIVATE "${MYSQL_LIB}")
if (WIN32)
    target_link_libraries(campus_trade_web PRIVATE ws2_32)
endif()
