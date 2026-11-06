set(TARGET unit_tests)

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination

# =========================
# Requirement
# =========================
if(CMAKE_CROSSCOMPILING)
    find_package(GTest CONFIG QUIET) # built for the target in the sysroot (ex: mingw64-unit-tests image)
endif()
if(CMAKE_CROSSCOMPILING AND NOT GTest_FOUND)
    # No GTest package for the target (ex: MinGW): built from the sources
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
elseif(NOT CMAKE_CROSSCOMPILING)
    find_package(GTest REQUIRED)
endif()

# =========================
# Sources
# =========================
# Project sources: the library target when it exists, else the shared list of the parent (never its entry point)
set(SRC_PROJECT "")
if(NOT XDESK_PACKAGE_LIB)
    foreach(source ${SRC_SHARED})
        if(IS_ABSOLUTE ${source})
            list(APPEND SRC_PROJECT ${source}) # headers (moc), generated qrc
        else()
            list(APPEND SRC_PROJECT ${CMAKE_SOURCE_DIR}/${source})
        endif()
    endforeach()
endif()

set(SRC
    # Entry
    xdesk/Environment.cpp

    # Main
    xdesk/Desktop.cpp

    ## Security (sub-part: security)
    xdesk/security/Crypto.cpp
    xdesk/security/PairingCode.cpp
    xdesk/security/DeviceRegistry.cpp

    ## Network (sub-part: net)
    xdesk/net/Protocol.cpp
    xdesk/net/Server.cpp
    xdesk/net/Datagram.cpp

    ## Host (sub-part: host)
    xdesk/host/Host.cpp
    xdesk/host/FlowControl.cpp
    xdesk/host/LanePolicy.cpp
    xdesk/host/Lanes.cpp

    ## Platform (sub-part: platform)
    xdesk/platform/FrameEncoder.cpp
    xdesk/platform/XkbLayout.cpp

    ## User interface (sub-part: ui)
    xdesk/ui/tty/Ansi.cpp
)
add_executable(${TARGET} ${SRC} ${SRC_PROJECT})

# =========================
# Dependencies
# =========================
target_include_directories(${TARGET} PRIVATE ../include/ ../include/exception/ ../third_party/ .)
if(XDESK_HAS_PIPEWIRE)
    target_include_directories(${TARGET} SYSTEM PRIVATE ../third_party/pipewire) # <pipewire/...>, <spa/...>
endif()
target_link_libraries(${TARGET} PRIVATE ${XDESK_LIBS} GTest::gtest GTest::gtest_main)
target_compile_definitions(${TARGET} PRIVATE ${XDESK_DEFINITIONS})
if(XDESK_PACKAGE_LIB)
    target_link_libraries(${TARGET} PRIVATE ${TARGET_LIB})
endif()
add_dependencies(${TARGET} generated_external_exception_header)
if(WIN32)
    target_link_options(${TARGET} PRIVATE -static-libgcc -static-libstdc++) # Qt stays in its dll
endif()

# =========================
# Modes
# =========================
target_compile_options(${TARGET} PRIVATE -g -ggdb3)
if(NOT WIN32)
    # libutils_asan.a needs the AddressSanitizer runtime at the link
    target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Asan>:-fsanitize=address -fno-omit-frame-pointer>)
    target_link_options(${TARGET} PRIVATE $<$<CONFIG:Asan>:-fsanitize=address>)
endif()

include(GoogleTest)
if(CMAKE_CROSSCOMPILING)
    # Listed when ctest runs (CMAKE_CROSSCOMPILING_EMULATOR, ex: wine), not at the build
    gtest_discover_tests(${TARGET}
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 120 # first start of wine
    )
else()
    gtest_discover_tests(${TARGET})
endif()
