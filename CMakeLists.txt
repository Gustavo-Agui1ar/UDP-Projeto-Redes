cmake_minimum_required(VERSION 3.10)

project(udp_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(OpenSSL REQUIRED)

add_executable(udp_server ${SOURCES})

target_link_libraries(udp_server PRIVATE OpenSSL::Crypto)
