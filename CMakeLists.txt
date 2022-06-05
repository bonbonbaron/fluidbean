cmake_minimum_required(VERSION 3.1)
set(CMAKE_BUILD_TYPE Debug)

project(fluidbean)

# This is why CMake knows where to find all the includes below.
list( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_admin )
# Then, using the above, we'll know where to find our modules.
include(GNUInstallDirs)
include (PkgConfigHelpers) # has unset_pkg_config()

list(APPEND HEADERS
    include/fluidbean.h
)

set(DRIVER_DIR ${CMAKE_SOURCE_DIR}/drivers)

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
    src/fluid_ringbuffer.c
    src/fluid_sys.c
    ${DRIVER_DIR}/fluid_adriver.c
    ${DRIVER_DIR}/fluid_alsa.c
)

# Basic C library checks
# Default install directory names, some provided by GNUInstallDirs
include (DefaultDirs)
include (CheckCCompilerFlag)
include (CheckSTDC)
include (CheckIncludeFile)
include (CheckSymbolExists)
include (CheckTypeSize)
check_include_file ( string.h HAVE_STRING_H )
check_include_file ( stdlib.h HAVE_STDLIB_H )
check_include_file ( stdio.h HAVE_STDIO_H )
check_include_file ( math.h HAVE_MATH_H )
check_include_file ( errno.h HAVE_ERRNO_H )
check_include_file ( stdarg.h HAVE_STDARG_H )
check_include_file ( unistd.h HAVE_UNISTD_H )
check_include_file ( sys/mman.h HAVE_SYS_MMAN_H )
check_include_file ( sys/types.h HAVE_SYS_TYPES_H )
check_include_file ( sys/time.h HAVE_SYS_TIME_H )
check_include_file ( sys/stat.h HAVE_SYS_STAT_H )
check_include_file ( fcntl.h HAVE_FCNTL_H )
check_include_file ( sys/socket.h HAVE_SYS_SOCKET_H )
check_include_file ( netinet/in.h HAVE_NETINET_IN_H )
check_include_file ( netinet/tcp.h HAVE_NETINET_TCP_H )
check_include_file ( arpa/inet.h HAVE_ARPA_INET_H )
check_include_file ( limits.h  HAVE_LIMITS_H )
check_include_file ( pthread.h HAVE_PTHREAD_H )
check_include_file ( signal.h HAVE_SIGNAL_H )
check_include_file ( getopt.h HAVE_GETOPT_H )
check_include_file ( stdint.h HAVE_STDINT_H )
check_type_size ( "long long" LONG_LONG )

# So just because I know I have the above, doesn't mean the *code* knows I have it.
# So I have to configure the config.h file, included by fluidsynth_priv.h, so they know.
configure_file(${CMAKE_SOURCE_DIR}/src/config.cmake ${CMAKE_SOURCE_DIR}/src/include/config.h)

# Make Fluidbean static library target with its build properties (c99 standard, output name, etc.).
add_library(${PROJECT_NAME} STATIC ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set(FLUIDBEAN_INSTALL_TARGETS ${FLUIDBEAN_INSTALL_TARGETS} ";"${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
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

# Link all the libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE ${M_LIBRARY} PRIVATE ${BOTOX_LIB})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${DRIVER_DIR}/include)

set_target_properties(${PROJECT_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_compile_definitions(${PROJECT_NAME} PUBLIC SF3_SUPPORT=${SF3_SUPPORT})

install(TARGETS ${FLUIDBEAN_INSTALL_TARGETS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${SCOPED_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fluidbean)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fluidbean.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)


######################################
## Config pkg-conifg for external use.
######################################
find_package ( PkgConfig REQUIRED )

# Use pkg-config to properly latch the glib libraries on.
pkg_check_modules ( GLIB REQUIRED glib-2.0>=2.6.5 gthread-2.0>=2.6.5 IMPORTED_TARGET )
list( APPEND PC_REQUIRES_PRIV "glib-2.0" "gthread-2.0")
message("fs bin dir is " ${CMAKE_BINARY_DIR})

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
  message("already defined")
  target_link_libraries ( ${PROJECT_NAME} PUBLIC PkgConfig::GLIB )
else()
  message("linking now")
  message("glib include dirs is " ${GLIB_INCLUDE_DIRS})
  target_include_directories ( ${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS} )
  message("glib libraries is " ${GLIB_LIBRARIES})
  target_link_libraries ( ${PROJECT_NAME} PUBLIC ${GLIB_LIBRARIES} )
endif()
