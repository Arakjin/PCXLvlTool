cmake_minimum_required(VERSION 3.16)

project(VWingLevTools LANGUAGES CXX)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_library(vwing_level_format
    src/lev_reader.cpp
    src/lev_writer.cpp
    src/pcx_writer.cpp
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

add_executable(VWingLevelEditor WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/level_canvas.cpp
    src/level_canvas.h
)
target_link_libraries(VWingLevelEditor PRIVATE
    vwing_level_format
    Qt6::Widgets
)
set_target_properties(VWingLevelEditor PROPERTIES AUTOMOC ON)

foreach(target vwing_level_format levdump levcompare pcxfixtures lev2pcx levroundtrip VWingLevelEditor)
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

include(CTest)
if(BUILD_TESTING)
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

    add_test(NAME gui_smoke_test COMMAND VWingLevelEditor --smoke-test)
    set_tests_properties(gui_smoke_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
