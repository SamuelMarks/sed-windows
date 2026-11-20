#[=======================================================================[.rst:
VerifyC89Strict
---------------

Pure-CMake static hygiene, strict ISO C89 conformance, and 100% documentation
coverage gate for ``sed-windows``.

Script Parameters
^^^^^^^^^^^^^^^^^

``PROJECT_ROOT``
  Path to the ``sed-windows`` repository root (defaults to ``..`` relative to
  this module).

``SED_GEN_DIR``
  Optional path to generated build headers when checking ``tests/*.c`` files
  that include ``upstream_manifest.h`` or ``config.h``.

Functions
^^^^^^^^^

.. command:: sed_verify_no_stray_c_files

  Fails if any ``.c``, ``.h``, or ``.h.in`` file is present in ``PROJECT_ROOT``
  outside ``tests/`` (ignoring ``build*``, ``_deps``, ``.git``, ``.vagrant``,
  ``.watcom``, ``coverage*``, ``dist``, and any local ``sed/`` or
  ``auto-win-msvc/`` dependency checkouts).

.. command:: sed_verify_cmake_rst_docs

  Verifies that every ``.cmake`` module in ``PROJECT_ROOT`` and ``cmake/`` has
  a top-level ``.rst:`` documentation block and documents every public
  ``function()`` definition.

.. command:: sed_verify_c89_comments_and_doxygen

  Verifies that every ``.c`` and ``.h`` file under ``tests/`` contains an
  ``@file`` Doxygen block, zero ``//`` C99 comments outside string/character
  literals, and ``/** ... */`` Doxygen blocks immediately preceding top-level
  function definitions.

.. command:: sed_verify_c89_compiler_syntax

  Invokes the host C compiler with ``-std=c89 -pedantic -pedantic-errors -Wall
  -Wextra -Werror -Wdeclaration-after-statement -Wstrict-prototypes
  -Wmissing-prototypes -Wold-style-definition -fsyntax-only`` on every
  ``tests/*.c`` translation unit when a GCC/Clang-compatible compiler is
  available.
#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

string(ASCII 9  _SED_TAB)
string(ASCII 10 _SED_NL)
string(ASCII 13 _SED_CR)
string(ASCII 34 _SED_DQ)
string(ASCII 92 _SED_BS)

if(NOT DEFINED PROJECT_ROOT OR "${PROJECT_ROOT}" STREQUAL "")
    set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" ABSOLUTE)

# -----------------------------------------------------------------------------
# sed_verify_no_stray_c_files(<root_dir>)
# -----------------------------------------------------------------------------
function(sed_verify_no_stray_c_files _root_dir)
    file(GLOB_RECURSE _all_c_files
        RELATIVE "${_root_dir}"
        "${_root_dir}/*.c"
        "${_root_dir}/*.h"
        "${_root_dir}/*.h.in"
    )
    set(_violations "")
    foreach(_rel IN LISTS _all_c_files)
        if(_rel MATCHES [[^(tests/|build|_deps/|\.git/|\.vagrant/|vagrant/dos/\.vagrant/|vagrant/dos/\.watcom/|vagrant/dos/build|coverage|dist/|sed/|auto-win-msvc/)]])
            continue()
        endif()
        list(APPEND _violations "${_rel}")
    endforeach()
    if(_violations)
        string(JOIN "${_SED_NL}  - " _msg ${_violations})
        message(FATAL_ERROR "[VerifyC89Strict] Forbidden committed C files outside tests/:${_SED_NL}  - ${_msg}")
    endif()
endfunction()

# -----------------------------------------------------------------------------
# sed_verify_cmake_rst_docs(<root_dir>)
# -----------------------------------------------------------------------------
function(sed_verify_cmake_rst_docs _root_dir)
    file(GLOB _cmake_files
        "${_root_dir}/CMakeLists.txt"
        "${_root_dir}/CPack*.cmake"
        "${_root_dir}/cmake/*.cmake"
    )
    foreach(_f IN LISTS _cmake_files)
        file(READ "${_f}" _txt)
        if(NOT _txt MATCHES [[\.rst:]])
            message(FATAL_ERROR "[VerifyC89Strict] Missing .rst module documentation block in ${_f}")
        endif()
        string(REGEX MATCHALL "${_SED_NL}(function|macro)[(][A-Za-z0-9_]+" _funcs "${_txt}")
        foreach(_fn_match IN LISTS _funcs)
            string(REGEX REPLACE "^${_SED_NL}(function|macro)[(]" "" _fn_name "${_fn_match}")
            if(NOT _txt MATCHES "[.][.][ ]+command::[ ]+${_fn_name}")
                message(FATAL_ERROR "[VerifyC89Strict] Undocumented CMake function/macro '${_fn_name}' in ${_f}")
            endif()
        endforeach()
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# sed_verify_c89_comments_and_doxygen(<root_dir>)
# -----------------------------------------------------------------------------
function(sed_verify_c89_comments_and_doxygen _root_dir)
    file(GLOB _test_files
        "${_root_dir}/tests/*.h"
        "${_root_dir}/tests/*.c"
    )
    foreach(_f IN LISTS _test_files)
        file(READ "${_f}" _raw)
        if(NOT _raw MATCHES [[/\*\*.*@file]])
            message(FATAL_ERROR "[VerifyC89Strict] Missing /** @file ... */ header documentation in ${_f}")
        endif()

        # Strip /* ... */ block comments and "..." / '...' literals to detect any // C99 comments
        string(REGEX REPLACE [[/\*([^*]|\*+[^*/])*\*+/]] " " _stripped "${_raw}")
        string(REGEX REPLACE [=["([^"\\]|\\.)*"]=] [[""]] _stripped "${_stripped}")
        string(REGEX REPLACE [=['([^'\\]|\\.)*']=] [['']] _stripped "${_stripped}")
        if(_stripped MATCHES [[//]])
            message(FATAL_ERROR "[VerifyC89Strict] Forbidden C99 '//' comment found in ${_f}")
        endif()

        string(REPLACE "${_SED_CR}${_SED_NL}" "${_SED_NL}" _norm "${_raw}")

        # Verify every non-guard #define macro has a preceding /** ... */ doc comment
        string(REGEX MATCHALL "(${_SED_NL}/[*][*]([^*]|[*]+[^*/])*[*]+/(${_SED_NL}#[ ${_SED_TAB}]*undef[ ${_SED_TAB}]+[A-Za-z_][A-Za-z0-9_]*)?)?${_SED_NL}#[ ${_SED_TAB}]*define[ ${_SED_TAB}]+[A-Za-z_][A-Za-z0-9_]*" _macros "${_norm}")
        foreach(_mdef IN LISTS _macros)
            string(REGEX REPLACE "^.*define[ ${_SED_TAB}]+" "" _mname "${_mdef}")
            if(NOT _mname MATCHES [[_H$]] AND NOT _mdef MATCHES [[/\*\*]])
                message(FATAL_ERROR "[VerifyC89Strict] Undocumented #define macro '${_mname}' in ${_f}")
            endif()
        endforeach()

        # Verify every struct/enum type definition at column 0 has a preceding /** ... */ doc comment
        string(REGEX MATCHALL "(${_SED_NL}/[*][*]([^*]|[*]+[^*/])*[*]+/)?${_SED_NL}(struct|enum)[ ${_SED_TAB}]+[A-Za-z_][A-Za-z0-9_]*[ ${_SED_TAB}${_SED_NL}]*[{]" _types "${_norm}")
        foreach(_tdef IN LISTS _types)
            if(NOT _tdef MATCHES [[/\*\*]])
                string(STRIP "${_tdef}" _tsnip)
                message(FATAL_ERROR "[VerifyC89Strict] Undocumented struct/enum in ${_f}: ${_tsnip}")
            endif()
        endforeach()

        # Verify every top-level function definition in .h/.c has a preceding /** ... */ doc comment and @param/@return
        string(REGEX MATCHALL "(${_SED_NL}/[*][*]([^*]|[*]+[^*/])*[*]+/)?${_SED_NL}(static[ ${_SED_TAB}]+)?(const[ ${_SED_TAB}]+)?(unsigned[ ${_SED_TAB}]+|signed[ ${_SED_TAB}]+|enum[ ${_SED_TAB}]+[A-Za-z0-9_]+[ ${_SED_TAB}]+|struct[ ${_SED_TAB}]+[A-Za-z0-9_]+[ ${_SED_TAB}]+)?[A-Za-z_][A-Za-z0-9_]*[ ${_SED_TAB}*]+[A-Za-z_][A-Za-z0-9_]*[(][^;${_SED_NL}]*[)][ ${_SED_TAB}]*${_SED_NL}?[{]" _defs "${_norm}")
        foreach(_def IN LISTS _defs)
            if(_def MATCHES [=[[ 	*]+(if|for|while|switch)[(]]=])
                continue()
            endif()
            if(NOT _def MATCHES [[/\*\*]])
                string(STRIP "${_def}" _snippet)
                string(LENGTH "${_snippet}" _slen)
                if(_slen GREATER 80)
                    string(SUBSTRING "${_snippet}" 0 80 _snippet)
                endif()
                message(FATAL_ERROR "[VerifyC89Strict] Undocumented C89 function in ${_f}: ${_snippet}")
            endif()
            # Extract signature after closing */ and verify @param and @return tags
            string(REGEX REPLACE "^.*[*]+/${_SED_NL}" "" _sig "${_def}")
            if(NOT _sig MATCHES [=[[(][ 	]*void[ 	]*[)]]=])
                if(NOT _def MATCHES [[@param]])
                    string(STRIP "${_sig}" _ssnip)
                    message(FATAL_ERROR "[VerifyC89Strict] Missing @param documentation on function in ${_f}: ${_ssnip}")
                endif()
            endif()
            if(NOT _sig MATCHES [=[^(static[ 	]+)?void[ 	]+]=] AND NOT _def MATCHES [[@return]])
                string(STRIP "${_sig}" _ssnip)
                message(FATAL_ERROR "[VerifyC89Strict] Missing @return documentation on non-void function in ${_f}: ${_ssnip}")
            endif()
        endforeach()
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# sed_verify_c89_compiler_syntax(<root_dir>)
# -----------------------------------------------------------------------------
function(sed_verify_c89_compiler_syntax _root_dir)
    file(GLOB _c_files "${_root_dir}/tests/*.c")
    if(NOT _c_files)
        return()
    endif()

    find_program(_CC NAMES cc gcc clang)
    if(NOT _CC)
        return()
    endif()

    execute_process(
        COMMAND "${_CC}" --version
        OUTPUT_VARIABLE _cc_ver
        ERROR_VARIABLE _cc_ver_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT "${_cc_ver}${_cc_ver_err}" MATCHES [[clang|GCC|Free Software Foundation]])
        return()
    endif()

    set(_inc_args "-I${_root_dir}/tests" "-D_POSIX_C_SOURCE=200809L")
    if(DEFINED SED_GEN_DIR AND EXISTS "${SED_GEN_DIR}")
        list(APPEND _inc_args "-I${SED_GEN_DIR}")
    elseif(EXISTS "${_root_dir}/build_cov/sed_generated")
        list(APPEND _inc_args "-I${_root_dir}/build_cov/sed_generated")
    elseif(EXISTS "${_root_dir}/build/sed_generated")
        list(APPEND _inc_args "-I${_root_dir}/build/sed_generated")
    endif()

    set(_stub_dir "")
    foreach(_cf IN LISTS _c_files)
        set(_file_args ${_inc_args})
        file(READ "${_cf}" _cf_txt)
        if(_cf_txt MATCHES [[upstream_manifest\.h]])
            set(_has_manifest FALSE)
            foreach(_ia IN LISTS _inc_args)
                string(REGEX REPLACE [[^-I]] "" _idir "${_ia}")
                if(EXISTS "${_idir}/upstream_manifest.h")
                    set(_has_manifest TRUE)
                endif()
            endforeach()
            if(NOT _has_manifest)
                set(_stub_dir "${_root_dir}/build_c89_syntax_stub")
                file(MAKE_DIRECTORY "${_stub_dir}")
                file(WRITE "${_stub_dir}/upstream_manifest.h"
                    "#ifndef SED_UPSTREAM_MANIFEST_H${_SED_NL}"
                    "#define SED_UPSTREAM_MANIFEST_H${_SED_NL}"
                    "#define SED_UPSTREAM_FILE_COUNT 1U${_SED_NL}"
                    "#define SED_UPSTREAM_SOURCE_DIR ${_SED_DQ}.${_SED_DQ}${_SED_NL}"
                    "#define SED_GENERATED_DIR ${_SED_DQ}.${_SED_DQ}${_SED_NL}"
                    "struct sed_upstream_sha256_entry { const char *rel_path; const char *sha256_hex; };${_SED_NL}"
                    "static const struct sed_upstream_sha256_entry g_sed_upstream_manifest[] = {${_SED_NL}"
                    "  { ${_SED_DQ}sed/sed.c${_SED_DQ}, ${_SED_DQ}0${_SED_DQ} },${_SED_NL}"
                    "  { (const char *)0, (const char *)0 }${_SED_NL}"
                    "};${_SED_NL}"
                    "#endif${_SED_NL}"
                )
                list(APPEND _file_args "-I${_stub_dir}")
            endif()
        endif()

        execute_process(
            COMMAND "${_CC}"
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
                -fsyntax-only
                ${_file_args}
                "${_cf}"
            RESULT_VARIABLE _rc
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err
        )
        if(_stub_dir AND EXISTS "${_stub_dir}")
            file(REMOVE_RECURSE "${_stub_dir}")
            set(_stub_dir "")
        endif()
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "[VerifyC89Strict] Strict C89 check failed on ${_cf}:${_SED_NL}${_out}${_SED_NL}${_err}")
        endif()
    endforeach()
endfunction()

sed_verify_no_stray_c_files("${PROJECT_ROOT}")
sed_verify_cmake_rst_docs("${PROJECT_ROOT}")
sed_verify_c89_comments_and_doxygen("${PROJECT_ROOT}")
sed_verify_c89_compiler_syntax("${PROJECT_ROOT}")
message(STATUS "[VerifyC89Strict] All C89 syntax, hygiene, and 100% doc coverage gates passed.")
