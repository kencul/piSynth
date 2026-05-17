include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(synth_tests
    test_svf.cpp
    test_pluck.cpp
    test_ring_buffer.cpp
    test_adsr.cpp
    test_smoothed_value.cpp
    test_lfo.cpp
    ${CMAKE_SOURCE_DIR}/src/adsr/adsr.cpp
    ${CMAKE_SOURCE_DIR}/src/effects/svf.cpp
    ${CMAKE_SOURCE_DIR}/src/effects/primitives/one_pole.cpp
    ${CMAKE_SOURCE_DIR}/src/effects/primitives/lfo.cpp
    ${CMAKE_SOURCE_DIR}/src/common/smoothed_value.cpp
    ${CMAKE_SOURCE_DIR}/src/osc/osc.cpp
)

target_include_directories(synth_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(synth_tests PRIVATE
    Catch2::Catch2WithMain
)

target_precompile_headers(synth_tests PRIVATE <catch2/catch_all.hpp>)

include(CTest)
include(Catch)
catch_discover_tests(synth_tests)

add_executable(fft_debug
    test_fft_stability.cpp
    ${CMAKE_SOURCE_DIR}/src/fft/fft_processor.cpp
)

target_include_directories(fft_debug PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/pffft/include/pffft
)

target_link_libraries(fft_debug PRIVATE
    PFFFT::PFFFT
)
