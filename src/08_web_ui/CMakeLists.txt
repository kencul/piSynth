file(GLOB USOCKETS_SOURCES
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/*.c
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/eventing/*.c
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/crypto/*.c
)

# exclude SSL backends
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*openssl.*")
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*wolfssl.*")
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*boringssl.*")

add_library(uSockets STATIC ${USOCKETS_SOURCES})

target_include_directories(uSockets PUBLIC
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src
)
target_compile_definitions(uSockets PUBLIC 
    LIBUS_NO_SSL=1 
    UWS_NO_ZLIB=1
)

add_executable(08_web_ui
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
    web/web_server.cpp
)

target_compile_definitions(08_web_ui PRIVATE
    HTML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
)

target_include_directories(08_web_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/src
)

target_link_libraries(08_web_ui PRIVATE
    ALSA::ALSA
    atomic
    uSockets
)

add_custom_command(TARGET 08_web_ui POST_BUILD
    COMMAND sudo setcap cap_sys_nice+ep $<TARGET_FILE:08_web_ui>
    COMMENT "Granting realtime scheduling capability"
)