cmake_minimum_required(VERSION 3.10)
project(PiSynth)

set(CMAKE_CXX_STANDARD 17)

# Find the ALSA library
find_package(ALSA REQUIRED)

add_executable(synth main.cpp)

# Link ALSA
target_link_libraries(synth PRIVATE ALSA::ALSA)