cmake_minimum_required(VERSION 3.16)

project(TicTacToeQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

qt_add_executable(tictactoe_client
    client/client.cpp
)

target_link_libraries(tictactoe_client
    PRIVATE
        Qt6::Widgets
        Qt6::Network
)

qt_add_executable(tictactoe_server
    server/server.cpp
)

target_link_libraries(tictactoe_server
    PRIVATE
        Qt6::Core
        Qt6::Network
)
