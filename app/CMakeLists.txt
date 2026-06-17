# The StageView control-surface executable.
#
# Qt 6's qt_add_executable + qt_add_qml_module is the modern idiom: the QML files
# are compiled and embedded into the binary as a proper QML module, so there are
# no loose .qml files to ship and the engine resolves `import StageView` the same
# way it resolves built-in modules. This is the pattern a Qt shop expects to see.

qt_add_executable(stageview
    src/main.cpp
    src/StageModel.cpp
    src/StageModel.h
    src/MidiInput.cpp
    src/MidiInput.h
)

qt_add_qml_module(stageview
    URI StageView
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/StageCanvas.qml
        qml/SourceNode.qml
        qml/Meter.qml
        qml/ControlPanel.qml
)

target_link_libraries(stageview PRIVATE
    stageview_core
    rtmidi
    Qt6::Quick
    Qt6::Network
    Qt6::QuickControls2
)

# WIN32_EXECUTABLE keeps a console window from popping up behind the GUI on
# Windows; on a control surface you want the window to be the whole experience.
set_target_properties(stageview PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
