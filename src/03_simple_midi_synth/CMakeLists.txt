add_executable(03_simple_midi_synth
    main.cpp
    osc/osc.cpp
    midi/midi.cpp
    audio/audio.cpp
)

target_include_directories(03_simple_midi_synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(03_simple_midi_synth PRIVATE
    ALSA::ALSA
    atomic
)