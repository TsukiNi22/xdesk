cmake_minimum_required(VERSION 3.20)
set(TARGET xdesk)
if(NOT CMAKE_TOOLCHAIN_FILE AND NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++) # A toolchain (ex: cmake/toolchain/mingw-w64.cmake) or -DCMAKE_CXX_COMPILER wins
endif()
project(${TARGET} VERSION 1.0.0 LANGUAGES CXX)

# =========================
# Options
# =========================
# Project kind: defaults of the package content options below (each one can still be forced with -D...=ON/OFF)
#   binary      -> XDESK_PACKAGE_BIN=ON  XDESK_PACKAGE_LIB=OFF XDESK_PACKAGE_DEV=OFF (executable only)
#   library     -> XDESK_PACKAGE_BIN=OFF XDESK_PACKAGE_LIB=ON  XDESK_PACKAGE_DEV=ON  (.a / .so + headers + CMake config)
#   header-only -> XDESK_PACKAGE_BIN=OFF XDESK_PACKAGE_LIB=OFF XDESK_PACKAGE_DEV=ON  (headers + CMake config only)
# A binary can also ship its library and its headers: -DXDESK_PACKAGE_LIB=ON -DXDESK_PACKAGE_DEV=ON
set(XDESK_KIND "binary" CACHE STRING "Project kind: binary, library, header-only")
set_property(CACHE XDESK_KIND PROPERTY STRINGS binary library header-only)
if(XDESK_KIND STREQUAL "binary")
    set(KIND_BIN ON)
    set(KIND_LIB OFF)
    set(KIND_DEV OFF)
elseif(XDESK_KIND STREQUAL "library")
    set(KIND_BIN OFF)
    set(KIND_LIB ON)
    set(KIND_DEV ON)
elseif(XDESK_KIND STREQUAL "header-only")
    set(KIND_BIN OFF)
    set(KIND_LIB OFF)
    set(KIND_DEV ON)
else()
    message(FATAL_ERROR "Unknown XDESK_KIND '${XDESK_KIND}' (binary, library, header-only)")
endif()

# Package content (what is built, installed and packaged)
option(XDESK_PACKAGE_BIN "Build and package the executable (xdesk-op / -db / -as)" ${KIND_BIN})
option(XDESK_PACKAGE_LIB "Build and package the library (lib<name>-op / -db / -as)" ${KIND_LIB})
option(XDESK_PACKAGE_DEV "Package the public headers and the CMake config (lib<name>-dev)" ${KIND_DEV})
option(XDESK_SHARED_LIB "Shared library (.so / .dll) instead of a static one (.a)" OFF)

# Package building (set by the package_release target)
option(XDESK_DEV_PACKAGE "Build development package only" OFF)
option(XDESK_PACKAGE "Build global package" OFF)
option(XDESK_STABLE_RELEASE "Channel as stable release" OFF)
option(BUILD_TESTS "Build unit tests" OFF)

if(NOT XDESK_PACKAGE_BIN AND NOT XDESK_PACKAGE_LIB AND NOT XDESK_PACKAGE_DEV)
    message(FATAL_ERROR "Nothing to build: enable XDESK_PACKAGE_BIN, XDESK_PACKAGE_LIB or XDESK_PACKAGE_DEV")
endif()

# Target names: the executable keeps the project name, the library gets '_lib' only when both exist
if(XDESK_PACKAGE_BIN)
    set(TARGET_LIB ${TARGET}_lib)
else()
    set(TARGET_LIB ${TARGET})
endif()
string(REGEX REPLACE "^lib" "" LIB_NAME ${TARGET}) # lib<name>.a, never liblib<name>.a

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Moderne compilation -fPIE

# =========================
# Requirement
# =========================
set(LIBUTILS_VERSION 4.1.0) # Minimal libutils (v4.1.0: Windows packages mingw64-libutils*)
find_package(utils ${LIBUTILS_VERSION} CONFIG REQUIRED) # Check utils dependencies existance
find_package(PkgConfig REQUIRED)
find_package(Python3 REQUIRED) # Used for the special dependencie
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network) # QSslServer since 6.4
find_package(OpenSSL 3.2 REQUIRED) # Argon2id, ML-KEM hybrid TLS 1.3 groups
find_package(JPEG REQUIRED) # libjpeg-turbo: tiles of the frames
find_package(Threads REQUIRED)
set(CMAKE_AUTOMOC ON) # Q_OBJECT classes (headers added to the targets below)
set(CMAKE_AUTORCC ON) # web client + android app embedded in the binary

# Optional backends (detected, never required)
set(XDESK_HAS_DBUS OFF)
set(XDESK_HAS_PIPEWIRE OFF)
set(XDESK_HAS_XTEST OFF)
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus) # xdg-desktop-portal (Wayland capture)
    if(Qt6DBus_FOUND)
        set(XDESK_HAS_DBUS ON)
    endif()
    # Wayland real time capture: PipeWire headers vendored (third_party/pipewire), libpipewire loaded at run time
    if(XDESK_HAS_DBUS)
        set(XDESK_HAS_PIPEWIRE ON)
    endif()
    pkg_check_modules(XTEST QUIET IMPORTED_TARGET xtst x11) # X11 input injection (uinput otherwise)
    if(XTEST_FOUND)
        set(XDESK_HAS_XTEST ON)
    endif()
endif()
message(STATUS "xdesk backends: dbus=${XDESK_HAS_DBUS} pipewire=${XDESK_HAS_PIPEWIRE} xtest=${XDESK_HAS_XTEST}")
find_program(CCACHE_PROGRAM ccache) # Used for cache during compilation
if (CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# =========================
# Warnings
# =========================
add_compile_options(
    -W
    -Wall
    -Wextra
    -Wpedantic
    -Wunused-parameter
    -Wshadow
    -Wuninitialized
)

# =========================
# Sources
# =========================
# Executable only (never in the library)
set(SRC
    # Entry
    src/xdesk/main.cpp
)

# Shared by the executable and the library (the <name>_lib target when both exist)
set(SRC_SHARED
    # Entry
    src/xdesk/Desktop-init.cpp

    # Main
    src/xdesk/Desktop.cpp

    ## Core (sub-part: core)
    src/xdesk/core/Config.cpp
    src/xdesk/core/AuditLog.cpp

    ## Security (sub-part: security)
    src/xdesk/security/Crypto.cpp
    src/xdesk/security/Certificate.cpp
    src/xdesk/security/DeviceRegistry.cpp
    src/xdesk/security/PairingCode.cpp
    src/xdesk/security/IntegrityGuard.cpp

    ## Network (sub-part: net)
    src/xdesk/net/Protocol.cpp
    src/xdesk/net/WebSocket.cpp
    src/xdesk/net/Server.cpp
    src/xdesk/net/Datagram.cpp
    src/xdesk/net/Exposure.cpp

    ## Host (sub-part: host)
    src/xdesk/host/Session.cpp
    src/xdesk/host/Stream.cpp
    src/xdesk/host/FlowControl.cpp
    src/xdesk/host/LanePolicy.cpp
    src/xdesk/host/UdpServer.cpp
    src/xdesk/host/Lanes.cpp
    src/xdesk/host/Host.cpp

    ## Client (sub-part: client)
    src/xdesk/client/Client.cpp
    src/xdesk/client/UdpLink.cpp

    ## Platform (sub-part: platform)
    src/xdesk/platform/FrameEncoder.cpp
    src/xdesk/platform/ScreenCapture.cpp
    src/xdesk/platform/QtCapture.cpp
    src/xdesk/platform/InputInjector.cpp
    src/xdesk/platform/Keymap.cpp
    src/xdesk/platform/Terminal.cpp

    ## Interface (sub-part: ui)
    src/xdesk/ui/Ui.cpp
    src/xdesk/ui/gui/Theme.cpp
    src/xdesk/ui/gui/HostWindow.cpp
    src/xdesk/ui/gui/ViewerWindow.cpp
    src/xdesk/ui/gui/TerminalWidget.cpp
    src/xdesk/ui/gui/VtScreen.cpp
    src/xdesk/ui/tty/Ansi.cpp
    src/xdesk/ui/tty/HostTui.cpp
    src/xdesk/ui/tty/ClientTui.cpp

    ## Third party (sub-part: third_party)
    third_party/qrcodegen/qrcodegen.cpp
)

# One platform only (selected here, never wrapped in an #ifdef)
set(SRC_UNIX
    src/xdesk/platform/unix/PtyTerminal.cpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/platform/unix/PtyTerminal.hpp # Q_OBJECT (moc)
)
set(SRC_LINUX
    src/xdesk/platform/linux/UInputInjector.cpp
    src/xdesk/platform/linux/XkbLayout.cpp
)
set(SRC_DBUS
    src/xdesk/platform/linux/PortalCapture.cpp
)
set(SRC_XTEST
    src/xdesk/platform/linux/XTestInjector.cpp
)
set(SRC_WINDOWS
    src/xdesk/platform/windows/ConPtyTerminal.cpp
    src/xdesk/platform/windows/SendInputInjector.cpp
    src/xdesk/platform/windows/DxgiCapture.cpp
)
if(WIN32)
    list(APPEND SRC_SHARED ${SRC_WINDOWS})
else()
    list(APPEND SRC_SHARED ${SRC_UNIX})
    if(NOT APPLE)
        list(APPEND SRC_SHARED ${SRC_LINUX})
        if(XDESK_HAS_DBUS)
            list(APPEND SRC_SHARED ${SRC_DBUS})
        endif()
        if(XDESK_HAS_XTEST)
            list(APPEND SRC_SHARED ${SRC_XTEST})
        endif()
    endif()
endif()

# Headers of the Q_OBJECT classes: given to the targets so that AUTOMOC finds them (include/ and src/ are split)
set(HDR_MOC
    ${CMAKE_SOURCE_DIR}/include/xdesk/client/Client.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/client/UdpLink.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/core/AuditLog.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/host/Host.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/host/Session.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/host/Stream.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/host/UdpServer.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/host/Lanes.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/net/Exposure.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/net/Server.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/net/WebSocket.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/platform/ITerminal.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/security/DeviceRegistry.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/security/IntegrityGuard.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/security/PairingCode.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/gui/HostWindow.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/gui/TerminalWidget.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/gui/ViewerWindow.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/gui/VtScreen.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/tty/Ansi.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/tty/ClientTui.hpp
    ${CMAKE_SOURCE_DIR}/include/xdesk/ui/tty/HostTui.hpp
)
list(APPEND SRC_SHARED ${HDR_MOC})

# Embedded resources: web client (web/) + android application (android/build/xdesk.apk when built)
file(GLOB_RECURSE WEB_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/web/*)
set(QRC_CONTENT "<RCC>\n    <qresource prefix=\"/\">\n")
foreach(web_file ${WEB_FILES})
    string(APPEND QRC_CONTENT "        <file alias=\"${web_file}\">${CMAKE_SOURCE_DIR}/${web_file}</file>\n")
endforeach()
if(EXISTS ${CMAKE_SOURCE_DIR}/android/build/xdesk.apk)
    string(APPEND QRC_CONTENT "        <file alias=\"android/xdesk.apk\">${CMAKE_SOURCE_DIR}/android/build/xdesk.apk</file>\n")
    string(APPEND QRC_CONTENT "        <file alias=\"android/version.txt\">${CMAKE_SOURCE_DIR}/android/build/version.txt</file>\n")
endif()
string(APPEND QRC_CONTENT "        <file alias=\"icons/xdesk.svg\">${CMAKE_SOURCE_DIR}/docs/logo.svg</file>\n")
string(APPEND QRC_CONTENT "    </qresource>\n</RCC>\n")
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/xdesk.qrc CONTENT "${QRC_CONTENT}")
list(APPEND SRC_SHARED ${CMAKE_BINARY_DIR}/xdesk.qrc)

# Library: compiled (.a / .so), or INTERFACE when only the headers are shipped (header-only)
if(XDESK_PACKAGE_LIB)
    if(XDESK_SHARED_LIB)
        add_library(${TARGET_LIB} SHARED ${SRC_SHARED})
    else()
        add_library(${TARGET_LIB} STATIC ${SRC_SHARED})
    endif()
elseif(NOT XDESK_PACKAGE_BIN)
    add_library(${TARGET_LIB} INTERFACE)
endif()

# Executable: linked on the library when it exists, else built from the same sources
if(XDESK_PACKAGE_BIN)
    if(XDESK_PACKAGE_LIB)
        add_executable(${TARGET} ${SRC})
    else()
        add_executable(${TARGET} ${SRC} ${SRC_SHARED})
    endif()
endif()

# Compiled targets (modes, includes, exception header)
set(COMPILED_TARGETS "")
if(XDESK_PACKAGE_BIN)
    list(APPEND COMPILED_TARGETS ${TARGET})
endif()
if(XDESK_PACKAGE_LIB)
    list(APPEND COMPILED_TARGETS ${TARGET_LIB})
endif()

# =========================
# Special Targets
# =========================

file(GLOB_RECURSE EXCEPTION_CONFIG_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cmake/config/exceptions/*.json"
)
set(GENERATED_EXCEPTION_HEADER
    "${CMAKE_SOURCE_DIR}/include/exception/generated_external_exception_header.hpp"
)
add_custom_command(
    OUTPUT ${GENERATED_EXCEPTION_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${EXCEPTION_CONFIG_FILES} ${CMAKE_SOURCE_DIR}/cmake/scripts/generate_exception_header.py
    COMMENT "Generating exception header (config changed)"
    VERBATIM
)
add_custom_target(generated_external_exception_header ALL # ALL: also generated for a header-only project
    DEPENDS ${GENERATED_EXCEPTION_HEADER}
)

# =========================
# Release (build Debug, Asan & Optimized)
# =========================
cmake_host_system_information(RESULT XDESK_DEFAULT_JOBS QUERY NUMBER_OF_LOGICAL_CORES)
set(XDESK_JOBS ${XDESK_DEFAULT_JOBS} CACHE STRING "Jobs of the inner builds of release / install_release / package_release (low on small machines: MinGW + Qt is heavy)")
# Asan: not with MinGW (Windows packages mingw64-*, built with the MinGW toolchain)
set(RELEASE_ASAN_COMMANDS "")
set(INSTALL_ASAN_COMMANDS "")
set(PACKAGE_ASAN_COMMANDS "")
if(NOT WIN32)
    list(APPEND RELEASE_ASAN_COMMANDS
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Asan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel ${XDESK_JOBS}
    )
    list(APPEND INSTALL_ASAN_COMMANDS
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Asan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel ${XDESK_JOBS}
    )
    list(APPEND PACKAGE_ASAN_COMMANDS
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Asan -DXDESK_DEV_PACKAGE=OFF -DXDESK_PACKAGE=OFF
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel ${XDESK_JOBS}
    )
endif()

add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel ${XDESK_JOBS}
    ${RELEASE_ASAN_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --parallel ${XDESK_JOBS}
    COMMENT "Building the debug versions Debug & Asan (lib${LIB_NAME}_debug.a, lib${LIB_NAME}_asan.a) and the Optimized one (lib${LIB_NAME}.a; the executable at the root is the last one built), no Asan with MinGW"
    VERBATIM
)
add_custom_target(install_release
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel ${XDESK_JOBS}
    ${INSTALL_ASAN_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install --parallel ${XDESK_JOBS}
    COMMENT "Installing the debug versions Debug & Asan and the Optimized one (the executable: Optimized only, same path), no Asan with MinGW"
    VERBATIM
)

# Packages: one build per variant (-db, -as, -op: executable and/or library), then the headers (-dev*) and the global one
set(PACKAGE_VARIANT_COMMANDS "")
set(PACKAGE_DEV_COMMANDS "")
if(COMPILED_TARGETS)
    list(APPEND PACKAGE_VARIANT_COMMANDS
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Debug -DXDESK_DEV_PACKAGE=OFF -DXDESK_PACKAGE=OFF
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel ${XDESK_JOBS}
        ${PACKAGE_ASAN_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized -DXDESK_DEV_PACKAGE=OFF -DXDESK_PACKAGE=OFF
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel ${XDESK_JOBS}
    )
endif()
if(XDESK_PACKAGE_DEV)
    list(APPEND PACKAGE_DEV_COMMANDS
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized -DXDESK_DEV_PACKAGE=ON -DXDESK_PACKAGE=OFF
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel ${XDESK_JOBS}
    )
endif()

add_custom_target(package_release
    ${PACKAGE_VARIANT_COMMANDS}
    ${PACKAGE_DEV_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR} -DCMAKE_BUILD_TYPE=Optimized -DXDESK_DEV_PACKAGE=OFF -DXDESK_PACKAGE=ON
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package --parallel ${XDESK_JOBS}
    COMMENT "Building the packages: the variants -db, -as, -op (executable and/or library), the headers -dev* and the global one, mingw64-* without Asan with the MinGW toolchain"
    VERBATIM
)

# =========================
# Dependencies
# =========================
foreach(target ${COMPILED_TARGETS})
    target_include_directories(${target} PRIVATE include/exception third_party)
    if(XDESK_HAS_PIPEWIRE)
        target_include_directories(${target} SYSTEM PRIVATE third_party/pipewire) # <pipewire/...>, <spa/...>
    endif()
    add_dependencies(${target} generated_external_exception_header)
endforeach()
if(XDESK_PACKAGE_LIB)
    target_link_libraries(${TARGET_LIB} PUBLIC ${XDESK_LIBS}) # The users of the library need libutils too
    target_compile_definitions(${TARGET_LIB} PUBLIC ${XDESK_DEFINITIONS})
endif()
set(XDESK_LIBS
    utils::utils
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    JPEG::JPEG
    Threads::Threads
)
if(XDESK_HAS_DBUS)
    list(APPEND XDESK_LIBS Qt6::DBus)
endif()
if(XDESK_HAS_PIPEWIRE)
    list(APPEND XDESK_LIBS ${CMAKE_DL_LIBS}) # dlopen of libpipewire-0.3.so.0
endif()
if(XDESK_HAS_XTEST)
    list(APPEND XDESK_LIBS PkgConfig::XTEST)
endif()
if(WIN32)
    list(APPEND XDESK_LIBS d3d11 dxgi user32 gdi32 advapi32 ws2_32 iphlpapi)
elseif(NOT APPLE)
    list(APPEND XDESK_LIBS util) # forkpty
endif()
set(XDESK_DEFINITIONS
    $<$<BOOL:${XDESK_HAS_DBUS}>:XDESK_HAS_DBUS>
    $<$<BOOL:${XDESK_HAS_PIPEWIRE}>:XDESK_HAS_PIPEWIRE>
    $<$<BOOL:${XDESK_HAS_XTEST}>:XDESK_HAS_XTEST>
    XDESK_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN> # no winioctl.h (its _unused member clashes with libutils)
    $<$<BOOL:${WIN32}>:NOMINMAX> # std::min / std::max
)
if(XDESK_PACKAGE_BIN)
    if(XDESK_PACKAGE_LIB)
        target_link_libraries(${TARGET} PRIVATE ${TARGET_LIB})
    else()
        target_link_libraries(${TARGET} PRIVATE ${XDESK_LIBS})
    endif()
    target_compile_definitions(${TARGET} PRIVATE ${XDESK_DEFINITIONS})
    if(WIN32)
        target_link_options(${TARGET} PRIVATE -static-libgcc -static-libstdc++) # Qt stays in its dll (shipped next to the .exe)
        set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE OFF) # console kept: tty mode, --help
    endif()
endif()
if(TARGET ${TARGET_LIB} AND NOT XDESK_PACKAGE_LIB)
    target_link_libraries(${TARGET_LIB} INTERFACE utils::utils) # header-only
endif()

# Unit tests (after the libraries and definitions: they link the same ones)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =========================
# Includes
# =========================
if(XDESK_PACKAGE_BIN)
    target_include_directories(${TARGET} PRIVATE include)
endif()
if(TARGET ${TARGET_LIB})
    if(XDESK_PACKAGE_LIB)
        set(INCLUDE_SCOPE PUBLIC)
    else()
        set(INCLUDE_SCOPE INTERFACE)
    endif()
    target_include_directories(${TARGET_LIB} ${INCLUDE_SCOPE}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

# =========================
# Modes
# =========================
if(XDESK_PACKAGE_LIB)
    set_target_properties(${TARGET_LIB} PROPERTIES
        OUTPUT_NAME "${LIB_NAME}$<$<CONFIG:Debug>:_debug>$<$<CONFIG:Asan>:_asan>"
        EXPORT_NAME ${TARGET} # find_package(xdesk): xdesk::xdesk
    )
endif()

foreach(target ${COMPILED_TARGETS})
    if(WIN32)
        # MinGW: no AddressSanitizer (Asan = Debug), no PLT (not ELF)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug,Asan>:-g -ggdb3>
            $<$<CONFIG:Optimized>:-O3 -ffast-math -funroll-loops -pipe -DNDEBUG>
        )
    else()
        # No -march=native: the executable and the library are distributed (packages)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-g -ggdb3>
            $<$<CONFIG:Asan>:-fsanitize=address -fno-omit-frame-pointer -g -ggdb3>
            $<$<CONFIG:Optimized>:-O3 -ffast-math -funroll-loops -fno-plt -pipe -DNDEBUG>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Asan>:-fsanitize=address>
        )
    endif()
endforeach()

# =========================
# Installation
# =========================
if(NOT XDESK_PACKAGE)
include(GNUInstallDirs) # load variable used for installation
include(CMakePackageConfigHelpers)

# Executable (component bin): Optimized only, same path for every variant (packages in conflict)
if(XDESK_PACKAGE_BIN)
    install(TARGETS ${TARGET}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
    )
endif()

# Library (component lib) and its export (component headers: find_package(xdesk CONFIG REQUIRED))
if(TARGET ${TARGET_LIB})
    install(TARGETS ${TARGET_LIB}
        EXPORT ${TARGET}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT lib
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(XDESK_PACKAGE_DEV)
    # license
    install(
        FILES ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${TARGET}
        COMPONENT headers
    )

    # header
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT headers
        PATTERN "*.in" EXCLUDE
    )

    # CMake setup for find_package(xdesk CONFIG REQUIRED): only with a library target to export
    if(TARGET ${TARGET_LIB})
        set(CONFIG_INSTALL_DIR
            ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
        )

        install(EXPORT ${TARGET}Targets
            FILE ${TARGET}Targets.cmake
            NAMESPACE ${TARGET}::
            DESTINATION ${CONFIG_INSTALL_DIR}
        )

        # Not SECURE!!!!!!!!!! Depends on the 'CMakeFiles/Export' path (one config file per variant: -dev-db, -dev-as, -dev-op)
        # Fuck EXPORT, Fuck Export/<hash>/, Fuck!!!!!!
        set(EXPORT_PATH "CMakeFiles/Export")
        if(XDESK_DEV_PACKAGE)
            # main-headers
            file(GLOB_RECURSE XDESK_EXPORT_FILES "${CMAKE_BINARY_DIR}/${EXPORT_PATH}/*/${TARGET}Targets.cmake")
            install(
                FILES ${XDESK_EXPORT_FILES}
                DESTINATION ${CONFIG_INSTALL_DIR}
                COMPONENT headers
            )

            # sub-headers (compiled library only: an INTERFACE library has no per-config file)
            if(XDESK_PACKAGE_LIB)
                file(GLOB_RECURSE XDESK_EXPORT_FILES "${CMAKE_BINARY_DIR}/${EXPORT_PATH}/*/${TARGET}Targets-debug.cmake")
                install(
                    FILES ${XDESK_EXPORT_FILES}
                    DESTINATION ${CONFIG_INSTALL_DIR}
                    COMPONENT headers-db
                )
                file(GLOB_RECURSE XDESK_EXPORT_FILES "${CMAKE_BINARY_DIR}/${EXPORT_PATH}/*/${TARGET}Targets-asan.cmake")
                install(
                    FILES ${XDESK_EXPORT_FILES}
                    DESTINATION ${CONFIG_INSTALL_DIR}
                    COMPONENT headers-as
                )
                file(GLOB_RECURSE XDESK_EXPORT_FILES
                    "${CMAKE_BINARY_DIR}/${EXPORT_PATH}/*/${TARGET}Targets-optimized.cmake"
                    "${CMAKE_BINARY_DIR}/${EXPORT_PATH}/*/${TARGET}Targets-noconfig.cmake" # No config select optimized by default
                )
                install(
                    FILES ${XDESK_EXPORT_FILES}
                    DESTINATION ${CONFIG_INSTALL_DIR}
                    COMPONENT headers-op
                )
            endif()
        endif()

        configure_package_config_file(
            ${CMAKE_SOURCE_DIR}/cmake/package/Config.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}Config.cmake
            INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
        )

        write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}ConfigVersion.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
        )

        install(
            FILES
                ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}ConfigVersion.cmake
            DESTINATION ${CONFIG_INSTALL_DIR}
            COMPONENT headers
        )
    endif()
endif()

endif()
# =========================
# Utils
# =========================
add_custom_target(get_unregistered_files
    COMMAND bash ${CMAKE_SOURCE_DIR}/cmake/scripts/check_files.sh unregistered ${SRC} ${SRC_SHARED}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# =========================
# Package Building
# =========================
set(CPACK_PACKAGE_VENDOR "TsukiNi22")
set(CPACK_PACKAGE_CONTACT "xartania.contact@gmail.com")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_GENERATOR "RPM;DEB")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/package/CPackProjectConfig.cmake") # install prefix per generator

# Arch Linux (pacman): CPack External generator, one .pkg.tar.zst per component written by cmake/package/CPackArch.cmake
find_program(BSDTAR_PROGRAM bsdtar)
if(NOT WIN32 AND BSDTAR_PROGRAM)
    list(APPEND CPACK_GENERATOR "External")
    set(CPACK_EXTERNAL_ENABLE_STAGING ON)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/package/CPackArch.cmake")
elseif(NOT WIN32)
    message(STATUS "Arch packages skipped: bsdtar (libarchive) not found")
endif()

# Windows (cross compiled): mingw64-*, installed in the MinGW sysroot (found by the MinGW toolchain)
if(WIN32)
    set(PACKAGE_PREFIX "mingw64-")
    set(CPACK_XDESK_RPM_PREFIX "/usr/x86_64-w64-mingw32/sys-root/mingw") # Fedora (mingw64-filesystem)
    set(CPACK_XDESK_DEB_PREFIX "/usr/x86_64-w64-mingw32") # Debian / Ubuntu (mingw-w64)
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "noarch") # Windows binaries: no host architecture
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "all")
    set(CPACK_RPM_PACKAGE_AUTOREQPROV OFF)
    set(CPACK_RPM_SPEC_MORE_DEFINE "%define __os_install_post %{nil}\n%define debug_package %{nil}") # no host strip / debuginfo
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/x86_64-w64-mingw32
        /usr/x86_64-w64-mingw32/sys-root
        /usr/x86_64-w64-mingw32/sys-root/mingw
        /usr/x86_64-w64-mingw32/sys-root/mingw/bin
        /usr/x86_64-w64-mingw32/sys-root/mingw/include
        /usr/x86_64-w64-mingw32/sys-root/mingw/lib
        /usr/x86_64-w64-mingw32/sys-root/mingw/lib/cmake
        /usr/x86_64-w64-mingw32/sys-root/mingw/share
        /usr/x86_64-w64-mingw32/sys-root/mingw/share/licenses
    )
    set(LIBUTILS_PACKAGE "mingw64-libutils")
    set(SYSTEM_DEPENDS "") # no Debian package of the MinGW OpenSSL
    set(RPM_SYSTEM_REQUIRES ", mingw64-openssl") # dll of the sysroot needed by libutils
else()
    set(PACKAGE_PREFIX "")
    set(LIBUTILS_PACKAGE "libutils")
    set(SYSTEM_DEPENDS "")
    set(RPM_SYSTEM_REQUIRES "") # found by the RPM auto requires (openssl-libs)
endif()

# Package bases: the executable <name>, the library lib<name>, the headers with the library (else with the executable)
set(PACKAGE_BIN "${PACKAGE_PREFIX}${TARGET}")
set(PACKAGE_LIB "${PACKAGE_PREFIX}lib${LIB_NAME}")
if(XDESK_PACKAGE_BIN)
    set(PACKAGE_BASE "${PACKAGE_BIN}")
else()
    set(PACKAGE_BASE "${PACKAGE_LIB}")
endif()
if(XDESK_PACKAGE_LIB)
    set(PACKAGE_DEV "${PACKAGE_LIB}-dev")
else()
    set(PACKAGE_DEV "${PACKAGE_BASE}-dev")
endif()

# Setup the channel name
if(XDESK_STABLE_RELEASE)
    set(CHANNEL "")
    set(MIRROR_CHANNEL "-pre")
else()
    set(CHANNEL "-pre")
    set(MIRROR_CHANNEL "")
endif()

# Edit release value (run count)
if(XDESK_STABLE_RELEASE)
    # A stable version should never have to be rebuild and should always be removed or marked as depreacted
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
    set(CPACK_ARCH_PACKAGE_RELEASE "1")
elseif(DEFINED PACKAGE_RELEASE)
    set(CPACK_RPM_PACKAGE_RELEASE "${PACKAGE_RELEASE}")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "${PACKAGE_RELEASE}")
    set(CPACK_ARCH_PACKAGE_RELEASE "${PACKAGE_RELEASE}")
else() # 0 is impossible for github.run_number
    set(CPACK_RPM_PACKAGE_RELEASE "0")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "0")
    set(CPACK_ARCH_PACKAGE_RELEASE "0")
endif()

# Arch: libutils is packaged for Arch since v4.2.0
if(LIBUTILS_VERSION VERSION_LESS 4.2.0)
    set(ARCH_LIBUTILS_VERSION 4.2.0)
else()
    set(ARCH_LIBUTILS_VERSION ${LIBUTILS_VERSION})
endif()

# Version constraint of the packages of the same build: exact for a pre-release, minimal for a stable one
if(XDESK_STABLE_RELEASE)
    set(RPM_SAME_VERSION ">= ${PROJECT_VERSION}")
    set(DEB_SAME_VERSION "(>= ${PROJECT_VERSION})")
    set(ARCH_SAME_VERSION ">=${PROJECT_VERSION}")
else()
    set(RPM_SAME_VERSION "= ${PROJECT_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")
    set(DEB_SAME_VERSION "(= ${PROJECT_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE})")
    set(ARCH_SAME_VERSION "=${PROJECT_VERSION}-${CPACK_ARCH_PACKAGE_RELEASE}")
endif()

# Conflict between the stable & unstable channels of a package (the stable one obsoletes the -pre one)
macro(package_channel_conflict COMPONENT NAME)
    # COMPONENT: "" for the main package, else the component in upper case (LIB, HEADERS-DB...)
    if("${COMPONENT}" STREQUAL "")
        set(RPM_KEY "CPACK_RPM_PACKAGE")
        set(DEB_KEY "CPACK_DEBIAN_PACKAGE")
        set(ARCH_KEY "CPACK_ARCH_PACKAGE")
    else()
        set(RPM_KEY "CPACK_RPM_${COMPONENT}_PACKAGE")
        set(DEB_KEY "CPACK_DEBIAN_${COMPONENT}_PACKAGE")
        set(ARCH_KEY "CPACK_ARCH_${COMPONENT}_PACKAGE")
    endif()
    if(XDESK_STABLE_RELEASE)
        # If a recent stable version exist the <package>-pre installation will result in the stable version
        set(${RPM_KEY}_OBSOLETES "${NAME}${MIRROR_CHANNEL} <= ${PROJECT_VERSION}")
        set(${DEB_KEY}_REPLACES "${NAME}${MIRROR_CHANNEL} (<= ${PROJECT_VERSION})")
        set(${ARCH_KEY}_REPLACES "${NAME}${MIRROR_CHANNEL}<=${PROJECT_VERSION}")
    endif()
    set(${RPM_KEY}_CONFLICTS "${NAME}${MIRROR_CHANNEL}")
    set(${DEB_KEY}_BREAKS "${NAME}${MIRROR_CHANNEL}")
    set(${DEB_KEY}_CONFLICTS "${NAME}${MIRROR_CHANNEL}")
    set(${ARCH_KEY}_CONFLICTS "${NAME}${MIRROR_CHANNEL}")
endmacro()

# Separate the executable, the library & the headers
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
if(XDESK_PACKAGE)
    # Global package: installs the default packages (Optimized variants and headers)
    set(CPACK_PACKAGE_NAME "${PACKAGE_BASE}${CHANNEL}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Project xdesk (global)")
    set(CPACK_COMPONENTS_ALL "")
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF) # In case~
    package_channel_conflict("" "${PACKAGE_BASE}")

    set(RPM_REQUIRES "")
    set(DEB_DEPENDS "")
    set(CPACK_ARCH_PACKAGE_DEPENDS "")
    if(XDESK_PACKAGE_BIN)
        list(APPEND RPM_REQUIRES "${PACKAGE_BIN}-op${CHANNEL} ${RPM_SAME_VERSION}")
        list(APPEND DEB_DEPENDS "${PACKAGE_BIN}-op${CHANNEL} ${DEB_SAME_VERSION}")
        list(APPEND CPACK_ARCH_PACKAGE_DEPENDS "${PACKAGE_BIN}-op${CHANNEL}${ARCH_SAME_VERSION}")
    endif()
    if(XDESK_PACKAGE_LIB)
        list(APPEND RPM_REQUIRES "${PACKAGE_LIB}-op${CHANNEL} ${RPM_SAME_VERSION}")
        list(APPEND DEB_DEPENDS "${PACKAGE_LIB}-op${CHANNEL} ${DEB_SAME_VERSION}")
        list(APPEND CPACK_ARCH_PACKAGE_DEPENDS "${PACKAGE_LIB}-op${CHANNEL}${ARCH_SAME_VERSION}")
    elseif(XDESK_PACKAGE_DEV)
        list(APPEND RPM_REQUIRES "${PACKAGE_DEV}${CHANNEL} ${RPM_SAME_VERSION}")
        list(APPEND DEB_DEPENDS "${PACKAGE_DEV}${CHANNEL} ${DEB_SAME_VERSION}")
        list(APPEND CPACK_ARCH_PACKAGE_DEPENDS "${PACKAGE_DEV}${CHANNEL}${ARCH_SAME_VERSION}")
    endif()
    list(JOIN RPM_REQUIRES ", " CPACK_RPM_PACKAGE_REQUIRES)
    list(JOIN DEB_DEPENDS ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
elseif(XDESK_DEV_PACKAGE)
    # Headers + CMake config (-dev), and the CMake config of each variant (-dev-db, -dev-as, -dev-op)
    set(CPACK_PACKAGE_NAME "${PACKAGE_DEV}${CHANNEL}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Project xdesk (headers/cmake)")
    set(CPACK_COMPONENTS_ALL headers)
    set(CPACK_RPM_HEADERS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_DEBIAN_HEADERS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    package_channel_conflict("" "${PACKAGE_DEV}")
    set(CPACK_RPM_PACKAGE_REQUIRES "${LIBUTILS_PACKAGE}-dev >= ${LIBUTILS_VERSION}") # public headers include <utils/utils.hpp>
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-dev (>= ${LIBUTILS_VERSION})")
    set(CPACK_ARCH_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-dev>=${ARCH_LIBUTILS_VERSION}")

    if(XDESK_PACKAGE_LIB)
        if(WIN32)
            set(DEV_VARIANTS db op) # no Asan with MinGW
        else()
            set(DEV_VARIANTS db as op)
        endif()
        foreach(variant ${DEV_VARIANTS})
            string(TOUPPER "${variant}" VARIANT_UPPER)
            list(APPEND CPACK_COMPONENTS_ALL headers-${variant})
            set(CPACK_RPM_HEADERS-${VARIANT_UPPER}_PACKAGE_NAME "${PACKAGE_DEV}-${variant}${CHANNEL}")
            set(CPACK_DEBIAN_HEADERS-${VARIANT_UPPER}_PACKAGE_NAME "${PACKAGE_DEV}-${variant}${CHANNEL}")
            set(CPACK_RPM_HEADERS-${VARIANT_UPPER}_PACKAGE_REQUIRES "${LIBUTILS_PACKAGE}-${variant} >= ${LIBUTILS_VERSION}") # the variant links libutils of the same variant
            set(CPACK_DEBIAN_HEADERS-${VARIANT_UPPER}_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-${variant} (>= ${LIBUTILS_VERSION})")
            set(CPACK_ARCH_HEADERS-${VARIANT_UPPER}_PACKAGE_NAME "${PACKAGE_DEV}-${variant}${CHANNEL}")
            set(CPACK_ARCH_HEADERS-${VARIANT_UPPER}_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-${variant}>=${ARCH_LIBUTILS_VERSION}")
            package_channel_conflict("HEADERS-${VARIANT_UPPER}" "${PACKAGE_DEV}-${variant}")
        endforeach()
    endif()
else()
    # One variant of the executable and/or the library: -db (Debug), -as (Asan), -op (Optimized, default)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(VARIANT "db")
        set(VARIANT_SUMMARY "debug")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Asan")
        set(VARIANT "as")
        set(VARIANT_SUMMARY "asan")
    else()
        set(VARIANT "op")
        set(VARIANT_SUMMARY "optimized")
    endif()
    set(CPACK_PACKAGE_NAME "${PACKAGE_BASE}-${VARIANT}${CHANNEL}") # file name base: <base>-<variant>-<version>-<system>-<component>
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Project xdesk (${VARIANT_SUMMARY})")
    set(CPACK_COMPONENTS_ALL "")

    # Executable: a single /usr/bin/xdesk -> the variants conflict with each other (one installed at a time)
    if(XDESK_PACKAGE_BIN)
        list(APPEND CPACK_COMPONENTS_ALL bin)
        set(CPACK_RPM_BIN_PACKAGE_NAME "${PACKAGE_BIN}-${VARIANT}${CHANNEL}")
        set(CPACK_DEBIAN_BIN_PACKAGE_NAME "${PACKAGE_BIN}-${VARIANT}${CHANNEL}")
        set(CPACK_ARCH_BIN_PACKAGE_NAME "${PACKAGE_BIN}-${VARIANT}${CHANNEL}")
        set(BIN_CONFLICTS "")
        foreach(other db as op)
            foreach(other_channel "" "-pre")
                if(NOT "${other}${other_channel}" STREQUAL "${VARIANT}${CHANNEL}")
                    list(APPEND BIN_CONFLICTS "${PACKAGE_BIN}-${other}${other_channel}")
                endif()
            endforeach()
        endforeach()
        set(CPACK_ARCH_BIN_PACKAGE_CONFLICTS ${BIN_CONFLICTS}) # list
        set(CPACK_ARCH_BIN_PACKAGE_DEPENDS "openssl") # shared libraries of OpenSSL used by libutils
        list(JOIN BIN_CONFLICTS ", " BIN_CONFLICTS)
        if(XDESK_STABLE_RELEASE)
            set(CPACK_RPM_BIN_PACKAGE_OBSOLETES "${PACKAGE_BIN}-${VARIANT}${MIRROR_CHANNEL} <= ${PROJECT_VERSION}")
            set(CPACK_DEBIAN_BIN_PACKAGE_REPLACES "${PACKAGE_BIN}-${VARIANT}${MIRROR_CHANNEL} (<= ${PROJECT_VERSION})")
            set(CPACK_ARCH_BIN_PACKAGE_REPLACES "${PACKAGE_BIN}-${VARIANT}${MIRROR_CHANNEL}<=${PROJECT_VERSION}")
        endif()
        set(CPACK_RPM_BIN_PACKAGE_CONFLICTS "${BIN_CONFLICTS}")
        set(CPACK_DEBIAN_BIN_PACKAGE_BREAKS "${BIN_CONFLICTS}")
        set(CPACK_DEBIAN_BIN_PACKAGE_CONFLICTS "${BIN_CONFLICTS}")
        if(WIN32)
            set(CPACK_RPM_BIN_PACKAGE_REQUIRES "mingw64-openssl") # dll of the sysroot needed by libutils
        endif()
        if(VARIANT STREQUAL "db" AND NOT WIN32)
            set(CPACK_RPM_BIN_PACKAGE_SUGGESTS "gdb") # Suggests the installation of gdb along side the debug version
            set(CPACK_DEBIAN_BIN_PACKAGE_SUGGESTS "gdb")
            set(CPACK_ARCH_BIN_PACKAGE_OPTDEPENDS "gdb: debugging")
        endif()
    endif()

    # Library: lib<name>.a, lib<name>_debug.a, lib<name>_asan.a (installed side by side)
    if(XDESK_PACKAGE_LIB)
        list(APPEND CPACK_COMPONENTS_ALL lib)
        set(CPACK_RPM_LIB_PACKAGE_NAME "${PACKAGE_LIB}-${VARIANT}${CHANNEL}")
        set(CPACK_DEBIAN_LIB_PACKAGE_NAME "${PACKAGE_LIB}-${VARIANT}${CHANNEL}")
        set(CPACK_ARCH_LIB_PACKAGE_NAME "${PACKAGE_LIB}-${VARIANT}${CHANNEL}")
        package_channel_conflict("LIB" "${PACKAGE_LIB}-${VARIANT}")
        if(XDESK_PACKAGE_DEV)
            set(CPACK_RPM_LIB_PACKAGE_REQUIRES "${PACKAGE_DEV}${CHANNEL} ${RPM_SAME_VERSION}, ${PACKAGE_DEV}-${VARIANT}${CHANNEL} ${RPM_SAME_VERSION}, ${LIBUTILS_PACKAGE}-${VARIANT} >= ${LIBUTILS_VERSION}${RPM_SYSTEM_REQUIRES}")
            set(CPACK_DEBIAN_LIB_PACKAGE_DEPENDS "${PACKAGE_DEV}${CHANNEL} ${DEB_SAME_VERSION}, ${PACKAGE_DEV}-${VARIANT}${CHANNEL} ${DEB_SAME_VERSION}, ${LIBUTILS_PACKAGE}-${VARIANT} (>= ${LIBUTILS_VERSION})${SYSTEM_DEPENDS}")
            set(CPACK_ARCH_LIB_PACKAGE_DEPENDS "${PACKAGE_DEV}${CHANNEL}${ARCH_SAME_VERSION}" "${PACKAGE_DEV}-${VARIANT}${CHANNEL}${ARCH_SAME_VERSION}" "${LIBUTILS_PACKAGE}-${VARIANT}>=${ARCH_LIBUTILS_VERSION}")
        else()
            set(CPACK_RPM_LIB_PACKAGE_REQUIRES "${LIBUTILS_PACKAGE}-${VARIANT} >= ${LIBUTILS_VERSION}${RPM_SYSTEM_REQUIRES}")
            set(CPACK_DEBIAN_LIB_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-${VARIANT} (>= ${LIBUTILS_VERSION})${SYSTEM_DEPENDS}")
            set(CPACK_ARCH_LIB_PACKAGE_DEPENDS "${LIBUTILS_PACKAGE}-${VARIANT}>=${ARCH_LIBUTILS_VERSION}")
        endif()
        if(VARIANT STREQUAL "db" AND NOT WIN32)
            set(CPACK_RPM_LIB_PACKAGE_SUGGESTS "gdb")
            set(CPACK_DEBIAN_LIB_PACKAGE_SUGGESTS "gdb")
            set(CPACK_ARCH_LIB_PACKAGE_OPTDEPENDS "gdb: debugging")
        endif()
    endif()
endif()

# Arch (same names, channels and dependencies as the RPM / DEB, read by cmake/package/CPackArch.cmake)
set(CPACK_ARCH_PACKAGE_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_ARCH_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_ARCH_PACKAGE_URL "https://github.com/TsukiNi22/external-desktop")
set(CPACK_ARCH_PACKAGE_PACKAGER "${CPACK_PACKAGE_VENDOR} <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_ARCH_PACKAGE_LICENSE "MIT")
set(CPACK_ARCH_PACKAGE_ARCHITECTURE "any") # global package & headers
set(CPACK_ARCH_BIN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_ARCH_LIB_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

# Debian
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TsukiNi22")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/TsukiNi22/external-desktop")

include(CPack)
