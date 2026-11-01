project(smb2_ps4 C)

# libsmb2 static library for PS4, built without libsmb2's own CMakeLists.txt.
#
# libsmb2's cmake "PS4" mode replaces CMAKE_C_FLAGS with "-funwind-tables",
# dropping the OpenOrbis sysroot/include flags. libsmb2 then gets compiled
# against the build machine's glibc headers and pemu fails to link
# (undefined __isoc23_strtol, fcntl64). The OpenOrbis toolchain file turns
# that mode on by itself, so we can't just avoid it: build the sources here
# instead, with the toolchain flags every other pemu library uses.

set(LIBSMB2_DIR ${CMAKE_SOURCE_DIR}/external/libsmb2)

set(LIBSMB2_SOURCES
        aes.c aes_reference.c aes128ccm.c aes_apple.c alloc.c asn1-ber.c compat.c
        errors.c hmac.c hmac-md5.c init.c libsmb2.c md4c.c md5.c ntlmssp.c pdu.c
        quic.c smbdirect.c sha1.c sha224-256.c sha384-512.c
        smb2-cmd-cancel.c smb2-cmd-close.c smb2-cmd-create.c smb2-cmd-echo.c
        smb2-cmd-error.c smb2-cmd-flush.c smb2-cmd-ioctl.c smb2-cmd-lock.c
        smb2-cmd-logoff.c smb2-cmd-negotiate.c smb2-cmd-notify-change.c
        smb2-cmd-oplock-break.c smb2-cmd-query-directory.c smb2-cmd-query-info.c
        smb2-cmd-read.c smb2-cmd-session-setup.c smb2-cmd-set-info.c
        smb2-cmd-tree-connect.c smb2-cmd-tree-disconnect.c smb2-cmd-write.c
        smb2-data-file-info.c smb2-data-filesystem-info.c
        smb2-data-security-descriptor.c smb2-data-reparse-point.c
        smb2-share-enum.c smb3-seal.c smb2-signing.c socket.c spnego-wrapper.c
        sync.c timestamps.c unicode.c usha.c
)
list(TRANSFORM LIBSMB2_SOURCES PREPEND ${LIBSMB2_DIR}/lib/)

add_library(smb2 STATIC ${LIBSMB2_SOURCES})
target_include_directories(smb2
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LIBSMB2_DIR}/include/smb2 ${LIBSMB2_DIR}/lib
        PUBLIC ${LIBSMB2_DIR}/include)
target_compile_definitions(smb2 PRIVATE
        HAVE_CONFIG_H
        PS4_PLATFORM
        HAVE_DEV_URANDOM
        _FILE_OFFSET_BITS=64
        "_U_=__attribute__((unused))"
        # glibc-style byte swap helpers expected by libsmb2's portable-endian.h
        __bswap_16=__builtin_bswap16
        __bswap_32=__builtin_bswap32
        __bswap_64=__builtin_bswap64)
target_compile_options(smb2 PRIVATE -funwind-tables)

# printed in the build log, to check the PS4 toolchain flags are in use
message(STATUS "pemu: building libsmb2 for PS4 with C flags: ${CMAKE_C_FLAGS}")
