add_executable(04_poly_midi_synth
    main.cpp
    osc/osc.cpp
    midi/midi.cpp
    audio/audio.cpp
    voice/voice_manager.cpp
    adsr/adsr.cpp
)

target_include_directories(04_poly_midi_synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(04_poly_midi_synth PRIVATE
    ALSA::ALSA
    atomic
)