# Wymagana minimalna wersja CMake
cmake_minimum_required(VERSION 3.16)

# Ustaw nazwę projektu, wersję (jak w README) i języki
project(ImageTo3D
        VERSION 0.1.0
        DESCRIPTION "2D Image to 3D Converter"
        LANGUAGES CXX)

# Standard dla C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatyczne ułatwienia dla Qt (automatyczne moc, rcc, ui)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Znajdź pakiet Qt6 i jego komponent 'Widgets' (do okienek)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Znajdź pakiet OpenCV (dodajemy od razu, przyda się później)
find_package(OpenCV REQUIRED)

# --- Plik konfiguracyjny dla wersji (jak w README) ---
include(GenerateExportHeader)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)
# ---------------------------------------------------

# Dodaj plik wykonywalny (nazwa 'ImageTo3D' z pliku 'main.cpp')
add_executable(ImageTo3D
    main.cpp
    mainframe.cpp
    mainframe.h
    mainframe.ui
)

# Dołącz folder 'build', aby program znalazł config.h
target_include_directories(ImageTo3D PUBLIC 
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# Podlinkuj biblioteki Qt i OpenCV do swojego programu
target_link_libraries(ImageTo3D PRIVATE
    Qt6::Widgets
    ${OpenCV_LIBS} # Linkuje wszystkie znalezione moduły OpenCV
)
