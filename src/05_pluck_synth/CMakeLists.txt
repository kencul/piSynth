add_executable(05_pluck_synth
    main.cpp
    osc/osc.cpp
    midi/midi.cpp
    audio/audio.cpp
    voice/voice_manager.cpp
    adsr/adsr.cpp
)

target_include_directories(05_pluck_synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(05_pluck_synth PRIVATE
    ALSA::ALSA
    atomic
)