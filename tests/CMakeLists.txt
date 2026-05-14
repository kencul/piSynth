include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(synth_tests
    test_main.cpp
    test_svf.cpp
    ${CMAKE_SOURCE_DIR}/src/effects/svf.cpp
)

target_include_directories(synth_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(synth_tests PRIVATE
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(synth_tests)
