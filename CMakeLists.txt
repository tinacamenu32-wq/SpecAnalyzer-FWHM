cmake_minimum_required(VERSION 3.22)
project(SpecAnalyzer-FWHM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    # Keep source and execution character sets consistent on Windows.
    add_compile_options(/utf-8)
endif()

# Qt6 install prefix (override with -DCMAKE_PREFIX_PATH on your system)
if(NOT CMAKE_PREFIX_PATH)
    if(APPLE)
        set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt" CACHE STRING "Qt6 install prefix")
    endif()
endif()

# 优先 Qt6，找不到则回退 Qt5（Win7 兼容）
find_package(Qt6 6.5 QUIET COMPONENTS Core Gui Widgets Charts)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Charts)
    set(QT_VERSION_MAJOR 5)
    message(STATUS "Using Qt5 (for Windows 7 compatibility)")
else()
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Using Qt6")
endif()

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
    src/PeakIntegration.cpp
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
    src/UiTheme.h
    src/CsvExport.h
)

add_executable(SpecAnalyzer-FWHM ${SOURCES} ${HEADERS})

target_include_directories(SpecAnalyzer-FWHM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpecAnalyzer-FWHM PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
)

# macOS bundle
set_target_properties(SpecAnalyzer-FWHM PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.geosurvey.spectrumviewer"
    MACOSX_BUNDLE_BUNDLE_NAME "SpecAnalyzer-FWHM"
)

# 临时测试程序
set(TEST_SOURCES ${SOURCES})
list(REMOVE_ITEM TEST_SOURCES "src/main.cpp")
add_executable(test_peaks test_peaks.cpp ${TEST_SOURCES})
target_include_directories(test_peaks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_peaks PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_csv_export tests/test_csv_export.cpp)
    target_include_directories(test_csv_export PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_csv_export PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME csv_export_encoding COMMAND test_csv_export)
    add_executable(test_peak_integration tests/test_peak_integration.cpp src/SpectralAnalyzer.cpp src/PeakIntegration.cpp)
    target_include_directories(test_peak_integration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_peak_integration PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME peak_integration COMMAND test_peak_integration)
    add_executable(test_ui_smoke tests/test_ui_smoke.cpp ${TEST_SOURCES} ${HEADERS})
    target_include_directories(test_ui_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_ui_smoke PRIVATE
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Charts)
    add_test(NAME ui_smoke COMMAND test_ui_smoke)
    set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
