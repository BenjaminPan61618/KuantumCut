cmake_minimum_required(VERSION 3.22)
project(KuantumCut VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)
qt_standard_project_setup(REQUIRES 6.10)

add_library(kuantumcut_backend STATIC
    src/app/SettingsManager.cpp
    src/app/SettingsManager.h
    src/core/Draft.cpp
    src/core/Draft.h
    src/core/DraftRepository.cpp
    src/core/DraftRepository.h
    src/core/MediaProbe.cpp
    src/core/MediaProbe.h
    src/core/SystemProbe.cpp
    src/core/SystemProbe.h
    src/models/AssetModel.cpp
    src/models/AssetModel.h
    src/models/TimelineModel.cpp
    src/models/TimelineModel.h
    src/editor/EditorController.cpp
    src/editor/EditorController.h
    src/editor/ExportController.cpp
    src/editor/ExportController.h
    src/editor/PlaybackController.cpp
    src/editor/PlaybackController.h
    src/editor/UndoCommands.cpp
    src/editor/UndoCommands.h
)
target_include_directories(kuantumcut_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(kuantumcut_backend PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia
)

qt_add_executable(kuantumcut src/main.cpp)
target_link_libraries(kuantumcut PRIVATE kuantumcut_backend)

set(kuantumcut_qml_files
    src/qml/Main.qml
    src/qml/ProbeInfo.qml
    src/qml/DraftManagerPage.qml
    src/qml/SettingsPage.qml
    src/qml/EditorPage.qml
    src/qml/ExportDialog.qml
    src/qml/AssetPanel.qml
    src/qml/PreviewPane.qml
    src/qml/Timeline.qml
    src/qml/Inspector.qml
)
foreach(qml_file IN LISTS kuantumcut_qml_files)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "qml/${qml_name}")
endforeach()
qt_add_qml_module(kuantumcut
    URI KuantumCut
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${kuantumcut_qml_files}
)

find_program(QSB_EXECUTABLE NAMES qsb HINTS /usr/lib64/qt6/bin /usr/lib/qt6/bin REQUIRED)
set(color_filter_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/color_filter.frag")
set(color_filter_qsb "${CMAKE_CURRENT_BINARY_DIR}/shaders/color_filter.frag.qsb")
add_custom_command(
    OUTPUT "${color_filter_qsb}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND "${QSB_EXECUTABLE}" --qt6 -o "${color_filter_qsb}" "${color_filter_shader}"
    DEPENDS "${color_filter_shader}"
    VERBATIM
)
add_custom_target(kuantumcut_filter_shader DEPENDS "${color_filter_qsb}")
add_dependencies(kuantumcut kuantumcut_filter_shader)
qt_add_resources(kuantumcut "kuantumcut_shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    FILES "${color_filter_qsb}"
)

find_package(Qt6 6.10 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
    qt_add_translations(kuantumcut
        TS_FILES src/app/kuantumcut_zh_CN.ts src/app/kuantumcut_en_US.ts
        RESOURCE_PREFIX /i18n
    )
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Test)
    qt_add_executable(test_draft tests/test_draft.cpp)
    target_link_libraries(test_draft PRIVATE kuantumcut_backend Qt6::Test)
    add_test(NAME test_draft COMMAND test_draft)

    qt_add_executable(test_timeline_undo tests/test_timeline_undo.cpp)
    target_link_libraries(test_timeline_undo PRIVATE kuantumcut_backend Qt6::Test)
    add_test(NAME test_timeline_undo COMMAND test_timeline_undo)

    qt_add_executable(test_export tests/test_export.cpp)
    target_link_libraries(test_export PRIVATE kuantumcut_backend Qt6::Test)
    add_test(NAME test_export COMMAND test_export)
endif()
