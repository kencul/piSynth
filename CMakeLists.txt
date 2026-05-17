cmake_minimum_required(VERSION 3.10)
project(PiSynth)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS    "Build the doctest test suite"   OFF)
option(BUILD_CHAPTERS "Build the chapter example bins" OFF)

# Find the ALSA library
find_package(ALSA REQUIRED)

# All executables land in build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(deps/pffft)

if(BUILD_CHAPTERS)
    add_subdirectory(chapters/01_basic_sine)
    add_subdirectory(chapters/02_midi_monitor)
    add_subdirectory(chapters/03_simple_midi_synth)
    add_subdirectory(chapters/04_poly_midi_synth)
    add_subdirectory(chapters/05_pluck_synth)
    add_subdirectory(chapters/06_pluck_synth_cc)
    add_subdirectory(chapters/07_pluck_synth_effects)
    add_subdirectory(chapters/08_web_ui)
    add_subdirectory(chapters/09_plug_and_play)
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()