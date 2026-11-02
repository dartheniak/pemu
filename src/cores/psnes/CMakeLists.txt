cmake_minimum_required(VERSION 3.10)

# pppEmu version: the ps4 package version, shown on the loading screen and in pppemu.log.
# Raise it with every release so the ps4 sees a new version.
set(PPPEMU_VERSION "1.02")
#set(CMAKE_VERBOSE_MAKEFILE ON)

project(psnes)

##############
# SNES9X
##############
set(OPTION_SOFT_SCALERS OFF CACHE BOOL "Build with software scalers support")
set(OPTION_SOUND ON CACHE BOOL "Build with audio support")
set(OPTION_SCREENSHOT ON CACHE BOOL "Build with screenshot support")
set(OPTION_NETPLAY OFF CACHE BOOL "Build with screenshot support")
set(OPTION_GAMEPAD ON CACHE BOOL "Build with gamepad support")
set(OPTION_GZIP ON CACHE BOOL "Build with gzip support")
set(OPTION_ZIP ON CACHE BOOL "Build with zip support")
set(OPTION_JMA OFF CACHE BOOL "Build with jma support")
set(OPTION_DEBUGGER OFF CACHE BOOL "Build with debugger support")

set(CMAKE_CXX_STANDARD 17)

set(SNES9X_INC
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/filter
)

file(GLOB SNES9X_SRC
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/dsp/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/*.c*
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/filter/*.c*
)

list(REMOVE_ITEM SNES9X_SRC
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/spc7110dec.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/spc7110emu.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/srtcemu.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/algorithms.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/core.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/memory.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/timing.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/smp/iplrom.cpp
        ${CMAKE_SOURCE_DIR}/external/cores/snes9x/apu/bapu/dsp/SPC_DSP.cpp
)

set(SNES9X_CFLAGS
        -fomit-frame-pointer -fno-exceptions
        -pedantic -Wall -W -Wno-unused-parameter
        -DHAVE_MKSTEMP -DHAVE_STRINGS_H
        -DHAVE_SYS_IOCTL_H -DHAVE_STDINT_H -DRIGHTSHIFT_IS_SAR
        -DGFX_MULTI_FORMAT -D__LIBRETRO__
        -D__BYTE_ORDER__=__ORDER_LITTLE_ENDIAN__
        -D__PSNES__
        -DPPPEMU_VERSION="${PPPEMU_VERSION}"
        -D__PEMU_VERSION_MAJOR__=${VERSION_MAJOR}
        -D__PEMU_VERSION_MINOR__=${VERSION_MINOR}
)

set(SNES9X_LDFLAGS m z)

#####################
# OPTIONS
#####################
if (OPTION_SOFT_SCALERS)
    list(APPEND SNES9X_CFLAGS -D__SOFT_SCALERS__)
endif (OPTION_SOFT_SCALERS)
if (OPTION_SOUND)
else ()
    list(APPEND SNES9X_CFLAGS -DNOSOUND)
endif (OPTION_SOUND)
if (OPTION_SCREENSHOT)
endif (OPTION_SCREENSHOT)
if (OPTION_NETPLAY)
endif (OPTION_NETPLAY)
if (OPTION_GAMEPAD)
    list(APPEND SNES9X_CFLAGS -DJOYSTICK_SUPPORT)
endif (OPTION_GAMEPAD)
if (OPTION_GZIP)
    list(APPEND SNES9X_CFLAGS -DZLIB)
endif (OPTION_GZIP)
if (OPTION_ZIP)
    list(APPEND SNES9X_CFLAGS -DUNZIP_SUPPORT -DSYSTEM_ZIP)
endif (OPTION_ZIP)
if (OPTION_JMA)
    set(JMA_INC ${CMAKE_SOURCE_DIR}/external/cores/snes9x/jma)
    file(GLOB JMA_SRC ${CMAKE_SOURCE_DIR}/external/cores/snes9x/jma/*.c*)
    list(APPEND SNES9X_CFLAGS -DJMA_SUPPORT)
endif (OPTION_JMA)
if (OPTION_DEBUGGER)
endif (OPTION_DEBUGGER)

##############
# psnes
##############
set(PSNES_INC sources)
file(GLOB PSNES_SRC sources/*.c*)
list(APPEND PSNES_SRC ../main.cpp)

#####################
# PLATORM SPECIFIC
#####################
if (PLATFORM_VITA)
    #####################
    # PSP2 PLATORM
    #####################
    set(TITLE_ID "PSNE00001" CACHE STRING "" FORCE)
    list(APPEND SNES9X_LDFLAGS ScePower_stub)
elseif (PLATFORM_PS4)
    # pppEmu: own title id so it never clashes with the official pEMU apps
    set(PS4_PKG_TITLE "pppEmu")
    set(PS4_PKG_TITLE_ID "PPPE00001")
    set(PS4_PKG_VERSION "${PPPEMU_VERSION}")
elseif (PLATFORM_SWITCH)
    #####################
    # NX PLATORM
    #####################
    list(APPEND SNES9X_CFLAGS -Wno-pedantic -Wno-missing-field-initializers -Wno-format-truncation)
elseif (PLATFORM_WINDOWS)
    list(REMOVE_ITEM SNES9X_SRC ${CMAKE_SOURCE_DIR}/external/cores/snes9x/compat.cpp)
elseif (PLATFORM_ANDROID)
    #####################
    # ANDROID PLATORM
    #####################
    if (ANDROID_ABI STREQUAL armeabi-v7a)
        list(APPEND PGEN_CFLAGS -marm -mcpu=cortex-a7 -mfpu=neon
                -ffunction-sections -fomit-frame-pointer) # fix crash with release optimizations
    endif ()
    list(APPEND GENESIS_CFLAGS -DHAVE_SYS_PARAM_H)
endif ()

add_executable(${PROJECT_NAME} ${PSNES_SRC} ${SNES9X_SRC} ${UNZIP_SRC} ${JMA_SRC})
target_compile_options(${PROJECT_NAME} PRIVATE ${SNES9X_CFLAGS})
target_include_directories(${PROJECT_NAME} PRIVATE ${PSNES_INC} ${SNES9X_INC} ${UNZIP_INC} ${JMA_INC})
target_link_libraries(${PROJECT_NAME} cross2dui ${SNES9X_LDFLAGS})

# pppEmu: the package says "enter button chosen by the system software" (param.sfo ATTRIBUTE
# 0x20), so the ps4 keyboard and other system dialogs confirm with the console's button
# (cross outside Japan) instead of circle. add_pkg (toolchain) takes the attribute as 6th argument.
if (PLATFORM_PS4)
    function(add_pkg project pkgdir title_id title version)
        _add_pkg("${project}" "${pkgdir}" "${title_id}" "${title}" "${version}" 32)
    endfunction()
endif ()

# include libcross2d targets
include(${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/targets.cmake)

# add common skin data
add_custom_target(${PROJECT_NAME}.data.skin
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/common/datadir -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_datadir -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/${TARGET_PLATFORM}/datadir -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_datadir -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/common/romfs -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_romfs -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/data/${TARGET_PLATFORM}/romfs -D DST=${CMAKE_CURRENT_BINARY_DIR}/data_romfs -P ${CMAKE_SOURCE_DIR}/external/libcross2d/cmake/copy_custom.cmake
)
add_dependencies(${PROJECT_NAME}.data.skin ${PROJECT_NAME}.data)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data.skin)

#####################
# pppEmu: other consoles' games run in other pEMU programs (pgen: sega consoles, pnes: nes,
# pgba: game boy, game boy color, game boy advance, pfbneo: arcade),
# shipped in the same package and started with the game (see src/skeleton/launcher.h)
#####################
set(PPPEMU_AUTH_INFO "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")
function(pppemu_add_program core define)
    if (NOT TARGET ${core})
        return()
    endif ()
    if (PLATFORM_PS4)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ${define}="/app0/${core}.bin")
        # the program as a second signed program of the package: /app0/<core>.bin
        add_custom_target(${PROJECT_NAME}.${core}
                COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}" "${OPENORBIS}/bin/create-fself"
                    "-in=$<TARGET_FILE:${core}>" "-out=${core}.oelf" "--eboot" "${core}.bin"
                    "--paid" "0x3800000000000035" "--authinfo" "${PPPEMU_AUTH_INFO}"
                COMMAND ${CMAKE_COMMAND} -E copy ${core}.bin ${CMAKE_CURRENT_BINARY_DIR}/data_romfs/${core}.bin
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                DEPENDS ${core}
                VERBATIM
        )
        # after the data copy (it empties data_romfs), before the package
        add_dependencies(${PROJECT_NAME}.${core} ${PROJECT_NAME}.data.skin)
        add_dependencies(${PROJECT_NAME}_pkg ${PROJECT_NAME}.${core})
    elseif (PLATFORM_LINUX)
        # desktop test build: the program from the build directory
        target_compile_definitions(${PROJECT_NAME} PRIVATE ${define}="$<TARGET_FILE:${core}>")
        add_dependencies(${PROJECT_NAME} ${core})
    endif ()
endfunction()
pppemu_add_program(pgen PPPEMU_GENESIS_PROGRAM)
pppemu_add_program(pnes PPPEMU_NES_PROGRAM)
pppemu_add_program(pgba PPPEMU_GBA_PROGRAM)
pppemu_add_program(pfbneo PPPEMU_ARCADE_PROGRAM)

# arcade: romset names list for the launcher (fbneo_arcade.tsv in the package), made from the
# FBNeo game list. FBNeo's generated files (pfbneo.deps) are made first if they don't exist yet.
if (TARGET pfbneo AND TARGET pfbneo.deps)
    find_package(Perl REQUIRED)
    set(PPPEMU_FBNEO_DEPS ${CMAKE_BINARY_DIR}/src/cores/pfbneo/deps)
    add_custom_command(
            OUTPUT ${PPPEMU_FBNEO_DEPS}/gamelist.txt
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target pfbneo.deps
            COMMENT "pppEmu: generating FBNeo files (pfbneo.deps), first build only"
    )
    add_custom_target(pppemu.fbneo.deps DEPENDS ${PPPEMU_FBNEO_DEPS}/gamelist.txt)
    add_dependencies(pfbneo pppemu.fbneo.deps)

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fbneo_arcade.tsv
            COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/fbneo_arcade_list.pl
                ${PPPEMU_FBNEO_DEPS}/gamelist.txt ${CMAKE_CURRENT_BINARY_DIR}/fbneo_arcade.tsv
            DEPENDS ${PPPEMU_FBNEO_DEPS}/gamelist.txt ${CMAKE_CURRENT_SOURCE_DIR}/tools/fbneo_arcade_list.pl
    )
    add_custom_target(${PROJECT_NAME}.arcadelist
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/fbneo_arcade.tsv
                ${CMAKE_CURRENT_BINARY_DIR}/data_romfs/fbneo_arcade.tsv
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fbneo_arcade.tsv
    )
    # after the data copy (it empties data_romfs)
    add_dependencies(${PROJECT_NAME}.arcadelist ${PROJECT_NAME}.data.skin)
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.arcadelist)
endif ()
