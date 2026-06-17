# Top-level build for StageView.
#
# WHY a CMake super-project (this file + app/CMakeLists.txt) rather than one flat
# list: it keeps the build description close to the code it builds, and lets a CI
# job or a teammate configure the whole thing with a single `cmake -S . -B build`.
# For cross-platform C++/Qt builds, CMake is the standard.

cmake_minimum_required(VERSION 3.21)

project(StageView
    VERSION 0.1.0
    DESCRIPTION "Spatial control surface over a networked audio engine"
    LANGUAGES CXX)

# Modern, explicit C++. Qt 6 itself requires C++17; we hold to it deliberately
# rather than chasing newer standards we don't need.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect every executable in one predictable place (build/bin) no matter which
# subdirectory defines the target. Makes running, windeployqt and CI scripts simple.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Quick      -> the QML/Quick scene graph (the UI).
# Network    -> QUdpSocket, our transport to the engine (OSC over UDP).
# QuickControls2 -> ready-made controls so the UI effort goes into the stage, not buttons.
find_package(Qt6 REQUIRED COMPONENTS Quick Network QuickControls2 Test)

# qt_standard_project_setup() turns on AUTOMOC/RCC and the QML tooling wiring so
# we don't hand-maintain it. REQUIRES 6.5 documents the floor we test against.
qt_standard_project_setup(REQUIRES 6.5)

# Adopt the modern QML-module resource layout (QTP0004) so .qml files living in a
# qml/ subdirectory are handled without a hand-written qmldir. Guarded so older
# Qt that predates this policy still configures cleanly.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# --- RtMidi: cross-platform MIDI I/O, fetched and pinned ---
# Pulled at configure time and built static, single-API per platform (WinMM on
# Windows, ALSA on Linux, CoreMIDI on macOS). JACK is turned off so a Linux build
# needs only libasound, and RtMidi's own tests are skipped. FetchContent keeps the
# dependency explicit and version-pinned without vendoring a source blob.
include(FetchContent)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RTMIDI_API_JACK OFF CACHE BOOL "" FORCE)
# RtMidi and RtAudio both default their uninstall target to "uninstall"; rename so
# the two FetchContent deps don't collide on the target name.
set(RTMIDI_TARGETNAME_UNINSTALL "uninstall_rtmidi" CACHE STRING "" FORCE)
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG 6.0.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(rtmidi)

# --- RtAudio: cross-platform real-time audio I/O for the C++ audio engine ---
# Same family as RtMidi. Static, tests off; on Windows it uses WASAPI/DirectSound
# (no extra deps). Drives the real-time stem-playback + DSP engine.
set(RTAUDIO_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RTAUDIO_API_JACK OFF CACHE BOOL "" FORCE)
set(RTAUDIO_TARGETNAME_UNINSTALL "uninstall_rtaudio" CACHE STRING "" FORCE)
# ASIO (Windows): exposes ALL of an interface's hardware outputs, which WASAPI's
# per-endpoint view doesn't - needed for per-output monitor mixes. RtAudio bundles
# the ASIO SDK + a MinGW thiscall shim, so it builds on our toolchain.
if(WIN32)
    set(RTAUDIO_API_ASIO ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(rtaudio
    GIT_REPOSITORY https://github.com/thestk/rtaudio.git
    GIT_TAG 6.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(rtaudio)

# core must be configured before the app and tests that link it.
add_subdirectory(core)

# Tests are part of the default build so they can never silently rot; `ctest`
# runs them. (BUILD_TESTING is ON by default; a packager can turn it off.)
enable_testing()
add_subdirectory(tests)

add_subdirectory(app)

# The real-time C++ audio engine (plays the stems with per-channel DSP, speaks OSC).
add_subdirectory(audio-engine)
