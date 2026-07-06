cmake_minimum_required(VERSION 3.22)
project(SpecAnalyzer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Point to Homebrew Qt 6 on Apple Silicon
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt" CACHE STRING "Qt6 install prefix")

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Charts
)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CsvParser.cpp
    src/SpectrumData.cpp
    src/FileListPanel.cpp
    src/SpectrumChartView.cpp
    src/MetadataPanel.cpp
    src/FileScanner.cpp
    src/SpectralAnalyzer.cpp
)

set(HEADERS
    src/MainWindow.h
    src/CsvParser.h
    src/SpectrumData.h
    src/FileListPanel.h
    src/SpectrumChartView.h
    src/MetadataPanel.h
    src/FileScanner.h
    src/SpectralAnalyzer.h
)

add_executable(SpecAnalyzer ${SOURCES} ${HEADERS})

target_include_directories(SpecAnalyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpecAnalyzer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
)

# macOS bundle
set_target_properties(SpecAnalyzer PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.geosurvey.spectrumviewer"
    MACOSX_BUNDLE_BUNDLE_NAME "SpecAnalyzer"
)
