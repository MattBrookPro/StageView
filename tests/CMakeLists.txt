# C++ tests for the core library. Registered with CTest so CI runs them with a
# single `ctest` and fails the build on a non-zero exit.

# Unit tests for the OSC codec - framework-free, just an exit code.
qt_add_executable(osc_test osc_test.cpp)
target_link_libraries(osc_test PRIVATE stageview_core Qt6::Core)
add_test(NAME osc_test COMMAND osc_test)

# Unit tests for the MIDI->action mapper - also framework-free.
qt_add_executable(midi_test midi_test.cpp)
target_link_libraries(midi_test PRIVATE stageview_core Qt6::Core)
add_test(NAME midi_test COMMAND midi_test)

# Integration test for the threaded endpoint - uses Qt Test + QSignalSpy to drive
# a real worker thread and a stand-in engine socket.
qt_add_executable(endpoint_test endpoint_test.cpp)
target_link_libraries(endpoint_test PRIVATE stageview_core Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME endpoint_test COMMAND endpoint_test)
