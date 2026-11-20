#[=======================================================================[.rst:
CMakeLists
----------

Top-level CMake build orchestration for ``sed-windows``: Native Windows (MSVC)
and 32-bit Protected-Mode MS-DOS (OpenWatcom v2) ports of GNU ``sed`` ``4.10``.

Orchestrates locating or fetching upstream GNU ``sed`` ``4.10``, deterministic
build-time header and SHA-256 manifest generation (``GenerateSedHeaders``),
POSIX/DOS/MSVC polyfill linking via ``../auto-win-msvc``, strict ISO C89 test
suite compilation and execution, C coverage instrumentation, and CPack
packaging (``ZIP``, ``NSIS``, ``WIX``).

Functions
^^^^^^^^^

.. command:: _sed_configure_upstream_target

  Applies include directories, ``HAVE_CONFIG_H``, and target-specific compiler
  flags (MSVC ``/std:c11`` + ``/FIconfig.h``, OpenWatcom ``wcl386 -bt=dos -mf
  -3r`` + ``-fi=config.h``, or host GCC/Clang ``-include config.h``) to an
  upstream GNU ``sed`` target::

    _sed_configure_upstream_target(<target_name>)

.. command:: _sed_apply_strict_c89_flags

  Applies strict ISO C89/C90 conformance and maximum warning-as-error flags
  (``-std=c89 -pedantic-errors -Werror`` on GCC/Clang, ``/W4 /WX`` on MSVC, and
  ``-za -w4 -wx`` on OpenWatcom) to a test library or executable target::

    _sed_apply_strict_c89_flags(<target_name>)
#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

if(MSVC AND NOT CMAKE_HOST_WIN32 AND NOT CMAKE_NM)
    find_program(_SED_HOST_NM
        NAMES llvm-nm x86_64-w64-mingw32-nm nm
        PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
    )
    if(_SED_HOST_NM)
        set(CMAKE_NM "${_SED_HOST_NM}" CACHE FILEPATH "Path to nm" FORCE)
    endif()
endif()

project(sed_windows VERSION 4.10.0 LANGUAGES C)

if(MSVC AND NOT CMAKE_HOST_WIN32)
    foreach(_flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
    )
        if(DEFINED ${_flag_var})
            string(REPLACE "/Zi" "/Z7" ${_flag_var} "${${_flag_var}}")
        endif()
    endforeach()
    add_link_options(/MANIFEST:NO /DEBUG:NONE /INCREMENTAL:NO)
endif()

include(CTest)
include(FetchContent)

option(SED_TARGET_DOS "Target 32-bit Protected-Mode MS-DOS via OpenWatcom" OFF)
option(SED_ENABLE_COVERAGE "Enable gcov/lcov C code coverage instrumentation" OFF)
option(SED_STATIC_CRT "Link MSVC runtime statically (/MT)" ON)

if(WATCOM OR CMAKE_SYSTEM_NAME STREQUAL "DOS" OR CMAKE_C_COMPILER_ID MATCHES "Watcom")
    set(SED_TARGET_DOS ON CACHE BOOL "Target 32-bit Protected-Mode MS-DOS via OpenWatcom" FORCE)
endif()

# -----------------------------------------------------------------------------
# 1. Locate or Fetch ../auto-win-msvc
# -----------------------------------------------------------------------------
if(DEFINED ENV{AUTO_WIN_MSVC_DIR} AND NOT "$ENV{AUTO_WIN_MSVC_DIR}" STREQUAL "")
    set(_AWM_DEFAULT_DIR "$ENV{AUTO_WIN_MSVC_DIR}")
else()
    set(_AWM_DEFAULT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../auto-win-msvc")
endif()
set(AUTO_WIN_MSVC_DIR "${_AWM_DEFAULT_DIR}" CACHE PATH "Path to local auto-win-msvc checkout")
set(_AWM_LOCAL_DIR "${AUTO_WIN_MSVC_DIR}")
set(_SAVED_BUILD_TESTING "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(AUTO_WIN_MSVC_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(SED_STATIC_CRT)
    set(AUTO_WIN_MSVC_CRT "Static" CACHE STRING "" FORCE)
endif()

if(EXISTS "${_AWM_LOCAL_DIR}/CMakeLists.txt")
    message(STATUS "[sed-windows] Using local auto-win-msvc: ${_AWM_LOCAL_DIR}")
    FetchContent_Declare(
        auto_win_msvc
        SOURCE_DIR "${_AWM_LOCAL_DIR}"
    )
else()
    FetchContent_Declare(
        auto_win_msvc
        GIT_REPOSITORY "https://github.com/SamuelMarks/auto-win-msvc.git"
        GIT_TAG "master"
    )
endif()
FetchContent_MakeAvailable(auto_win_msvc)
set(BUILD_TESTING "${_SAVED_BUILD_TESTING}" CACHE BOOL "" FORCE)

include("${auto_win_msvc_SOURCE_DIR}/cmake/AutoWinMsvcTransform.cmake")

# -----------------------------------------------------------------------------
# 2. Locate or Fetch Upstream GNU sed 4.10 (Zero Edits)
# -----------------------------------------------------------------------------
set(SED_SOURCE_DIR "" CACHE PATH "Path to upstream GNU sed 4.10 source tree")

if(SED_SOURCE_DIR AND EXISTS "${SED_SOURCE_DIR}/sed/sed.h")
    get_filename_component(SED_SOURCE_DIR "${SED_SOURCE_DIR}" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../sed/sed/sed.h" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../sed/lib/stdckdint.in.h")
    get_filename_component(SED_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sed" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sed/sed/sed.h" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sed/lib/stdckdint.in.h")
    get_filename_component(SED_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sed" ABSOLUTE)
else()
    message(STATUS "[sed-windows] Fetching upstream GNU sed 4.10 release tarball...")
    FetchContent_Declare(
        gnu_sed_upstream
        URL
            "https://ftp.gnu.org/gnu/sed/sed-4.10.tar.xz"
            "https://ftpmirror.gnu.org/sed/sed-4.10.tar.xz"
        URL_HASH SHA256=b8e72182b2ec96a3574e2998c47b7aaa64cc20ce000d8e9ac313cc07cecf28c7
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(gnu_sed_upstream)
    set(SED_SOURCE_DIR "${gnu_sed_upstream_SOURCE_DIR}")
endif()

message(STATUS "[sed-windows] Upstream GNU sed 4.10 source dir: ${SED_SOURCE_DIR}")

# -----------------------------------------------------------------------------
# 3. Generate Build-Time Headers & SHA-256 Manifest in ${CMAKE_BINARY_DIR}
# -----------------------------------------------------------------------------
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSedHeaders.cmake"
)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSedHeaders.cmake")

sed_extract_upstream_version("${SED_SOURCE_DIR}" SED_VERSION)
set(SED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/sed_generated")

set(_GEN_FLAGS "")
if(SED_TARGET_DOS)
    list(APPEND _GEN_FLAGS TARGET_DOS)
elseif(WIN32)
    list(APPEND _GEN_FLAGS TARGET_WIN32)
endif()

sed_generate_build_files(
    SED_DIR "${SED_SOURCE_DIR}"
    OUT_DIR "${SED_GEN_DIR}"
    ${_GEN_FLAGS}
)

# -----------------------------------------------------------------------------
# 4. Define Unmodified Upstream Source Lists (GNU sed 4.10)
# -----------------------------------------------------------------------------
set(SED_GNULIB_REL_SOURCES
    acl-errno-valid.c
    acl-internal.c
    basename-lgpl.c
    binary-io.c
    c-ctype.c
    c-strcasecmp.c
    c32isprint.c
    c32isspace.c
    c32rtomb.c
    c32tob.c
    c32tolower.c
    c32toupper.c
    canonicalize-lgpl.c
    cloexec.c
    copy-acl.c
    dfa.c
    dirname-lgpl.c
    error.c
    exitfail.c
    file-has-acl.c
    fseterr.c
    get-permissions.c
    getdelim.c
    getfilecon.c
    glthread/lock.c
    glthread/once.c
    glthread/threadlib.c
    hard-locale.c
    ialloc.c
    issymlink.c
    localcharset.c
    localeinfo.c
    malloc/dynarray_at_failure.c
    malloc/dynarray_emplace_enlarge.c
    malloc/dynarray_finalize.c
    malloc/dynarray_resize.c
    malloc/dynarray_resize_clear.c
    malloc/scratch_buffer_grow.c
    malloc/scratch_buffer_grow_preserve.c
    malloc/scratch_buffer_set_array_size.c
    malloca.c
    mbrtoc32.c
    mempcpy.c
    memrchr.c
    obstack.c
    progname.c
    qcopy-acl.c
    qset-acl.c
    quotearg.c
    rawmemchr.c
    reallocarray.c
    regex.c
    same-inode.c
    se-context.c
    se-label.c
    se-selinux.c
    set-acl.c
    set-permissions.c
    setlocale_null-unlocked.c
    setlocale_null.c
    stat-time.c
    stdio-consolesafe.c
    stripslash.c
    strverscmp.c
    unicase/tolower.c
    unicase/toupper.c
    unictype/ctype_print.c
    unictype/ctype_space.c
    version-etc-fsf.c
    version-etc.c
    xalloc-die.c
    xmalloc.c
)

if(WIN32 OR SED_TARGET_DOS)
    list(APPEND SED_GNULIB_REL_SOURCES
        btowc.c
        lc-charset-dispatch.c
        mbrtowc.c
        mbsinit.c
        mbtowc-lock.c
        wcrtomb.c
        wctob.c
    )
endif()

if(NOT SED_TARGET_DOS)
    list(APPEND SED_GNULIB_REL_SOURCES fwriting.c)
endif()

if(WIN32 AND NOT SED_TARGET_DOS)
    list(APPEND SED_GNULIB_REL_SOURCES msvc-inval.c msvc-nothrow.c)
endif()

set(SED_GNULIB_RAW_SOURCES "")
foreach(_rel IN LISTS SED_GNULIB_REL_SOURCES)
    list(APPEND SED_GNULIB_RAW_SOURCES "${SED_SOURCE_DIR}/lib/${_rel}")
endforeach()

set(SED_CORE_RAW_SOURCES
    "${SED_SOURCE_DIR}/sed/compile.c"
    "${SED_SOURCE_DIR}/sed/debug.c"
    "${SED_SOURCE_DIR}/sed/execute.c"
    "${SED_SOURCE_DIR}/sed/mbcs.c"
    "${SED_SOURCE_DIR}/sed/regexp.c"
    "${SED_SOURCE_DIR}/sed/sed.c"
    "${SED_SOURCE_DIR}/sed/utils.c"
)

if(SED_TARGET_DOS)
    auto_win_msvc_transform_sources(
        SOURCE_ROOT "${SED_SOURCE_DIR}/lib"
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/transformed_lib"
        OUTPUT_VAR SED_GNULIB_ACTIVE_SOURCES
        SOURCES ${SED_GNULIB_RAW_SOURCES}
    )
    auto_win_msvc_transform_sources(
        SOURCE_ROOT "${SED_SOURCE_DIR}/sed"
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/transformed_sed"
        OUTPUT_VAR SED_CORE_ACTIVE_SOURCES
        SOURCES ${SED_CORE_RAW_SOURCES}
    )
else()
    set(SED_GNULIB_ACTIVE_SOURCES ${SED_GNULIB_RAW_SOURCES})
    set(SED_CORE_ACTIVE_SOURCES ${SED_CORE_RAW_SOURCES})
endif()

# -----------------------------------------------------------------------------
# 5. Helper to Apply Upstream Compilation Flags & Strict Include Ordering
# -----------------------------------------------------------------------------
function(_sed_configure_upstream_target _tgt)
    if(SED_TARGET_DOS)
        target_include_directories(${_tgt} BEFORE PRIVATE
            "${SED_GEN_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/transformed_sed"
            "${CMAKE_CURRENT_BINARY_DIR}/transformed_lib"
            "${SED_SOURCE_DIR}"
            "${SED_SOURCE_DIR}/sed"
            "${SED_SOURCE_DIR}/lib"
        )
    else()
        target_include_directories(${_tgt} BEFORE PRIVATE
            "${SED_GEN_DIR}"
            "${SED_SOURCE_DIR}"
            "${SED_SOURCE_DIR}/sed"
            "${SED_SOURCE_DIR}/lib"
        )
    endif()
    if(_tgt STREQUAL "libsed_gnulib")
        target_link_libraries(${_tgt} PUBLIC auto-win-msvc)
    endif()

    if(MSVC)
        if(SED_STATIC_CRT)
            set_property(TARGET ${_tgt} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
        target_compile_options(${_tgt} PRIVATE
            /std:c11
            /utf-8
            "/FI${SED_GEN_DIR}/config.h"
            /wd4018 /wd4090 /wd4100 /wd4101 /wd4116 /wd4127 /wd4146
            /wd4200 /wd4244 /wd4245 /wd4267 /wd4389 /wd4456
            /wd4457 /wd4459 /wd4701 /wd4702 /wd4706 /wd4996
        )
    elseif(SED_TARGET_DOS)
        target_compile_options(${_tgt} PRIVATE
            -bt=dos -mf -3r -za99 -ze -aa -zq -wcd=124 -wcd=136 -wcd=201 -wcd=202 -wcd=303
            "-fi=${SED_GEN_DIR}/config.h"
        )
    else()
        set_target_properties(${_tgt} PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
        target_compile_options(${_tgt} PRIVATE
            -include "${SED_GEN_DIR}/config.h"
            -Wno-unused-parameter
            -Wno-sign-compare
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# 6. Build libsed_gnulib and Production sed Executable
# -----------------------------------------------------------------------------
add_library(libsed_gnulib STATIC ${SED_GNULIB_ACTIVE_SOURCES})
_sed_configure_upstream_target(libsed_gnulib)

add_executable(sed
    ${SED_CORE_ACTIVE_SOURCES}
    "${SED_GEN_DIR}/version.c"
)
target_link_libraries(sed PRIVATE libsed_gnulib)
_sed_configure_upstream_target(sed)

if(SED_TARGET_DOS)
    target_link_options(sed PRIVATE "option" "stack=524288")
endif()

if(SED_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sed PRIVATE --coverage -O0 -g)
    target_link_options(sed PRIVATE --coverage)
endif()

# -----------------------------------------------------------------------------
# 7. Strict ISO C89 Test Suite (BUILD_TESTING)
# -----------------------------------------------------------------------------
if(BUILD_TESTING)
    # Build libsed_testable for white-box fault-injection & DOS in-process tests
    add_library(libsed_testable STATIC
        ${SED_CORE_ACTIVE_SOURCES}
        "${SED_GEN_DIR}/version.c"
    )
    target_link_libraries(libsed_testable PUBLIC libsed_gnulib)
    _sed_configure_upstream_target(libsed_testable)
    target_include_directories(libsed_testable BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_definitions(libsed_testable PRIVATE
        SED_TESTABLE_BUILD=1
        main=sed_testable_main
    )
    if(SED_TARGET_DOS)
        target_compile_definitions(libsed_testable PRIVATE lint=1)
    endif()

    if(SED_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(libsed_testable PRIVATE --coverage -O0 -g)
        target_link_libraries(libsed_testable PUBLIC --coverage)
    endif()

    function(_sed_apply_strict_c89_flags _test_tgt)
        target_include_directories(${_test_tgt} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
            "${SED_GEN_DIR}"
        )
        if(MSVC)
            if(SED_STATIC_CRT)
                set_property(TARGET ${_test_tgt} PROPERTY
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            endif()
            target_compile_options(${_test_tgt} PRIVATE /W4 /WX /TC /utf-8 /D_CRT_SECURE_NO_WARNINGS)
        elseif(SED_TARGET_DOS)
            # Strict ANSI C89 (-za, NOT -za99) with warnings-as-errors (-w4 -wx)
            target_compile_options(${_test_tgt} PRIVATE -bt=dos -mf -3r -za -zq -w4 -wx)
            if(TARGET ${_test_tgt})
                get_target_property(_ttype ${_test_tgt} TYPE)
                if(_ttype STREQUAL "EXECUTABLE")
                    target_link_options(${_test_tgt} PRIVATE "option" "stack=163840")
                endif()
            endif()
        else()
            set_target_properties(${_test_tgt} PROPERTIES
                C_STANDARD 90
                C_STANDARD_REQUIRED ON
                C_EXTENSIONS OFF
            )
            target_compile_options(${_test_tgt} PRIVATE
                -std=c89
                -pedantic
                -pedantic-errors
                -Wall
                -Wextra
                -Werror
                -Wdeclaration-after-statement
                -Wstrict-prototypes
                -Wmissing-prototypes
                -Wold-style-definition
            )
            if(SED_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${_test_tgt} PRIVATE --coverage -O0 -g)
                get_target_property(_ttype ${_test_tgt} TYPE)
                if(_ttype STREQUAL "EXECUTABLE")
                    target_link_options(${_test_tgt} PRIVATE --coverage)
                endif()
            endif()
        endif()
    endfunction()

    add_library(sed_test_harness STATIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/sed_test_harness.c")
    _sed_apply_strict_c89_flags(sed_test_harness)
    if(NOT MSVC AND NOT SED_TARGET_DOS)
        target_compile_definitions(sed_test_harness PRIVATE _POSIX_C_SOURCE=200809L)
    endif()

    add_library(sed_test_inproc STATIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/sed_test_inproc.c")
    _sed_apply_strict_c89_flags(sed_test_inproc)
    if(NOT MSVC AND NOT SED_TARGET_DOS)
        target_compile_definitions(sed_test_inproc PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    target_link_libraries(sed_test_inproc PUBLIC libsed_testable sed_test_harness)

    set(_SED_C89_TESTS
        t_hygien:test_upstream_immutability.c
        t_cli:test_sed_cli_options.c
        t_cmds:test_sed_addresses_cmds.c
        t_reg410:test_sed_410_regressions.c
        t_white:test_sed_whitebox.c
    )

    foreach(_spec IN LISTS _SED_C89_TESTS)
        string(REPLACE ":" ";" _parts "${_spec}")
        list(GET _parts 0 _exe_name)
        list(GET _parts 1 _src_file)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src_file}")
            add_executable(${_exe_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src_file}")
            _sed_apply_strict_c89_flags(${_exe_name})
            if(NOT MSVC AND NOT SED_TARGET_DOS)
                target_compile_definitions(${_exe_name} PRIVATE _POSIX_C_SOURCE=200809L)
            endif()
            if(_exe_name STREQUAL "t_white" OR _exe_name STREQUAL "t_cli")
                target_link_libraries(${_exe_name} PRIVATE sed_test_inproc)
            elseif(SED_TARGET_DOS AND NOT _exe_name STREQUAL "t_hygien")
                target_compile_definitions(${_exe_name} PRIVATE SED_DOS_INPROC=1)
                target_link_libraries(${_exe_name} PRIVATE sed_test_inproc)
            else()
                target_link_libraries(${_exe_name} PRIVATE sed_test_harness)
            endif()
            if(NOT _exe_name STREQUAL "t_white")
                add_dependencies(${_exe_name} sed)
            endif()

            if(SED_TARGET_DOS)
                add_test(NAME ${_exe_name} COMMAND ${_exe_name})
            else()
                add_test(
                    NAME ${_exe_name}
                    COMMAND ${_exe_name} "$<TARGET_FILE:sed>"
                )
            endif()
        endif()
    endforeach()

    if(SED_TARGET_DOS)
        # Direct COMMAND.COM -> SED.EXE E2E verification tests for MS-DOS 1.25 / 2.0 / 4.0
        add_test(NAME sed_dos_version COMMAND sed --version)
        set_tests_properties(sed_dos_version PROPERTIES PASS_REGULAR_EXPRESSION "4\\.10")
        add_test(NAME sed_dos_subst COMMAND sed -e "s/foo/bar/g" SEDIN.TXT)
        set_tests_properties(sed_dos_subst PROPERTIES PASS_REGULAR_EXPRESSION "bar")
        add_test(NAME sed_dos_progfile COMMAND sed -f SEDPRG.SED SEDIN.TXT)
        set_tests_properties(sed_dos_progfile PROPERTIES PASS_REGULAR_EXPRESSION "FROM_PRG")
        add_test(NAME sed_dos_ere_backref COMMAND sed -E -e [[s/^(.?)(.?).?\2\1$/OK/]] SEDIN.TXT)
        set_tests_properties(sed_dos_ere_backref PROPERTIES PASS_REGULAR_EXPRESSION "OK")
        add_test(NAME sed_dos_ranges_cmds COMMAND sed -n -e "1,2s/a/A/gp" SEDIN.TXT)
        set_tests_properties(sed_dos_ranges_cmds PROPERTIES PASS_REGULAR_EXPRESSION "AbbA")
        add_test(NAME sed_dos_translit COMMAND sed -e "y/abc/XYZ/" SEDIN.TXT)
        set_tests_properties(sed_dos_translit PROPERTIES PASS_REGULAR_EXPRESSION "XYZ")
    else()
        add_test(
            NAME verify_c89_and_docs
            COMMAND "${CMAKE_COMMAND}"
                "-DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DSED_GEN_DIR=${SED_GEN_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyC89Strict.cmake"
        )
        if(SED_ENABLE_COVERAGE)
            add_custom_target(coverage
                COMMAND "${CMAKE_COMMAND}"
                    "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DRESET_COUNTERS=ON"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyCoverage.cmake"
                COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
                COMMAND "${CMAKE_COMMAND}"
                    "-DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DLCOV_OUT=${CMAKE_CURRENT_BINARY_DIR}/lcov.info"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyCoverage.cmake"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                DEPENDS sed t_hygien t_cli t_cmds t_reg410 t_white
                COMMENT "Running strict C89 test suites and enforcing 100% coverage gates"
                VERBATIM
            )
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# 8. Install & CPack Rules
# -----------------------------------------------------------------------------
install(TARGETS sed RUNTIME DESTINATION bin)

if(SED_TARGET_DOS)
    if(DEFINED ENV{WATCOM})
        foreach(_ext dos32a.exe dos4gw.exe)
            if(EXISTS "$ENV{WATCOM}/binw/${_ext}")
                install(FILES "$ENV{WATCOM}/binw/${_ext}" DESTINATION bin)
            endif()
        endforeach()
    endif()
endif()

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-APACHE"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-CC0"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE-MIT"
    DESTINATION share/licenses/sed-windows
)

include("${CMAKE_CURRENT_SOURCE_DIR}/CPackConfig.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CPackSourceConfig.cmake")
include(CPack)
