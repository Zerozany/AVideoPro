cmake_minimum_required(VERSION 3.29 FATAL_ERROR)

set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "Static link dynamic call")
project(avideoPro VERSION 1.0)

find_package(toml11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

file(GLOB SOURCE_FILES RELATIVE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_sources(${PROJECT_NAME}
    PRIVATE
    ${SOURCE_FILES}
)

target_compile_features(${PROJECT_NAME}
    PRIVATE
    cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inclue
    ${FFMPEG_INCLUDE_DIRS})

target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    toml11::toml11
    spdlog::spdlog
    ${FFMPEG_LIBRARIES})
