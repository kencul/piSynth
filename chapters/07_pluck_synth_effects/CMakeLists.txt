add_executable(07_pluck_synth_effects
    main.cpp
    osc/osc.cpp
    midi/midi.cpp
    audio/audio.cpp
    voice/voice_manager.cpp
    adsr/adsr.cpp
    voice/voice.cpp
    common/synth_params.cpp
    effects/master_bus.cpp
    effects/svf.cpp
    effects/chorus.cpp
    effects/primitives/delay_line.cpp
    effects/primitives/lfo.cpp
    effects/primitives/one_pole.cpp
    common/smoothed_value.cpp
    effects/primitives/allpass.cpp
    effects/primitives/comb.cpp
    effects/freeverb.cpp
    effects/ping_pong.cpp
)

target_include_directories(07_pluck_synth_effects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(07_pluck_synth_effects PRIVATE
    ALSA::ALSA
    atomic
)

add_custom_command(TARGET 07_pluck_synth_effects POST_BUILD
    COMMAND sudo setcap cap_sys_nice+ep $<TARGET_FILE:07_pluck_synth_effects>
    COMMENT "Granting realtime scheduling capability"
)