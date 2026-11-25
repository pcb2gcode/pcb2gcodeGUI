# End-to-end test: the GUI turns a configuration file into pcb2gcode arguments, pcb2gcode
# runs with them, and the GUI recognises and loads the preview images it wrote.
#
# Variables: GUI, PCB2GCODE, CONFIG, CASE, EXTRA (extra GUI arguments), WORK.

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

execute_process(
  COMMAND ${GUI} --print-command-line ${CONFIG} --pcb2gcode ${PCB2GCODE} ${EXTRA}
  OUTPUT_VARIABLE args_text
  ERROR_VARIABLE notes
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "pcb2gcodeGUI --print-command-line failed (${rc}):\n${notes}")
endif()
message(STATUS "GUI notes:\n${notes}")

string(REGEX REPLACE "\n$" "" args_text "${args_text}")
string(REPLACE ";" "\\;" args_text "${args_text}")
string(REPLACE "\n" ";" args "${args_text}")
message(STATUS "Arguments:\n${args_text}")

function(expect_arg arg)
  list(FIND args "${arg}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Expected argument ${arg} is missing")
  endif()
endfunction()

function(expect_no_arg prefix)
  foreach(a IN LISTS args)
    string(FIND "${a}" "${prefix}" pos)
    if(pos EQUAL 0)
      message(FATAL_ERROR "Unexpected argument ${a}")
    endif()
  endforeach()
endfunction()

expect_arg("--noconfigfile")
expect_no_arg("--vectorial")
expect_no_arg("--output-dir")

set(outputs "")
if(CASE MATCHES "^two_sided")
  expect_arg("--metric=true")
  expect_arg("--mill-diameters=0.2mm,0.5mm")
  expect_arg("--milling-overlap=25%")
  expect_arg("--spinup-time=1.5s")
  expect_arg("--mill-feed-direction=climb")
  expect_arg("--tsp-2opt=false")
  expect_arg("--pre-milling-gcode=M7")
  expect_arg("--pre-milling-gcode=(coolant on)")
  expect_arg("--zbridges=-1mm")
  expect_arg("--backtrack=50.8mm/s")
  expect_arg("--software=linuxcnc")
  set(outputs front.ngc back.ngc drill.ngc outline.ngc)
elseif(CASE STREQUAL "milldrill")
  expect_arg("--min-milldrill-hole-diameter=1.6mm")
  expect_arg("--min-milldrill-entry-diameter=140%")
  expect_arg("--max-milldrill-entry-diameter=180%")
  expect_arg("--milldrill-stepover=40%")
  expect_no_arg("--zwork")
  set(outputs drill.ngc milldrill.ngc)
elseif(CASE STREQUAL "legacy")
  expect_arg("--mill-diameters=0.2mm")
  expect_arg("--drill-side=front")
  expect_arg("--tolerance=0.01")
  expect_no_arg("--offset")
  expect_no_arg("--drill-front")
  set(outputs front.ngc back.ngc drill.ngc outline.ngc)
endif()

execute_process(
  COMMAND ${PCB2GCODE} ${args} --output-dir=${WORK}
  WORKING_DIRECTORY ${WORK}
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "pcb2gcode failed (${rc}):\n${out}\n${err}")
endif()

foreach(f IN LISTS outputs)
  if(NOT EXISTS ${WORK}/${f})
    message(FATAL_ERROR "pcb2gcode didn't write ${f}")
  endif()
endforeach()

execute_process(
  COMMAND ${GUI} --check-previews ${WORK}
  OUTPUT_VARIABLE previews
  ERROR_VARIABLE previews_err
  RESULT_VARIABLE rc)
message(STATUS "Previews:\n${previews}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "The GUI couldn't load the previews:\n${previews}\n${previews_err}")
endif()
