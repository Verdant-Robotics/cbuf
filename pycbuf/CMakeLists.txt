#TODO: Do nothing if python dev is not installed
find_package(Python3 COMPONENTS Interpreter Development)

if(Python3_FOUND)
    add_library(pycbuf SHARED pycbuf.cpp CBufParserPy.cpp cbuf_reader_python.cpp)
    target_include_directories(pycbuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pycbuf PUBLIC Python3::Module uloglib cbuf_internal)
    target_compile_options(pycbuf PRIVATE -Wno-unused-function)
    set_target_properties(pycbuf PROPERTIES PREFIX "")

    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
    install(TARGETS pycbuf DESTINATION lib)
    install(TARGETS pycbuf DESTINATION "/usr/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
    install(TARGETS pycbuf DESTINATION "/usr/local/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")

    set(PYCBUF_VERSION_MAJOR "1")
    set(PYCBUF_VERSION_MINOR "0")
    set(PYCBUF_VERSION_PATCH "0")

    set(CPACK_PACKAGE_NAME "pycbuf-${Python3_VERSION_MAJOR}-${Python3_VERSION_MINOR}")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_CONTACT "lawrence@verdantrobotics.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lawrence Ibarria <${CPACK_PACKAGE_CONTACT}>")
    set(CPACK_PACKAGE_VENDOR "Verdant Robotics")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Python interface to cbuf files"
        CACHE STRING "Pycbuf provides a comprehensive set of python interface to cbuf files, allowing to browser and decode any and all types on a cbuf" )
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
    set(CPACK_PACKAGE_VERSION_MAJOR ${PYCBUF_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PYCBUF_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PYCBUF_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE.txt")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)
endif()
