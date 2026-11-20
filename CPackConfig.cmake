#[=======================================================================[.rst:
CPackConfig
-----------

Binary installer and archive packaging configuration for ``sed-windows``.

Configures CPack metadata, license file bundling, target architecture naming,
and installer generators (``ZIP``, ``NSIS``, ``WIX`` on Windows; ``ZIP`` on
32-bit MS-DOS and POSIX hosts) for GNU ``sed`` ``4.10``.
#]=======================================================================]

set(CPACK_PACKAGE_NAME "sed")
set(CPACK_PACKAGE_VENDOR "SamuelMarks")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GNU sed 4.10 Native Windows (MSVC) & MS-DOS (OpenWatcom) Build")
set(CPACK_PACKAGE_VERSION "${SED_VERSION}")
if(NOT CPACK_PACKAGE_VERSION)
    set(CPACK_PACKAGE_VERSION "4.10.0")
elseif(CPACK_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+$")
    set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}.0")
endif()

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-MIT")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
endif()

if(SED_TARGET_DOS)
    set(CPACK_SYSTEM_NAME "dos32")
    set(CPACK_GENERATOR "ZIP")
elseif(WIN32)
    if(CMAKE_VS_PLATFORM_NAME)
        string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _sed_arch)
    elseif(CMAKE_SYSTEM_PROCESSOR)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _sed_arch)
    else()
        set(_sed_arch "x64")
    endif()
    set(CPACK_SYSTEM_NAME "win-${_sed_arch}")
    set(CPACK_GENERATOR "ZIP")
    find_program(_SED_MAKENSIS NAMES makensis)
    if(_SED_MAKENSIS)
        list(APPEND CPACK_GENERATOR "NSIS")
    endif()
    find_program(_SED_WIX_CANDLE NAMES candle wix)
    if(_SED_WIX_CANDLE)
        list(APPEND CPACK_GENERATOR "WIX")
    endif()
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_WIX_UPGRADE_GUID "8F4E5B2A-61D4-4C93-B9E1-72A4D6F0410C")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CPACK_SYSTEM_NAME)
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${SED_VERSION}-${CPACK_SYSTEM_NAME}")
