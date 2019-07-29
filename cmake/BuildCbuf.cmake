
#  This macro supports the following arguments:
#
#  OUTPUT_DIR : where to write the cbuf header
#  INCLUDE_DIR : List of paths where to search for includes
#  CBUF_FILE: Main cbuf file to compile
#
macro( build_cbuf )
    cmake_parse_arguments( CBUF "" "OUTPUT_DIR;CBUF_FILE" "INCLUDE_DIR" ${ARGN} )

    if (NOT CBUF_CBUF_FILE)
        message(FATAL_ERROR "A cbuf file is a must in the macro")
    endif()

    get_filename_component( name ${CBUF_CBUF_FILE} NAME_WE )

    get_filename_component( srcdir ${CBUF_CBUF_FILE} DIRECTORY )
    if( NOT CBUF_OUTPUT_DIR )
        get_filename_component( outdir ${CBUF_CBUF_FILE} DIRECTORY )
    else()
        set(outdir ${CBUF_OUTPUT_DIR})
    endif()

    list(APPEND incs "-I${CMAKE_CURRENT_SOURCE_DIR}/${srcdir}")
    foreach( incdir ${CBUF_INCLUDE_DIR})
        list(APPEND incs "-I${incdir}")
    endforeach()

    message( "producing ${name} in ${outdir}" )

  add_custom_command(
    OUTPUT ${outdir}/${name}.h
    COMMAND mkdir -p ${outdir}
    COMMAND ${cbuf_BINARY_DIR}/cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${CBUF_CBUF_FILE} ${incs} > ${outdir}/${name}.h
    DEPENDS cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${CBUF_CBUF_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
endmacro()
