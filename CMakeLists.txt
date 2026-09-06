cmake_minimum_required(VERSION 3.20)
project(word-list-builder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET_NAME ${PROJECT_NAME})

add_executable(${PROJECT_NAME}
    src/main.cpp
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)