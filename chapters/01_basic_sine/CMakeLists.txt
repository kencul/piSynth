add_executable(01_basic_sine main.cpp)

target_link_libraries(01_basic_sine PRIVATE ALSA::ALSA)