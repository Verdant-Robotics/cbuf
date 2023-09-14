string(ASCII 27 Esc)
set(ColourReset "${Esc}[m")
set(ColourBold "${Esc}[1m")
set(Red "${Esc}[31m")
set(Green "${Esc}[32m")
set(Yellow "${Esc}[33m")
set(Blue "${Esc}[34m")
set(Magenta "${Esc}[35m")
set(Cyan "${Esc}[36m")
set(White "${Esc}[37m")
set(BoldRed "${Esc}[1;31m")
set(BoldGreen "${Esc}[1;32m")
set(BoldYellow "${Esc}[1;33m")
set(BoldBlue "${Esc}[1;34m")
set(BoldMagenta "${Esc}[1;35m")
set(BoldCyan "${Esc}[1;36m")
set(BoldWhite "${Esc}[1;37m")

macro(fetch_dependency target url tag)
  include(FetchContent)
  if(NOT TARGET ${target})
    find_file(${target}_project ${target}.project
              PATHS ${PROJECT_SOURCE_DIR}/../${target} ${PROJECT_SOURCE_DIR}/../../${target}
                    ${PROJECT_SOURCE_DIR}/../../../${target} ${PROJECT_SOURCE_DIR}/../../../../${target})
    if(NOT ${target}_project)
      FetchContent_Declare(${target} GIT_REPOSITORY ${url} GIT_TAG ${tag} GIT_SHALLOW FALSE)
      FetchContent_GetProperties(${target})
      if(NOT ${target}_POPULATED)
        execute_process(COMMAND ${CMAKE_COMMAND} -E echo_append "-- Downloading ${target} ")
        FetchContent_Populate(${target})
        message("into ${${target}_SOURCE_DIR}... done.")
        add_subdirectory(${${target}_SOURCE_DIR} ${${target}_BINARY_DIR})
      endif()
      message(
        "${BoldGreen}-- ${CMAKE_CURRENT_LIST_FILE} Using ${target} downloaded from git, saved at: ${${target}_SOURCE_DIR}${ColourReset}"
      )
    else()
      get_filename_component(dir ${${target}_project} DIRECTORY)
      set(${target}_SOURCE_DIR ${dir})
      message(
        "${BoldGreen}-- ${CMAKE_CURRENT_LIST_FILE} Using ${target} on the local disk at: ${${target}_SOURCE_DIR}${ColourReset}"
      )
      add_subdirectory(${${target}_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${target})
    endif()
  endif()
endmacro()
