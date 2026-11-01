cmake_minimum_required(VERSION 3.10)
#set(CMAKE_VERBOSE_MAKEFILE ON)

SET(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
SET(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG "@")

# add libcross2d library. If you want to port pfba to another platform,
# you should (may) only need to port libcross2d library.
add_subdirectory(external/libcross2d)

####################
# BUILD OPTIONS
####################
set(OPTION_EMU OFF CACHE STRING "Emulator to build (pfbneo, pnes, psnes)")
set(OPTION_BUILTIN_LIBCONFIG OFF CACHE BOOL "Build libconfig from sources")
set(OPTION_MPV_PLAYER ON CACHE BOOL "Add support for video snaps (mpv)")
set(OPTION_UPDATE_CONFIG OFF CACHE BOOL "Force config file updating")
# smb support is tested on ps4 (and linux for development), off by default elsewhere
if (PLATFORM_PS4 OR PLATFORM_LINUX)
    set(OPTION_SMB ON CACHE BOOL "Load roms from SMB (samba/windows) network shares")
else ()
    set(OPTION_SMB OFF CACHE BOOL "Load roms from SMB (samba/windows) network shares")
endif ()
####################
# BUILD OPTIONS
####################

# libsmb2 (network roms loading from smb:// paths)
if (OPTION_SMB)
    if (PLATFORM_PS4)
        # libsmb2's own cmake breaks the PS4 toolchain flags, see src/smb2-ps4/CMakeLists.txt
        add_subdirectory(src/smb2-ps4)
    else ()
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ENABLE_UTILS OFF CACHE BOOL "" FORCE)
        set(ENABLE_LIBKRB5 OFF CACHE BOOL "" FORCE)
        set(ENABLE_GSSAPI OFF CACHE BOOL "" FORCE)
        set(ENABLE_LIBDCERPC OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/libsmb2 EXCLUDE_FROM_ALL)
    endif ()
endif ()

# linux/windows screen handling options
if (PLATFORM_LINUX OR PLATFORM_WINDOWS)
    set(OPTION_RESOLUTION ON CACHE BOOL "Add screen resolution option")
    set(OPTION_FULLSCREEN ON CACHE BOOL "Add switch to fullscreen option")
endif ()

# add sscrap (screenscrapper scrapper!) library
add_subdirectory(external/sscrap)

# required for cmake toolchain setup
project(pemu)
set(PROJECT_AUTHOR cpasjuste)
set(VERSION_MAJOR 7)
set(VERSION_MINOR 2)

# pemu (ui)
add_subdirectory(src/skeleton)

# cores
add_subdirectory(src/cores/pfbneo)
add_subdirectory(src/cores/pgen)
add_subdirectory(src/cores/pnes)
add_subdirectory(src/cores/psnes)
add_subdirectory(src/cores/pgba)
