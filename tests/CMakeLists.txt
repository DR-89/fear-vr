# Automatisierte Tests (ANWEISUNG.md §14). Baubar ohne Headset.
add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE fearvr-common fearvr-warnings)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_xr_session_state test_xr_session_state.cpp)
target_link_libraries(test_xr_session_state PRIVATE fearvr-common fearvr-warnings)
add_test(NAME xr_session_state COMMAND test_xr_session_state)

add_executable(test_stereo_math test_stereo_math.cpp)
target_link_libraries(test_stereo_math PRIVATE fearvr-common fearvr-warnings)
add_test(NAME stereo_math COMMAND test_stereo_math)

add_executable(test_head_tracking_math test_head_tracking_math.cpp)
target_link_libraries(
  test_head_tracking_math PRIVATE fearvr-common fearvr-warnings
)
add_test(NAME head_tracking_math COMMAND test_head_tracking_math)

add_executable(test_stereo_hud_math test_stereo_hud_math.cpp)
target_link_libraries(
  test_stereo_hud_math PRIVATE fearvr-common fearvr-warnings
)
add_test(NAME stereo_hud_math COMMAND test_stereo_hud_math)

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND TARGET fearvr-proxy)
  # Isolierter M2-Producer: Der echte System-Import bleibt d3d9.lib; erst
  # nach dem Link wird ausschließlich neben diese Test-EXE der Proxy kopiert.
  add_executable(d3d9_test_producer d3d9_test_producer.cpp)
  target_link_libraries(d3d9_test_producer PRIVATE
    fearvr-warnings
    d3d9
  )
  add_dependencies(d3d9_test_producer fearvr-proxy)
  add_custom_command(TARGET d3d9_test_producer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:fearvr-proxy>"
      "$<TARGET_FILE_DIR:d3d9_test_producer>/d3d9.dll"
    COMMENT "Staging M2 d3d9.dll beside the isolated test producer"
  )

  # Separates Ausgabeverzeichnis ohne lokalen d3d9.dll-Proxy: simuliert den
  # archcfg-Loader, der die eindeutig benannte Bridge lädt und die EXE-IAT
  # patcht, während Windows' System-d3d9 bereits importiert ist.
  add_executable(iat_hook_test iat_hook_test.cpp)
  target_link_libraries(iat_hook_test PRIVATE fearvr-warnings d3d9)
  add_dependencies(iat_hook_test fearvr-proxy)
  set_target_properties(iat_hook_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/iat-test"
  )
endif()

# Weitere Testziele folgen pro Meilenstein:
#   - test_math       (Quaternion, Achsenabbildung, Pose, FOV->Projektion)
#   - test_ringbuffer (Generationen, Timeoutpfade)
