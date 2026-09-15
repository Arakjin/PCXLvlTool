cmake_minimum_required(VERSION 3.16)

project(VWingLevTools LANGUAGES CXX)

add_executable(levdump
    tools/levdump/main.cpp
)

add_executable(levcompare
    tools/levcompare/main.cpp
)

foreach(target levdump levcompare)
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
