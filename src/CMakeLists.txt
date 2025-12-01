cmake_minimum_required(VERSION 3.16)

# --- 1. ODCZYT WERSJI Z PLIKU ---
# CMake otworzy plik version.txt i wczyta pierwszą linię do zmiennej APP_VERSION
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" APP_VERSION)
string(STRIP "${APP_VERSION}" APP_VERSION) # Usuwa spacje/entery

# --- 2. KONFIGURACJA PROJEKTU ---
project(ImageTo3D VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core5Compat)
find_package(OpenCV REQUIRED)

# Znajdź ONNX Runtime
find_library(ONNX_LIB NAMES onnxruntime PATHS /usr/local/lib REQUIRED)
include_directories(/usr/local/include)

# --- 3. GENEROWANIE CONFIG.H ---
# Zamienia @PROJECT_VERSION@ w pliku .in na "0.7.3" i zapisuje jako .h
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
# Dodajemy folder build do ścieżek, żeby kompilator widział wygenerowany config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Symlink do modeli
file(CREATE_LINK
    "${CMAKE_CURRENT_SOURCE_DIR}/models"
    "${CMAKE_CURRENT_BINARY_DIR}/models"
    SYMBOLIC
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- 4. PLIKI ŹRÓDŁOWE ---
add_executable(ImageTo3D
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    reconstructionmanager.cpp
    reconstructionmanager.h
    ai_reconstructionmanager.cpp
    ai_reconstructionmanager.h
    
    # --- WAŻNE: Dodajemy pliki konfiguracyjne do targetu ---
    # Dzięki temu CMake "wie", że projekt zależy od nich.
    # Jeśli zmienisz numer w version.txt, CMake automatycznie przebuduje config.h!
    version.txt
    config.h.in
)

target_link_libraries(ImageTo3D PRIVATE
    Qt6::Widgets
    Qt6::Core5Compat
    ${OpenCV_LIBS}
    ${ONNX_LIB}
)
