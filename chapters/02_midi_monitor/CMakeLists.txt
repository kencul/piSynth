add_executable(02_midi_monitor main.cpp)
target_link_libraries(02_midi_monitor PRIVATE ALSA::ALSA pthread)