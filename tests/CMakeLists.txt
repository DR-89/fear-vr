# Automatisierte Tests (ANWEISUNG.md §14). Baubar ohne Headset.
add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE fearvr-common fearvr-warnings)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_xr_session_state test_xr_session_state.cpp)
target_link_libraries(test_xr_session_state PRIVATE fearvr-common fearvr-warnings)
add_test(NAME xr_session_state COMMAND test_xr_session_state)

# Weitere Testziele folgen pro Meilenstein:
#   - test_math       (Quaternion, Achsenabbildung, Pose, FOV->Projektion)
#   - test_ringbuffer (Generationen, Timeoutpfade)
