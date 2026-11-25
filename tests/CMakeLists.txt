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

if(pcb2gcode_for_tests)
  message(STATUS "pcb2gcode for end-to-end tests: ${pcb2gcode_for_tests}")
  foreach(
    case IN
    ITEMS "two_sided:example_board/two_sided.millproject:"
          "two_sided_bundled:example_board/two_sided.millproject:--bundled-options"
          "milldrill:milldrill/milldrill.millproject:"
          "legacy:legacy/old_gui.millproject:")
    string(REPLACE ":" ";" parts "${case}")
    list(GET parts 0 case_name)
    list(GET parts 1 config)
    list(LENGTH parts n)
    set(extra "")
    if(n GREATER 2)
      list(GET parts 2 extra)
    endif()
    add_test(
      NAME e2e_${case_name}
      COMMAND
        ${CMAKE_COMMAND} -DGUI=$<TARGET_FILE:pcb2gcodeGUI> -DPCB2GCODE=${pcb2gcode_for_tests}
        -DCONFIG=${TEST_DATA}/${config} -DCASE=${case_name} -DEXTRA=${extra}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e/${case_name} -P ${CMAKE_CURRENT_SOURCE_DIR}/e2e.cmake)
    set_tests_properties(e2e_${case_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endforeach()
else()
  message(STATUS "pcb2gcode not found: end-to-end tests are disabled (set PCB2GCODE_EXECUTABLE)")
endif()
