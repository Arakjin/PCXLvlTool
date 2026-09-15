cmake_minimum_required(VERSION 3.16)

project(VWingLevTools LANGUAGES CXX)

add_executable(levdump
    tools/levdump/main.cpp
)

target_compile_features(levdump PRIVATE cxx_std_17)
set_target_properties(levdump PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(levdump PRIVATE /W4 /permissive-)
else()
    target_compile_options(levdump PRIVATE -Wall -Wextra -Wpedantic)
endif()
