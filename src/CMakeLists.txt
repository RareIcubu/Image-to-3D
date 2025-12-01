cmake_minimum_required(VERSION 3.16)
project(ImageTo3D VERSION 0.2.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core5Compat)
find_package(OpenCV REQUIRED)

# Znajdź ONNX Runtime
find_library(ONNX_LIB NAMES onnxruntime PATHS /usr/local/lib REQUIRED)
include_directories(/usr/local/include)

# Generowanie config.h
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Symlink do modeli (żeby nie kopiować ich przy każdym buildzie w devie)
file(CREATE_LINK 
    "${CMAKE_CURRENT_SOURCE_DIR}/models" 
    "${CMAKE_CURRENT_BINARY_DIR}/models" 
    SYMBOLIC
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(ImageTo3D
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    reconstructionmanager.cpp
    reconstructionmanager.h
    ai_reconstructionmanager.cpp 
    ai_reconstructionmanager.h
)

target_link_libraries(ImageTo3D PRIVATE
    Qt6::Widgets
    Qt6::Core5Compat
    ${OpenCV_LIBS}
    ${ONNX_LIB}
)
