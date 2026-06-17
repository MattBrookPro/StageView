# stageaudio - the real-time C++ audio engine.
#
# A console app (no GUI): it loads the stems, plays and mixes them through RtAudio
# with per-channel zero-latency DSP, and speaks the same OSC/UDP the surface does.
# Reuses stageview_core for the OSC codec; Qt6::Core/Network give JSON parsing, the
# UDP socket and an event loop for the control plane - none of which touch the
# real-time audio callback.

qt_add_executable(stageaudio
    src/main.cpp
    src/WavFile.cpp
    src/WavFile.h
    src/Dsp.cpp
    src/Dsp.h
    src/AudioEngine.cpp
    src/AudioEngine.h
    src/OscServer.cpp
    src/OscServer.h
)

target_link_libraries(stageaudio PRIVATE
    stageview_core
    rtaudio
    Qt6::Core
    Qt6::Network
)

# Console app: keep stdout and Ctrl+C (no WIN32 GUI subsystem).
set_target_properties(stageaudio PROPERTIES WIN32_EXECUTABLE OFF)
