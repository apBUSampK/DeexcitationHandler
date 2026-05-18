cmake_minimum_required(VERSION 3.16)
project(
    DeexcitationRunner
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)

set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${LIB_PATH} ${CMAKE_BINARY_DIR}/Deexcitation)

add_executable(Runner main.cpp)
target_link_libraries(Runner Deexcitation)
target_include_directories(Runner PUBLIC ${LIB_PATH})

file(CREATE_LINK ${PROJECT_SOURCE_DIR}/config.xml config.xml)
