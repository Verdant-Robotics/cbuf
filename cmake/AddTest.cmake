macro(add_unit_test test_name)
  # sets UNIT_TEST custom definition to only the unit tests that follow format: add_test(<TEST_NAME> <TARGET_NAME> [<ARGS>])
  # for other tests calls CMake's add_test as is

  set(add_flag TRUE)

  # exclude tests with format: add_test(NAME <name> COMMAND <command>)
  if("${test_name}" STREQUAL "NAME")
    set(add_flag FALSE)
  endif()

  # exclude tests with format: add_test(<TEST_NAME> COMMAND <command>)
  foreach(arg ${ARGN})
    if("${arg}" STREQUAL "COMMAND")
      set(add_flag FALSE)
      break()
    endif()
  endforeach()

  # manually exclude bash based tests or the test that don't follow format: add_test(<TEST_NAME> <TARGET_NAME> <ARGS>)
  set(exclude_targets test_vlapse01 vd_test test_memtrack_preload)
  foreach(item ${exclude_targets})
    if("${test_name}" STREQUAL "${item}")
      set(add_flag FALSE)
      break()
    endif()
  endforeach()

  # add custom test target definition
  if(add_flag)
    target_compile_definitions(${test_name} PUBLIC UNIT_TEST)
  endif()

  # call CMake's macro 'add_test': https://cmake.org/cmake/help/latest/command/add_test.html
  add_test(${test_name} ${ARGN})
endmacro()
