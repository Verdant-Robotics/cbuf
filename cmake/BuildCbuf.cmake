#  This macro supports the following arguments:
#
#  NAME: Name for the cbuf library to include elsewhere,
#        otherwise it is the name of the fist message
#  OUTPUT_DIR : where to write the cbuf header
#  INCLUDE_DIRS : List of paths where to search for includes
#  MSG_FILES: Main cbuf file to compile
#

cmake_policy(SET CMP0116 NEW)

macro( build_cbuf )
    cmake_parse_arguments( CBUF "" "OUTPUT_DIR;NAME" "INCLUDE_DIRS;MSG_FILES" ${ARGN} )

    if (NOT CBUF_MSG_FILES)
        message(FATAL_ERROR "At least one cbuf message file is a must in the macro")
    endif()

    # get the first cbuf
    list(GET CBUF_MSG_FILES 0 FIRST_MSG_FILE )

    if( NOT CBUF_NAME )
      get_filename_component( cbuf_lib_name ${FIRST_MSG_FILE} NAME_WE )
    else()
      set( cbuf_lib_name ${CBUF_NAME} )
    endif()

    if( NOT CBUF_OUTPUT_DIR )
        get_filename_component( outdir ${FIRST_MSG_FILE} DIRECTORY )
    else()
        set(outdir ${CBUF_OUTPUT_DIR})
    endif()

    get_filename_component( first_srcdir ${FIRST_MSG_FILE} DIRECTORY )

    set( cbuf_header_gen "" )

    foreach( msg_file ${CBUF_MSG_FILES})

        get_filename_component( name ${msg_file} NAME_WE )

        get_filename_component( srcdir ${msg_file} DIRECTORY )

        set(incs "")

        # Set the include path from the current folder
        list(APPEND incs "-I${CMAKE_CURRENT_SOURCE_DIR}/${srcdir}" )
        # set the include paths from included folders directly
        foreach( incdir ${CBUF_INCLUDE_DIRS} )
            list(APPEND incs "-I${incdir}")
        endforeach()

        add_custom_command(
          OUTPUT ${outdir}/${name}.h
          COMMAND mkdir -p ${outdir}
          COMMAND cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${msg_file} ${incs} -d ${outdir}/${name}.d -o ${outdir}/${name}.h
          DEPENDS cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${msg_file}
          DEPFILE ${outdir}/${name}.d
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
        list(APPEND cbuf_header_gen "${CMAKE_CURRENT_BINARY_DIR}/${outdir}/${name}.h")

        if(${ENABLE_HJSON})
          add_custom_command(
            OUTPUT ${outdir}/${name}_json.h
            COMMAND mkdir -p ${outdir}
            COMMAND cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${msg_file} ${incs} -j ${outdir}/${name}_json.h
            DEPENDS cbuf ${CMAKE_CURRENT_SOURCE_DIR}/${msg_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          )
          list(APPEND cbuf_header_gen "${CMAKE_CURRENT_BINARY_DIR}/${outdir}/${name}_json.h")
        endif()
    endforeach()

    add_custom_target( generate_${cbuf_lib_name}_headers DEPENDS ${cbuf_header_gen} )

    # Create an interface library to simply including the cbuf headers
    add_library(${cbuf_lib_name} INTERFACE)
    target_link_libraries(${cbuf_lib_name} INTERFACE cbuf_lib)
    add_dependencies(${cbuf_lib_name} generate_${cbuf_lib_name}_headers )
    target_include_directories(${cbuf_lib_name} INTERFACE ${CMAKE_CURRENT_BINARY_DIR} )
endmacro()
