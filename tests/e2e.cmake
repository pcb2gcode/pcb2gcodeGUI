# End-to-end test: the GUI turns a configuration file into pcb2gcode arguments, pcb2gcode
# runs with them, and the GUI recognises and loads the preview images it wrote.
#
# Variables: GUI, PCB2GCODE, CONFIG, CASE, WORK, EXTRA (more GUI arguments, space separated),
# SOURCE (text the GUI must print about where its option list came from).

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
separate_arguments(extra UNIX_COMMAND "${EXTRA}")

execute_process(
  COMMAND ${GUI} --print-command-line ${CONFIG} --pcb2gcode ${PCB2GCODE} ${extra}
  OUTPUT_VARIABLE args_text
  ERROR_VARIABLE notes
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "pcb2gcodeGUI --print-command-line failed (${rc}):\n${notes}")
endif()
message(STATUS "GUI notes:\n${notes}")
if(SOURCE)
  string(FIND "${notes}" "${SOURCE}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Expected the GUI to report \"${SOURCE}\"")
  endif()
endif()

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

# With a pcb2gcode that has --manifest, the previews must come from the manifest.
execute_process(COMMAND ${PCB2GCODE} --help OUTPUT_VARIABLE help ERROR_QUIET)
string(FIND "${help}" "--manifest" has_manifest)
set(manifest_args "")
set(check_args "")
if(NOT has_manifest EQUAL -1)
  set(manifest_args --manifest=manifest.json)
  set(check_args --manifest ${WORK}/manifest.json)
endif()

execute_process(
  COMMAND ${PCB2GCODE} ${args} --output-dir=${WORK} ${manifest_args}
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
  COMMAND ${GUI} --check-previews ${WORK} ${check_args}
  OUTPUT_VARIABLE previews
  ERROR_VARIABLE previews_err
  RESULT_VARIABLE rc)
message(STATUS "Previews:\n${previews}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "The GUI couldn't load the previews:\n${previews}\n${previews_err}")
endif()
if(manifest_args)
  string(FIND "${previews}" "found in the manifest" from_manifest)
  if(from_manifest EQUAL -1)
    message(FATAL_ERROR "The previews didn't come from the manifest")
  endif()
endif()
