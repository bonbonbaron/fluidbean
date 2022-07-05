cmake_minimum_required(VERSION 3.1)
set(CMAKE_BUILD_TYPE Debug)

project(fluidbean)

# This is why CMake knows where to find all the includes below.
list( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_admin )
# Then, using the above, we'll know where to find our modules.
include(GNUInstallDirs)
include (PkgConfigHelpers) # has unset_pkg_config()

set(DRIVER_DIR ${CMAKE_SOURCE_DIR}/drivers)
set(AUDIO_DRIVER_DIR ${DRIVER_DIR}/audio)
set(MIDI_DRIVER_DIR ${DRIVER_DIR}/midi)

# Includes
list(APPEND HEADERS
    ${CMAKE_SOURCE_DIR}/src/include/chan.h
    ${CMAKE_SOURCE_DIR}/src/include/chorus.h
    ${CMAKE_SOURCE_DIR}/src/include/config.h
    ${CMAKE_SOURCE_DIR}/src/include/conv.h
    ${CMAKE_SOURCE_DIR}/src/include/conv_tables.h
    ${CMAKE_SOURCE_DIR}/src/include/fluidbean.h
    ${CMAKE_SOURCE_DIR}/src/include/gen.h
    ${CMAKE_SOURCE_DIR}/src/include/midi.h
    ${CMAKE_SOURCE_DIR}/src/include/mod.h
    ${CMAKE_SOURCE_DIR}/src/include/phase.h
    ${CMAKE_SOURCE_DIR}/src/include/rev.h
    ${CMAKE_SOURCE_DIR}/src/include/soundfont.h
    ${CMAKE_SOURCE_DIR}/src/include/synth.h
    ${CMAKE_SOURCE_DIR}/src/include/sys.h
    ${CMAKE_SOURCE_DIR}/src/include/tuning.h
    ${CMAKE_SOURCE_DIR}/src/include/voice.h
)

# Source code
list(APPEND SOURCES
    ${CMAKE_SOURCE_DIR}/src/chan.c
    ${CMAKE_SOURCE_DIR}/src/chorus.c
    ${CMAKE_SOURCE_DIR}/src/conv.c
    ${CMAKE_SOURCE_DIR}/src/defsfont.c
    ${CMAKE_SOURCE_DIR}/src/dsp_float.c
    ${CMAKE_SOURCE_DIR}/src/gen.c
    ${CMAKE_SOURCE_DIR}/src/mod.c
    ${CMAKE_SOURCE_DIR}/src/rev.c
    ${CMAKE_SOURCE_DIR}/src/synth.c
    ${CMAKE_SOURCE_DIR}/src/sys.c
    ${CMAKE_SOURCE_DIR}/src/tuning.c
    ${CMAKE_SOURCE_DIR}/src/voice.c
    # AUdio driver files
    ${AUDIO_DRIVER_DIR}/fluid_adriver.c
    ${AUDIO_DRIVER_DIR}/fluid_alsa.c
    # Midi driver files
    ${MIDI_DRIVER_DIR}/fluid_midi.c
    ${MIDI_DRIVER_DIR}/fluid_midi_router.c
    ${MIDI_DRIVER_DIR}/fluid_seq.c
    ${MIDI_DRIVER_DIR}/fluid_seqbind.c
    ${MIDI_DRIVER_DIR}/fluid_seqbind_notes.cpp
    ${MIDI_DRIVER_DIR}/fluid_seq_queue.cpp
)

# Basic C library checks
# Default install directory names, some provided by GNUInstallDirs
include (DefaultDirs)
include (CheckCCompilerFlag)
include (CheckSTDC)
include (CheckIncludeFile)
include (CheckSymbolExists)
include (CheckTypeSize)

#check_include_file ( string.h HAVE_STRING_H )
#check_include_file ( stdlib.h HAVE_STDLIB_H )
#check_include_file ( stdio.h HAVE_STDIO_H )
#check_include_file ( math.h HAVE_MATH_H )
#check_include_file ( errno.h HAVE_ERRNO_H )
#check_include_file ( stdarg.h HAVE_STDARG_H )
#check_include_file ( unistd.h HAVE_UNISTD_H )
#check_include_file ( sys/mman.h HAVE_SYS_MMAN_H )
#check_include_file ( sys/types.h HAVE_SYS_TYPES_H )
#check_include_file ( sys/time.h HAVE_SYS_TIME_H )
#check_include_file ( sys/stat.h HAVE_SYS_STAT_H )
#check_include_file ( fcntl.h HAVE_FCNTL_H )
#check_include_file ( sys/socket.h HAVE_SYS_SOCKET_H )
#check_include_file ( netinet/in.h HAVE_NETINET_IN_H )
#check_include_file ( netinet/tcp.h HAVE_NETINET_TCP_H )
#check_include_file ( arpa/inet.h HAVE_ARPA_INET_H )
#check_include_file ( limits.h  HAVE_LIMITS_H )
#check_include_file ( pthread.h HAVE_PTHREAD_H )
#check_include_file ( signal.h HAVE_SIGNAL_H )
#check_include_file ( getopt.h HAVE_GETOPT_H )
#check_include_file ( stdint.h HAVE_STDINT_H )
#check_type_size ( "long long" LONG_LONG )

# So just because I know I have the above, doesn't mean the *code* knows I have it.
# So I have to configure the config.h file, included by fluidsynth_priv.h, so they know.
configure_file(${CMAKE_SOURCE_DIR}/src/config.cmake ${CMAKE_SOURCE_DIR}/src/include/config.h)

# Make Fluidbean static library target with its build properties (c99 standard, output name, etc.).
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${MIDI_DRIVER_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${AUDIO_DRIVER_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${DRIVER_DIR}/include)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set(FLUIDBEAN_INSTALL_TARGETS ${FLUIDBEAN_INSTALL_TARGETS} ";"${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

#set(HEADER_FILES ${YOUR_DIRECTORY}/file1.h ${YOUR_DIRECTORY}/file2.h)

#add_library(mylib libsrc.cpp ${HEADER_FILES})
#target_include_directories(mylib PRIVATE ${YOUR_DIRECTORY})
#add_executable(myexec execfile.cpp ${HEADER_FILES})
#target_include_directories(myexec PRIVATE ${YOUR_DIRECTORY})

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
if(NOT M_LIBRARY)
    set(M_LIBRARY "")
endif()

# Link all the libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE ${M_LIBRARY} PRIVATE ${BOTOX_LIB})

set_target_properties(${PROJECT_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)

install(TARGETS ${FLUIDBEAN_INSTALL_TARGETS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fluidbean.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


######################################
## Config pkg-conifg for external use.
######################################
find_package ( PkgConfig REQUIRED )

# Use pkg-config to properly latch the glib libraries on.
pkg_check_modules ( GLIB REQUIRED glib-2.0>=2.6.5 gthread-2.0>=2.6.5 IMPORTED_TARGET )
list( APPEND PC_REQUIRES_PRIV "glib-2.0" "gthread-2.0")

# generate_pkgconfig_spec    template                 outfile                   target )
#generate_pkgconfig_spec(fluidbean.pc.in ${CMAKE_BINARY_DIR}/fluidbean.pc libfluidbean-OBJ)
generate_pkgconfig_spec(fluidbean.pc.in ${CMAKE_BINARY_DIR}/fluidbean.pc fluidbean)
install ( FILES ${CMAKE_BINARY_DIR}/fluidbean.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )

if ( GLIB_glib-2.0_VERSION AND GLIB_glib-2.0_VERSION VERSION_LESS "2.26.0" )
    message ( WARNING "Your version of glib is very old. This may cause problems with fluidbean's sample cache on Windows. Consider updating to glib 2.26 or newer!" )
endif ( GLIB_glib-2.0_VERSION AND GLIB_glib-2.0_VERSION VERSION_LESS "2.26.0" )

configure_file(fluidbean.pc.in ${CMAKE_BINARY_DIR}/fluidbean.pc @ONLY)

##############################
## This is where to link GLIB. 
##############################
# All below strings are empty. We need to import these targets first, either in pkg-config
# form or as individual libraries. I'm leaning toward the former because of its inclusion
# of dependencies.
if ( TARGET PkgConfig::GLIB )
  target_link_libraries ( ${PROJECT_NAME} PUBLIC PkgConfig::GLIB )
else()
  target_include_directories ( ${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS} )
  target_link_libraries ( ${PROJECT_NAME} PUBLIC ${GLIB_LIBRARIES} )
endif()
