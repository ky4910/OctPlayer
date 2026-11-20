qt_add_executable(octplayer main.cpp qml.qrc)
target_link_libraries(octplayer PRIVATE player_core Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml)
