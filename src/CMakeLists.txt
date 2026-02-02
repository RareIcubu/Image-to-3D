cmake_minimum_required(VERSION 3.16)


project(ImageTo3D VERSION 0.9.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

find_package(Open3D REQUIRED)

# --- 1. ZALEŻNOŚCI (Quick3D, nie Qt3D!) ---
find_package(Qt6 REQUIRED COMPONENTS 
    Widgets 
    Core5Compat 
    Quick 
    Quick3D        # <--- To jest nowoczesny silnik
    QuickWidgets   # <--- Z "s" na końcu!
)
find_package(OpenCV REQUIRED)

find_library(ONNX_LIB NAMES onnxruntime PATHS /usr/local/lib REQUIRED)
include_directories(/usr/local/include)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

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
    IReconstructionManager.h
    ColmapReconstructionManager.cpp
    ColmapReconstructionManager.h
    OnnxReconstructionManager.cpp
    OnnxReconstructionManager.h
    SettingsDialog.cpp
    SettingsDialog.h
    SettingsDialog.ui
    viewer.qml
    Theme.cpp
    Theme.h
    SystemChecks.cpp
    SystemChecks.h
    PointCloudGeometry.cpp
    PointCloudGeometry.h
    # --- ADD THESE LINES ---
    MeshGenerator.cpp
    MeshGenerator.h
    # -----------------------
    config.h.in
)

# --- 2. LINKOWANIE (Quick3D) ---
target_link_libraries(ImageTo3D PRIVATE
    Qt6::Widgets
    Qt6::Core5Compat
    ${OpenCV_LIBS}
    Qt6::Quick
    Qt6::Quick3D       # <--- Silnik 3D
    Qt6::QuickWidgets  # <--- Integracja z QWidgets
    ${ONNX_LIB}
    Open3D::Open3D
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/viewer.qml"
    "${CMAKE_CURRENT_BINARY_DIR}/viewer.qml"
    COPYONLY
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
