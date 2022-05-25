cmake_minimum_required(VERSION 3.1)

#set(CMAKE_BUILD_TYPE Debug)
project(fluidbean)

include(GNUInstallDirs)

list(APPEND HEADERS
    include/fluidbean.h
)

list(APPEND SCOPED_HEADERS
    include/fluidsynth/types.h
    include/fluidsynth/settings.h
    include/fluidsynth/synth.h
    include/fluidsynth/sfont.h
    include/fluidsynth/log.h
    include/fluidsynth/misc.h
    include/fluidsynth/mod.h
    include/fluidsynth/gen.h
    include/fluidsynth/voice.h
    include/fluidsynth/version.h
)


list(APPEND SOURCES
    src/fluid_init.c
    src/fluid_chan.c
    src/fluid_chorus.c
    src/fluid_conv.c
    src/fluid_defsfont.c
    src/fluid_dsp_float.c
    src/fluid_gen.c
    src/fluid_hash.c
    src/fluid_list.c
    src/fluid_mod.c
    src/fluid_rev.c
    src/fluid_settings.c
    src/fluid_synth.c
    src/fluid_sys.c
    src/fluid_tuning.c
    src/fluid_voice.c
)

#target_include_directories(
#  ${PROJECT_NAME} PUBLIC src/include/
#)

# Dependencies:

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

#if (STB_VORBIS)
#    list(APPEND SOURCES
#        stb/stb_vorbis.c)
#
#    include_directories(${CMAKE_SOURCE_DIR}/stb)
#endif()

option(FLUIDLITE_BUILD_STATIC "Build static library" TRUE)
if(FLUIDLITE_BUILD_STATIC)
    add_library(${PROJECT_NAME}-static STATIC ${SOURCES})
    set_target_properties(${PROJECT_NAME}-static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set(FLUIDLITE_LIB_TARGET ${PROJECT_NAME}-static)
    set(FLUIDLITE_INSTALL_TARGETS ${FLUIDLITE_INSTALL_TARGETS} ";fluidbean-static")
    set_target_properties(${PROJECT_NAME}-static PROPERTIES C_STANDARD 99)
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME}-static PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

option(FLUIDLITE_BUILD_SHARED "Build shared library" TRUE)
if(FLUIDLITE_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})

    # find the math lib
    find_library(M_LIBRARY m)
    message(STATUS "Math library: ${M_LIBRARY}")
    if(NOT M_LIBRARY)
        set(M_LIBRARY "")
    endif()

    target_link_libraries(${PROJECT_NAME}
        ${LIBVORBIS_LIBRARIES}
        ${LIBVORBISFILE_LIBRARIES}
        ${LIBOGG_LIBRARIES}
        ${M_LIBRARY}
    )
    set(FLUIDLITE_LIB_TARGET ${PROJECT_NAME})
    set(FLUIDLITE_INSTALL_TARGETS ${FLUIDLITE_INSTALL_TARGETS} ";fluidbean")
    set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if((NOT FLUIDLITE_BUILD_SHARED) AND (NOT FLUIDLITE_BUILD_STATIC))
    message(FATAL_ERROR "Neither dynamic nor static library build is selected.")
endif()

if(FLUIDLITE_BUILD_SHARED AND FLUIDLITE_BUILD_STATIC)
    set_target_properties(${PROJECT_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${PROJECT_NAME}-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
endif()

if (ENABLE_SF3)
    if (STB_VORBIS)
        set(SF3_SUPPORT "SF3_STB_VORBIS")
    else()
        set(SF3_SUPPORT "SF3_XIPH_VORBIS")
    endif()

    if(FLUIDLITE_BUILD_SHARED)
        target_compile_definitions(${PROJECT_NAME} PUBLIC SF3_SUPPORT=${SF3_SUPPORT})
    endif()
    if(FLUIDLITE_BUILD_STATIC)
        target_compile_definitions(${PROJECT_NAME}-static PUBLIC SF3_SUPPORT=${SF3_SUPPORT})
    endif()
endif()

configure_file(fluidbean.pc.in ${CMAKE_BINARY_DIR}/fluidbean.pc @ONLY)

install(TARGETS ${FLUIDLITE_INSTALL_TARGETS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${SCOPED_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fluidsynth)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fluidbean.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
