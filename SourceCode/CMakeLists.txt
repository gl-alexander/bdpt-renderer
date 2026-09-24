cmake_minimum_required(VERSION 3.16)
project(BDPTRenderer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# rapidjson uses MSVC-style pragma spellings that clang/gcc don't recognize.
add_compile_options(-Wno-unknown-warning-option)

# Shared engine code (everything except the main() entry point). SourceCode/ is
# the include root (headers use paths like "Utils/Vector.h" and "Scene/Scene.h").
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Scene/*.cpp"
    "${CMAKE_SOURCE_DIR}/Utils/*.cpp")
list(APPEND ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/Raytracer.cpp"
    "${CMAKE_SOURCE_DIR}/Pathtracer.cpp")

add_library(engine STATIC ${ENGINE_SOURCES})
target_include_directories(engine PUBLIC "${CMAKE_SOURCE_DIR}")

find_package(Threads REQUIRED)
target_link_libraries(engine PUBLIC Threads::Threads)

add_executable(renderer "${CMAKE_SOURCE_DIR}/main.cpp")
target_link_libraries(renderer PRIVATE engine)
