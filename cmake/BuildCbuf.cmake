
macro( build_cbuf )
  foreach(ARG ${ARGN})
    get_filename_component( name ${ARG} NAME_WE )
    get_filename_component( dir ${ARG} DIRECTORY )

    message( "producing ${name} in ${dir}" )

    add_custom_command(
      OUTPUT ${dir}/${name}.h
      COMMAND mkdir -p ${dir}
      COMMAND ${cbuf_BINARY_DIR}/cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${ARG} > ${dir}/${name}.h
      DEPENDS cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${ARG}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  endforeach()
endmacro()
