#**************************************************************#
#Edition:
#**  17/10/2026 by Tsukini
#
#File Name:
#**  mingw-w64.cmake
#
#File Description:
#**  Toolchain used to cross compile the project for Windows (x86_64)
#**  cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake
#**************************************************************#

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Toolchain prefix (override with -DMINGW_PREFIX=<triplet>)
if(NOT DEFINED MINGW_PREFIX)
    set(MINGW_PREFIX x86_64-w64-mingw32)
endif()

set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Sysroot of the target (Fedora: mingw64-*, Debian/Ubuntu: mingw-w64), where mingw64-libutils* is installed
foreach(root /usr/${MINGW_PREFIX}/sys-root/mingw /usr/${MINGW_PREFIX})
    if(EXISTS ${root})
        list(APPEND CMAKE_FIND_ROOT_PATH ${root})
    endif()
endforeach()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)  # Host programs (python3, ccache)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)   # Target libraries (libutils, OpenSSL)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run the tests with wine when available (dll of the sysroot found through WINEPATH)
find_program(WINE_PROGRAM NAMES wine64 wine)
if(WINE_PROGRAM)
    list(GET CMAKE_FIND_ROOT_PATH 0 MINGW_SYSROOT)
    set(CMAKE_CROSSCOMPILING_EMULATOR
        ${CMAKE_COMMAND} -E env WINEPATH=${MINGW_SYSROOT}/bin WINEDEBUG=-all
        ${WINE_PROGRAM}
    )
endif()
