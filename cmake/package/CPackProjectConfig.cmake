#**************************************************************#
#Edition:
#**  17/10/2026 by Tsukini
#
#File Name:
#**  CPackProjectConfig.cmake
#
#File Description:
#**  Read by CPack for each generator: install prefix of the
#**  Windows packages (MinGW sysroot of Fedora / Debian)
#**************************************************************#

if(CPACK_GENERATOR STREQUAL "RPM" AND CPACK_XDESK_RPM_PREFIX)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CPACK_XDESK_RPM_PREFIX}")
elseif(CPACK_GENERATOR STREQUAL "DEB" AND CPACK_XDESK_DEB_PREFIX)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CPACK_XDESK_DEB_PREFIX}")
elseif(CPACK_GENERATOR STREQUAL "External")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr") # Arch packages (default: the root)
endif()
