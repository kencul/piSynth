cmake_minimum_required(VERSION 3.10)
project(PiSynth)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the ALSA library
find_package(ALSA REQUIRED)

# All executables land in build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src/01_basic_sine)
add_subdirectory(src/02_midi_monitor)
add_subdirectory(src/03_simple_midi_synth)
add_subdirectory(src/04_poly_midi_synth)
add_subdirectory(src/05_pluck_synth)
add_subdirectory(src/06_pluck_synth_cc)
add_subdirectory(src/07_pluck_synth_effects)
add_subdirectory(src/08_web_ui)