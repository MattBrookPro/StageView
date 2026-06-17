# stageview_core - the UI-free, testable C++ core.
#
# WHY a separate static library: the protocol codec (and, shortly, the UDP
# networking) have no dependency on QML or the windowing system. Pulling them into
# their own library means the test executable can link exactly this logic without
# dragging in Qt Quick, and it forces the "engine vs UI" seam to be real rather
# than aspirational. The GUI app links this library; so do the tests.
qt_add_library(stageview_core STATIC
    Osc.cpp
    Osc.h
    OscEndpoint.cpp
    OscEndpoint.h
    MidiMapper.cpp
    MidiMapper.h
)

# Qt6::Network for the UDP socket; still no Quick/QML/GUI dependency, so the whole
# protocol+transport layer stays headless and testable.
target_link_libraries(stageview_core PUBLIC Qt6::Core Qt6::Network)

# PUBLIC include dir so dependents can #include "Osc.h" without relative paths.
target_include_directories(stageview_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
