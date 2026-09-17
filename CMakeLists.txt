cmake_minimum_required(VERSION 3.16)

project(PCXLvlTool VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_package(Qt6 6.2 REQUIRED COMPONENTS Svg Widgets)

add_library(vwing_level_format
    src/auts_io.cpp
    src/default_palette.cpp
    src/game_profile.cpp
    src/layer_model.cpp
    src/lev_reader.cpp
    src/lev_writer.cpp
    src/palette_io.cpp
    src/pcxl_project_io.cpp
    src/pcx_reader.cpp
    src/pcx_writer.cpp
    src/wings_lev_writer.cpp
)
target_include_directories(vwing_level_format PUBLIC src)

add_executable(levdump
    tools/levdump/main.cpp
)
target_link_libraries(levdump PRIVATE vwing_level_format)

add_executable(levcompare
    tools/levcompare/main.cpp
)

add_executable(pcxfixtures
    tools/pcxfixtures/main.cpp
)

add_executable(lev2pcx
    tools/lev2pcx/main.cpp
)
target_link_libraries(lev2pcx PRIVATE vwing_level_format)

add_executable(levroundtrip
    tools/levroundtrip/main.cpp
)
target_link_libraries(levroundtrip PRIVATE vwing_level_format)

add_executable(PCXLvlTool WIN32
    src/export_settings.cpp
    src/export_settings.h
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/level_canvas.cpp
    src/level_canvas.h
    src/palette_widget.cpp
    src/palette_widget.h
    resources/icons.qrc
)
if(WIN32)
    configure_file(
        resources/windows/pcxlvltool.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/pcxlvltool.rc
        @ONLY
    )
    target_sources(PCXLvlTool PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/pcxlvltool.rc
    )
endif()
target_link_libraries(PCXLvlTool PRIVATE
    vwing_level_format
    Qt6::Svg
    Qt6::Widgets
)
target_compile_definitions(PCXLvlTool PRIVATE
    PCXLVLTOOL_VERSION="${PROJECT_VERSION}"
)
set_target_properties(PCXLvlTool PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

foreach(target vwing_level_format levdump levcompare pcxfixtures lev2pcx levroundtrip PCXLvlTool)
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

configure_file(
    resources/linux/io.github.Arakjin.PCXLvlTool.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.Arakjin.PCXLvlTool.desktop
    @ONLY
)

install(TARGETS PCXLvlTool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.Arakjin.PCXLvlTool.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES pcxlvltool-icon-pack/pcxlvltool.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME pcxlvltool.svg
)
foreach(icon_size 16 24 32 48 64 128 256 512 1024)
    install(FILES pcxlvltool-icon-pack/pcxlvltool-${icon_size}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}x${icon_size}/apps
        RENAME pcxlvltool.png
    )
endforeach()
install(FILES
    LICENSE
    CHANGELOG.md
    README.md
    QT-LGPL-NOTICE.md
    THIRD_PARTY_NOTICES.md
    docs/install-fi.md
    docs/user-guide-fi.md
    docs/pxlp-format.md
    docs/auts-format.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/PCXLvlTool
)
install(FILES
    CONV.EXE
    CONVERT.TXT
    FILE_ID.DIZ
    DESTINATION ${CMAKE_INSTALL_DATADIR}/PCXLvlTool/VWingConverter
)
install(DIRECTORY
    third_party/kolourpaint
    third_party/tabler-icons
    third_party/qt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/PCXLvlTool/third_party
)
install(DIRECTORY third_party/qt/LICENSES/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/PCXLvlTool/licenses/Qt
)

include(CTest)
if(BUILD_TESTING)
    add_executable(auts_io_tests
        tests/auts_io_tests.cpp
    )
    target_link_libraries(auts_io_tests PRIVATE vwing_level_format)
    target_compile_features(auts_io_tests PRIVATE cxx_std_17)
    set_target_properties(auts_io_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME auts_io_tests COMMAND auts_io_tests)

    add_executable(pcxl_project_tests
        tests/pcxl_project_tests.cpp
    )
    target_link_libraries(pcxl_project_tests PRIVATE vwing_level_format)
    target_compile_features(pcxl_project_tests PRIVATE cxx_std_17)
    set_target_properties(pcxl_project_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME pcxl_project_tests COMMAND pcxl_project_tests)

    add_executable(wings_lev_writer_tests
        tests/wings_lev_writer_tests.cpp
    )
    target_link_libraries(wings_lev_writer_tests PRIVATE vwing_level_format)
    target_compile_features(wings_lev_writer_tests PRIVATE cxx_std_17)
    set_target_properties(wings_lev_writer_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME wings_lev_writer_tests COMMAND wings_lev_writer_tests)

    add_executable(game_profile_tests
        tests/game_profile_tests.cpp
    )
    target_link_libraries(game_profile_tests PRIVATE vwing_level_format)
    target_compile_features(game_profile_tests PRIVATE cxx_std_17)
    set_target_properties(game_profile_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME game_profile_tests COMMAND game_profile_tests)

    add_executable(export_settings_tests
        tests/export_settings_tests.cpp
        src/export_settings.cpp
    )
    target_link_libraries(export_settings_tests PRIVATE
        vwing_level_format
        Qt6::Core
    )
    target_compile_features(export_settings_tests PRIVATE cxx_std_17)
    set_target_properties(export_settings_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME export_settings_tests COMMAND export_settings_tests)

    add_executable(lev_tests
        tests/lev_tests.cpp
    )
    target_link_libraries(lev_tests PRIVATE vwing_level_format)
    target_compile_features(lev_tests PRIVATE cxx_std_17)
    set_target_properties(lev_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(lev_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(lev_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME lev_reader_tests COMMAND lev_tests)

    add_executable(pcx_tests
        tests/pcx_tests.cpp
    )
    target_link_libraries(pcx_tests PRIVATE vwing_level_format)
    target_compile_features(pcx_tests PRIVATE cxx_std_17)
    set_target_properties(pcx_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(pcx_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(pcx_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME pcx_writer_tests COMMAND pcx_tests)

    add_executable(lev_writer_tests
        tests/lev_writer_tests.cpp
    )
    target_link_libraries(lev_writer_tests PRIVATE vwing_level_format)
    target_compile_features(lev_writer_tests PRIVATE cxx_std_17)
    set_target_properties(lev_writer_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(lev_writer_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(lev_writer_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME lev_writer_tests COMMAND lev_writer_tests)

    add_executable(palette_tests
        tests/palette_tests.cpp
    )
    target_link_libraries(palette_tests PRIVATE vwing_level_format)
    target_compile_features(palette_tests PRIVATE cxx_std_17)
    set_target_properties(palette_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(palette_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(palette_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME palette_tests COMMAND palette_tests)

    add_test(NAME gui_smoke_test COMMAND PCXLvlTool --smoke-test)
    set_tests_properties(gui_smoke_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(level_canvas_tests
        tests/level_canvas_tests.cpp
        src/layer_model.cpp
        src/level_canvas.cpp
        src/level_canvas.h
        src/palette_widget.cpp
        src/palette_widget.h
        resources/icons.qrc
    )
    target_link_libraries(level_canvas_tests PRIVATE Qt6::Svg Qt6::Widgets)
    target_include_directories(level_canvas_tests PRIVATE src)
    target_compile_features(level_canvas_tests PRIVATE cxx_std_17)
    set_target_properties(level_canvas_tests PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(level_canvas_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(level_canvas_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME level_canvas_tests COMMAND level_canvas_tests)
    set_tests_properties(level_canvas_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
