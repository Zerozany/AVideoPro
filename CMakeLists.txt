cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

set(CMAKE_TOOLCHAIN_FILE "F:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(avideoPro VERSION 1.0)

find_package(FFMPEG REQUIRED)
find_package(boost_url CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED
    COMPONENTS
    Widgets
    Core
    Multimedia
    MultimediaWidgets
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED
    COMPONENTS
    Widgets
    Core
    Multimedia
    MultimediaWidgets
)

if(MSVC)
    add_compile_options(/utf-8 /Zc:preprocessor) # 仅对 MSVC 生效
else()
    add_compile_options(-Wall -Wextra) # MinGW/GCC 的选项
endif()

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
    AUTORCC ON
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

file(GLOB SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

target_compile_features(${PROJECT_NAME}
    PRIVATE
    cxx_std_23)

target_sources(${PROJECT_NAME}
    PRIVATE
    ${SOURCE_FILES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_LIBRARY_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    spdlog::spdlog
    Boost::url
    ${FFMPEG_LIBRARIES}
    ${OpenCV_LIBS}
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)
