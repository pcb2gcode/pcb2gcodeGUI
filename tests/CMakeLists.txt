find_package(Qt6 REQUIRED COMPONENTS Test)

set(TEST_DATA ${CMAKE_CURRENT_SOURCE_DIR}/data)

if(PCB2GCODE_EXECUTABLE)
  set(pcb2gcode_for_tests ${PCB2GCODE_EXECUTABLE})
else()
  find_program(PCB2GCODE_ON_PATH pcb2gcode)
  set(pcb2gcode_for_tests "")
  if(PCB2GCODE_ON_PATH)
    set(pcb2gcode_for_tests ${PCB2GCODE_ON_PATH})
  endif()
endif()

function(pcb2gcodegui_test name)
  add_executable(${name} ${name}.cpp ${PCB2GCODEGUI_RESOURCES})
  target_link_libraries(${name} PRIVATE pcb2gcodegui_core Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(
    ${name} PROPERTIES ENVIRONMENT
                       "QT_QPA_PLATFORM=offscreen;PCB2GCODEGUI_TEST_DATA=${TEST_DATA};PCB2GCODE=${pcb2gcode_for_tests}")
endfunction()

pcb2gcodegui_test(tst_core)
pcb2gcodegui_test(tst_form)
# Compares the bundled option list with the pcb2gcode found above; skipped without one.
pcb2gcodegui_test(tst_sync)

if(NOT pcb2gcode_for_tests)
  message(STATUS "pcb2gcode not found: end-to-end tests are disabled (set PCB2GCODE_EXECUTABLE)")
  return()
endif()
message(STATUS "pcb2gcode for end-to-end tests: ${pcb2gcode_for_tests}")

# e2e_case(NAME CONFIG [PCB2GCODE path] [SOURCE text] [ARGS gui-arguments...])
# SOURCE is text the GUI must report about where its option list came from.
# GUI_CHECKS: the GUI must check the options itself even if pcb2gcode has --check-options.
function(e2e_case name config)
  cmake_parse_arguments(E "GUI_CHECKS" "PCB2GCODE;SOURCE" "ARGS" ${ARGN})
  if(NOT E_PCB2GCODE)
    set(E_PCB2GCODE ${pcb2gcode_for_tests})
  endif()
  string(REPLACE ";" " " extra "${E_ARGS}")
  add_test(
    NAME e2e_${name}
    COMMAND
      ${CMAKE_COMMAND} -DGUI=$<TARGET_FILE:pcb2gcodeGUI> -DPCB2GCODE=${E_PCB2GCODE}
      -DREAL_PCB2GCODE=${pcb2gcode_for_tests} -DCONFIG=${TEST_DATA}/${config} -DCASE=${name}
      "-DEXTRA=${extra}" "-DSOURCE=${E_SOURCE}" -DGUI_CHECKS=${E_GUI_CHECKS}
      -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e/${name} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/e2e.cmake)
  set_tests_properties(e2e_${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

e2e_case(two_sided example_board/two_sided.millproject)
e2e_case(two_sided_bundled example_board/two_sided.millproject ARGS --bundled-options SOURCE
         "Built-in option list")
e2e_case(milldrill milldrill/milldrill.millproject)
e2e_case(legacy legacy/old_gui.millproject)
e2e_case(invalid invalid/zsafe_below_zwork.millproject)

# A pcb2gcode that describes its own options, until pcb2gcode releases --dump-options.
if(UNIX)
  set(SNAPSHOT ${PROJECT_SOURCE_DIR}/resources/pcb2gcode-options.json)
  set(PCB2GCODE ${pcb2gcode_for_tests})
  configure_file(pcb2gcode-with-dump.sh.in ${CMAKE_CURRENT_BINARY_DIR}/generated/pcb2gcode-with-dump @ONLY)
  file(
    COPY ${CMAKE_CURRENT_BINARY_DIR}/generated/pcb2gcode-with-dump
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  # Its option list is the bundled one, which has no --check-options.
  e2e_case(two_sided_dump example_board/two_sided.millproject PCB2GCODE
           ${CMAKE_CURRENT_BINARY_DIR}/pcb2gcode-with-dump SOURCE "Options from pcb2gcode" GUI_CHECKS)
endif()
