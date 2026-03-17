add_executable(06_pluck_synth_effects
    main.cpp
    osc/osc.cpp
    midi/midi.cpp
    audio/audio.cpp
    voice/voice_manager.cpp
    adsr/adsr.cpp
    voice/voice.cpp
    midi/synth_params.cpp
)

target_include_directories(06_pluck_synth_effects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(06_pluck_synth_effects PRIVATE
    ALSA::ALSA
    atomic
)

add_custom_command(TARGET 06_pluck_synth_effects POST_BUILD
    COMMAND sudo setcap cap_sys_nice+ep $<TARGET_FILE:05_pluck_synth>
    COMMENT "Granting realtime scheduling capability"
)