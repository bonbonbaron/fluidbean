# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/bonbonbaron/hack/fluidbean

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/bonbonbaron/hack/fluidbean/example/out

# Include any dependencies generated for this target.
include CMakeFiles/fluidbean.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/fluidbean.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/fluidbean.dir/flags.make

CMakeFiles/fluidbean.dir/src/fluid_init.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_init.c.o: ../../src/fluid_init.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/fluidbean.dir/src/fluid_init.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_init.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_init.c

CMakeFiles/fluidbean.dir/src/fluid_init.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_init.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_init.c > CMakeFiles/fluidbean.dir/src/fluid_init.c.i

CMakeFiles/fluidbean.dir/src/fluid_init.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_init.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_init.c -o CMakeFiles/fluidbean.dir/src/fluid_init.c.s

CMakeFiles/fluidbean.dir/src/fluid_chan.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_chan.c.o: ../../src/fluid_chan.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/fluidbean.dir/src/fluid_chan.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_chan.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_chan.c

CMakeFiles/fluidbean.dir/src/fluid_chan.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_chan.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_chan.c > CMakeFiles/fluidbean.dir/src/fluid_chan.c.i

CMakeFiles/fluidbean.dir/src/fluid_chan.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_chan.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_chan.c -o CMakeFiles/fluidbean.dir/src/fluid_chan.c.s

CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o: ../../src/fluid_chorus.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_chorus.c

CMakeFiles/fluidbean.dir/src/fluid_chorus.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_chorus.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_chorus.c > CMakeFiles/fluidbean.dir/src/fluid_chorus.c.i

CMakeFiles/fluidbean.dir/src/fluid_chorus.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_chorus.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_chorus.c -o CMakeFiles/fluidbean.dir/src/fluid_chorus.c.s

CMakeFiles/fluidbean.dir/src/fluid_conv.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_conv.c.o: ../../src/fluid_conv.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object CMakeFiles/fluidbean.dir/src/fluid_conv.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_conv.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_conv.c

CMakeFiles/fluidbean.dir/src/fluid_conv.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_conv.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_conv.c > CMakeFiles/fluidbean.dir/src/fluid_conv.c.i

CMakeFiles/fluidbean.dir/src/fluid_conv.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_conv.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_conv.c -o CMakeFiles/fluidbean.dir/src/fluid_conv.c.s

CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o: ../../src/fluid_defsfont.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building C object CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_defsfont.c

CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_defsfont.c > CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.i

CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_defsfont.c -o CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.s

CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o: ../../src/fluid_dsp_float.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building C object CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_dsp_float.c

CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_dsp_float.c > CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.i

CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_dsp_float.c -o CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.s

CMakeFiles/fluidbean.dir/src/fluid_gen.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_gen.c.o: ../../src/fluid_gen.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building C object CMakeFiles/fluidbean.dir/src/fluid_gen.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_gen.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_gen.c

CMakeFiles/fluidbean.dir/src/fluid_gen.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_gen.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_gen.c > CMakeFiles/fluidbean.dir/src/fluid_gen.c.i

CMakeFiles/fluidbean.dir/src/fluid_gen.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_gen.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_gen.c -o CMakeFiles/fluidbean.dir/src/fluid_gen.c.s

CMakeFiles/fluidbean.dir/src/fluid_hash.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_hash.c.o: ../../src/fluid_hash.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building C object CMakeFiles/fluidbean.dir/src/fluid_hash.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_hash.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_hash.c

CMakeFiles/fluidbean.dir/src/fluid_hash.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_hash.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_hash.c > CMakeFiles/fluidbean.dir/src/fluid_hash.c.i

CMakeFiles/fluidbean.dir/src/fluid_hash.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_hash.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_hash.c -o CMakeFiles/fluidbean.dir/src/fluid_hash.c.s

CMakeFiles/fluidbean.dir/src/fluid_list.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_list.c.o: ../../src/fluid_list.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building C object CMakeFiles/fluidbean.dir/src/fluid_list.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_list.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_list.c

CMakeFiles/fluidbean.dir/src/fluid_list.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_list.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_list.c > CMakeFiles/fluidbean.dir/src/fluid_list.c.i

CMakeFiles/fluidbean.dir/src/fluid_list.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_list.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_list.c -o CMakeFiles/fluidbean.dir/src/fluid_list.c.s

CMakeFiles/fluidbean.dir/src/fluid_mod.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_mod.c.o: ../../src/fluid_mod.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Building C object CMakeFiles/fluidbean.dir/src/fluid_mod.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_mod.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_mod.c

CMakeFiles/fluidbean.dir/src/fluid_mod.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_mod.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_mod.c > CMakeFiles/fluidbean.dir/src/fluid_mod.c.i

CMakeFiles/fluidbean.dir/src/fluid_mod.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_mod.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_mod.c -o CMakeFiles/fluidbean.dir/src/fluid_mod.c.s

CMakeFiles/fluidbean.dir/src/fluid_rev.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_rev.c.o: ../../src/fluid_rev.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_11) "Building C object CMakeFiles/fluidbean.dir/src/fluid_rev.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_rev.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_rev.c

CMakeFiles/fluidbean.dir/src/fluid_rev.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_rev.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_rev.c > CMakeFiles/fluidbean.dir/src/fluid_rev.c.i

CMakeFiles/fluidbean.dir/src/fluid_rev.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_rev.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_rev.c -o CMakeFiles/fluidbean.dir/src/fluid_rev.c.s

CMakeFiles/fluidbean.dir/src/fluid_settings.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_settings.c.o: ../../src/fluid_settings.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_12) "Building C object CMakeFiles/fluidbean.dir/src/fluid_settings.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_settings.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_settings.c

CMakeFiles/fluidbean.dir/src/fluid_settings.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_settings.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_settings.c > CMakeFiles/fluidbean.dir/src/fluid_settings.c.i

CMakeFiles/fluidbean.dir/src/fluid_settings.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_settings.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_settings.c -o CMakeFiles/fluidbean.dir/src/fluid_settings.c.s

CMakeFiles/fluidbean.dir/src/fluid_synth.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_synth.c.o: ../../src/fluid_synth.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_13) "Building C object CMakeFiles/fluidbean.dir/src/fluid_synth.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_synth.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_synth.c

CMakeFiles/fluidbean.dir/src/fluid_synth.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_synth.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_synth.c > CMakeFiles/fluidbean.dir/src/fluid_synth.c.i

CMakeFiles/fluidbean.dir/src/fluid_synth.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_synth.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_synth.c -o CMakeFiles/fluidbean.dir/src/fluid_synth.c.s

CMakeFiles/fluidbean.dir/src/fluid_sys.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_sys.c.o: ../../src/fluid_sys.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_14) "Building C object CMakeFiles/fluidbean.dir/src/fluid_sys.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_sys.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_sys.c

CMakeFiles/fluidbean.dir/src/fluid_sys.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_sys.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_sys.c > CMakeFiles/fluidbean.dir/src/fluid_sys.c.i

CMakeFiles/fluidbean.dir/src/fluid_sys.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_sys.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_sys.c -o CMakeFiles/fluidbean.dir/src/fluid_sys.c.s

CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o: ../../src/fluid_tuning.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_15) "Building C object CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_tuning.c

CMakeFiles/fluidbean.dir/src/fluid_tuning.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_tuning.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_tuning.c > CMakeFiles/fluidbean.dir/src/fluid_tuning.c.i

CMakeFiles/fluidbean.dir/src/fluid_tuning.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_tuning.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_tuning.c -o CMakeFiles/fluidbean.dir/src/fluid_tuning.c.s

CMakeFiles/fluidbean.dir/src/fluid_voice.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_voice.c.o: ../../src/fluid_voice.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_16) "Building C object CMakeFiles/fluidbean.dir/src/fluid_voice.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_voice.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_voice.c

CMakeFiles/fluidbean.dir/src/fluid_voice.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_voice.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_voice.c > CMakeFiles/fluidbean.dir/src/fluid_voice.c.i

CMakeFiles/fluidbean.dir/src/fluid_voice.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_voice.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_voice.c -o CMakeFiles/fluidbean.dir/src/fluid_voice.c.s

CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o: ../../src/fluid_ringbuffer.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_17) "Building C object CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o -c /home/bonbonbaron/hack/fluidbean/src/fluid_ringbuffer.c

CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/src/fluid_ringbuffer.c > CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.i

CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/src/fluid_ringbuffer.c -o CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.s

CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o: ../../drivers/fluid_adriver.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_18) "Building C object CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o -c /home/bonbonbaron/hack/fluidbean/drivers/fluid_adriver.c

CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/drivers/fluid_adriver.c > CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.i

CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/drivers/fluid_adriver.c -o CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.s

CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o: CMakeFiles/fluidbean.dir/flags.make
CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o: ../../drivers/fluid_alsa.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_19) "Building C object CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o -c /home/bonbonbaron/hack/fluidbean/drivers/fluid_alsa.c

CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bonbonbaron/hack/fluidbean/drivers/fluid_alsa.c > CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.i

CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bonbonbaron/hack/fluidbean/drivers/fluid_alsa.c -o CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.s

# Object files for target fluidbean
fluidbean_OBJECTS = \
"CMakeFiles/fluidbean.dir/src/fluid_init.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_chan.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_conv.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_gen.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_hash.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_list.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_mod.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_rev.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_settings.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_synth.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_sys.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_voice.c.o" \
"CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o" \
"CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o" \
"CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o"

# External object files for target fluidbean
fluidbean_EXTERNAL_OBJECTS =

libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_init.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_chan.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_chorus.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_conv.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_defsfont.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_dsp_float.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_gen.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_hash.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_list.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_mod.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_rev.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_settings.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_synth.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_sys.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_tuning.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_voice.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/src/fluid_ringbuffer.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/drivers/fluid_adriver.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/drivers/fluid_alsa.c.o
libfluidbean.a: CMakeFiles/fluidbean.dir/build.make
libfluidbean.a: CMakeFiles/fluidbean.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles --progress-num=$(CMAKE_PROGRESS_20) "Linking C static library libfluidbean.a"
	$(CMAKE_COMMAND) -P CMakeFiles/fluidbean.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/fluidbean.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/fluidbean.dir/build: libfluidbean.a

.PHONY : CMakeFiles/fluidbean.dir/build

CMakeFiles/fluidbean.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/fluidbean.dir/cmake_clean.cmake
.PHONY : CMakeFiles/fluidbean.dir/clean

CMakeFiles/fluidbean.dir/depend:
	cd /home/bonbonbaron/hack/fluidbean/example/out && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/bonbonbaron/hack/fluidbean /home/bonbonbaron/hack/fluidbean /home/bonbonbaron/hack/fluidbean/example/out /home/bonbonbaron/hack/fluidbean/example/out /home/bonbonbaron/hack/fluidbean/example/out/CMakeFiles/fluidbean.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/fluidbean.dir/depend

