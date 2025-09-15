cmake_minimum_required(VERSION 3.10)

project(udp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Fontes comuns (Protocol)
set(PROTOCOL_SOURCES
    src/Protocol/ChromaProtocol.cpp
)

# Cliente
add_executable(udp_client
    src/main_client.cpp
    src/Client/ChromaClient.cpp
    ${PROTOCOL_SOURCES}
)

target_link_libraries(udp_client PRIVATE OpenSSL::Crypto)

# Servidor/Manager
add_executable(udp_manager
    src/main_manager.cpp
    src/Server/ChromaServer.cpp
    src/Server/ChromaServerManager.cpp
    ${PROTOCOL_SOURCES}
)

target_link_libraries(udp_manager PRIVATE OpenSSL::Crypto)
