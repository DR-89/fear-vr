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

add_executable(
  test_locomotion_reprojection test_locomotion_reprojection.cpp
)
target_link_libraries(
  test_locomotion_reprojection PRIVATE fearvr-common fearvr-warnings
)
add_test(
  NAME locomotion_reprojection COMMAND test_locomotion_reprojection
)

add_executable(test_stereo_hud_math test_stereo_hud_math.cpp)
target_link_libraries(
  test_stereo_hud_math PRIVATE fearvr-common fearvr-warnings
)
add_test(NAME stereo_hud_math COMMAND test_stereo_hud_math)

add_executable(test_input_state test_input_state.cpp)
target_link_libraries(
  test_input_state PRIVATE fearvr-common fearvr-warnings
)
add_test(NAME input_state COMMAND test_input_state)

add_executable(test_controller_mapping test_controller_mapping.cpp)
target_link_libraries(
  test_controller_mapping PRIVATE fearvr-common fearvr-warnings
)
# Der Test prüft ausschließlich über assert; RelWithDebInfo definiert sonst
# NDEBUG und der Test liefe wirkungslos durch.
if(MSVC)
  target_compile_options(test_controller_mapping PRIVATE /UNDEBUG)
endif()
add_test(NAME controller_mapping COMMAND test_controller_mapping)

add_executable(test_two_handed_grip test_two_handed_grip.cpp)
target_link_libraries(
  test_two_handed_grip PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_two_handed_grip PRIVATE /UNDEBUG)
endif()
add_test(NAME two_handed_grip COMMAND test_two_handed_grip)

add_executable(test_weapon_weight test_weapon_weight.cpp)
target_link_libraries(
  test_weapon_weight PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_weapon_weight PRIVATE /UNDEBUG)
endif()
add_test(NAME weapon_weight COMMAND test_weapon_weight)

add_executable(test_weapon_recoil test_weapon_recoil.cpp)
target_link_libraries(
  test_weapon_recoil PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_weapon_recoil PRIVATE /UNDEBUG)
endif()
add_test(NAME weapon_recoil COMMAND test_weapon_recoil)

add_executable(test_weapon_collision test_weapon_collision.cpp)
target_link_libraries(
  test_weapon_collision PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_weapon_collision PRIVATE /UNDEBUG)
endif()
add_test(NAME weapon_collision COMMAND test_weapon_collision)

add_executable(test_melee_thrust test_melee_thrust.cpp)
target_link_libraries(
  test_melee_thrust PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_melee_thrust PRIVATE /UNDEBUG)
endif()
add_test(NAME melee_thrust COMMAND test_melee_thrust)

add_executable(test_melee_actions test_melee_actions.cpp)
target_link_libraries(
  test_melee_actions PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_melee_actions PRIVATE /UNDEBUG)
endif()
add_test(NAME melee_actions COMMAND test_melee_actions)

add_executable(test_climb_grip test_climb_grip.cpp)
target_link_libraries(
  test_climb_grip PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_climb_grip PRIVATE /UNDEBUG)
endif()
add_test(NAME climb_grip COMMAND test_climb_grip)

add_executable(test_lean_collision test_lean_collision.cpp)
target_link_libraries(
  test_lean_collision PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_lean_collision PRIVATE /UNDEBUG)
endif()
add_test(NAME lean_collision COMMAND test_lean_collision)

add_executable(test_lean_body test_lean_body.cpp)
target_link_libraries(
  test_lean_body PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_lean_body PRIVATE /UNDEBUG)
endif()
add_test(NAME lean_body COMMAND test_lean_body)

add_executable(test_hid_fix test_hid_fix.cpp)
target_link_libraries(
  test_hid_fix PRIVATE fearvr-common fearvr-hid-fix fearvr-warnings
)
if(MSVC)
  target_compile_options(test_hid_fix PRIVATE /UNDEBUG)
endif()
add_test(NAME hid_fix COMMAND test_hid_fix)

find_program(FEARVR_POWERSHELL_EXECUTABLE
  NAMES powershell.exe powershell
  REQUIRED
)
add_test(
  NAME release_launcher_session
  COMMAND
    ${FEARVR_POWERSHELL_EXECUTABLE}
    -NoProfile
    -ExecutionPolicy Bypass
    -File
    ${CMAKE_CURRENT_SOURCE_DIR}/test_release_launcher_session.ps1
)

add_executable(test_vr_menu_model test_vr_menu_model.cpp)
target_link_libraries(
  test_vr_menu_model PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_vr_menu_model PRIVATE /UNDEBUG)
endif()
add_test(NAME vr_menu_model COMMAND test_vr_menu_model)

add_executable(test_dev_menu_model test_dev_menu_model.cpp)
target_link_libraries(
  test_dev_menu_model PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_dev_menu_model PRIVATE /UNDEBUG)
endif()
add_test(NAME dev_menu_model COMMAND test_dev_menu_model)

add_executable(test_render_scale test_render_scale.cpp)
target_link_libraries(
  test_render_scale PRIVATE fearvr-common fearvr-warnings
)
if(MSVC)
  target_compile_options(test_render_scale PRIVATE /UNDEBUG)
endif()
add_test(NAME render_scale COMMAND test_render_scale)

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND TARGET fearvr-proxy)
  # Isolierter M2-Producer: Der echte System-Import bleibt d3d9.lib; erst
  # nach dem Link wird ausschließlich neben diese Test-EXE der Proxy kopiert.
  add_executable(d3d9_test_producer d3d9_test_producer.cpp)
  target_link_libraries(d3d9_test_producer PRIVATE
    fearvr-common
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
