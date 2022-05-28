cmake_minimum_required(VERSION 3.1)
set(CMAKE_BUILD_TYPE Debug)

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
    #src/stb_vorbis.c
)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(FLUIDBEAN_STATIC_LIB ${PROJECT_NAME})
option(FLUIDLITE_BUILD_STATIC "Build static library" TRUE)
add_library(${FLUIDBEAN_STATIC_LIB} STATIC ${SOURCES})
set_target_properties(${FLUIDBEAN_STATIC_LIB} PROPERTIES OUTPUT_NAME ${FLUIDBEAN_STATIC_LIB})
set(FLUIDLITE_LIB_TARGET ${FLUIDBEAN_STATIC_LIB})
set(FLUIDLITE_INSTALL_TARGETS ${FLUIDLITE_INSTALL_TARGETS} ";"${FLUIDBEAN_STATIC_LIB})
set_target_properties(${FLUIDBEAN_STATIC_LIB} PROPERTIES C_STANDARD 99)
if(WIN32)
  target_compile_definitions(${FLUIDBEAN_STATIC_LIB} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

list(APPEND BOTOX_SRC
  libbotox/src/bt.c
  libbotox/src/data.c
  libbotox/src/x.c
)

set(BOTOX_LIB botox)
add_library(${BOTOX_LIB} ${BOTOX_SRC})
target_include_directories(${BOTOX_LIB} PUBLIC libbotox/src/include)

# find the math lib
find_library(M_LIBRARY m)
message(STATUS "Math library: ${M_LIBRARY}")
if(NOT M_LIBRARY)
    set(M_LIBRARY "")
endif()

message("math library is set to " ${M_LIBRARY})

# Link all the libraries.
target_link_libraries(${FLUIDBEAN_STATIC_LIB} ${M_LIBRARY} ${BOTOX_LIB})

option(FLUIDLITE_BUILD_SHARED "Build shared library" OFF)
if(FLUIDLITE_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})


    set(FLUIDLITE_LIB_TARGET ${PROJECT_NAME})
    set(FLUIDLITE_INSTALL_TARGETS ${FLUIDLITE_INSTALL_TARGETS} ";"${PROJECT_NAME})
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if((NOT FLUIDLITE_BUILD_SHARED) AND (NOT FLUIDLITE_BUILD_STATIC))
    message(FATAL_ERROR "Neither dynamic nor static library build is selected.")
endif()

if(FLUIDLITE_BUILD_SHARED AND FLUIDLITE_BUILD_STATIC)
    set_target_properties(${PROJECT_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${FLUIDBEAN_STATIC_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
endif()

if (ENABLE_SF3)
    if(FLUIDLITE_BUILD_SHARED)
        target_compile_definitions(${PROJECT_NAME} PUBLIC SF3_SUPPORT=${SF3_SUPPORT})
    endif()
    if(FLUIDLITE_BUILD_STATIC)
      target_compile_definitions(${FLUIDBEAN_STATIC_LIB} PUBLIC SF3_SUPPORT=${SF3_SUPPORT})
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
