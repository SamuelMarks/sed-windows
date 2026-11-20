#[=======================================================================[.rst:
VerifyCoverage
--------------

Pure-CMake coverage capture and threshold enforcement gate for ``sed-windows``
across Host GCC/Clang (``gcov`` / ``lcov``) and Native Windows MSVC
(``OpenCppCoverage`` Cobertura XML).

Script Parameters
^^^^^^^^^^^^^^^^^

``BUILD_DIR``
  Path to the coverage-instrumented CMake binary directory (default: ``build_cov``).

``PROJECT_ROOT``
  Path to the ``sed-windows`` repository root (default: parent of ``cmake/``).

``COBERTURA_XML``
  Optional path to an ``OpenCppCoverage`` Cobertura XML report on Windows MSVC.

``COBERTURA_MIN_PCT``
  Minimum required aggregate line-rate percentage (``0``..``100``) when
  validating ``COBERTURA_XML`` (default: ``90``).

``LCOV_OUT``
  Optional output path for the filtered ``lcov.info`` tracefile
  (default: ``${BUILD_DIR}/lcov.info``).

``RESET_COUNTERS``
  When ``ON``, removes all existing ``*.gcda`` counter files under ``BUILD_DIR``
  before CTest runs so stale object-timestamp counters never mask execution.
#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

#[[.rst:
.. command:: sed_verify_lcov_tracefile

  Parses an ``lcov`` tracefile and asserts 100% function, line, and branch
  coverage on ``tests/sed_test_harness.c`` and ``tests/sed_test_inproc.c``,
  100% function coverage across all ``sed/*.c`` units, and strict line/branch
  floors honoring upstream ``LCOV_EXCL_LINE`` / ``LCOV_EXCL_BR`` annotations::

    sed_verify_lcov_tracefile(<lcov_info_path>)
#]]
function(sed_verify_lcov_tracefile _info_file)
    if(NOT EXISTS "${_info_file}")
        message(FATAL_ERROR "[VerifyCoverage] Missing lcov tracefile: ${_info_file}")
    endif()

    file(STRINGS "${_info_file}" _lines)
    set(_cur_sf "")
    set(_sed_lf 0)
    set(_sed_lh 0)
    set(_sed_fnf 0)
    set(_sed_fnh 0)

    foreach(_ln IN LISTS _lines)
        if(_ln MATCHES "^SF:(.*)$")
            set(_cur_sf "${CMAKE_MATCH_1}")
            set(_f_lf 0)
            set(_f_lh 0)
            set(_f_fnf 0)
            set(_f_fnh 0)
            set(_f_brf 0)
            set(_f_brh 0)
        elseif(_ln MATCHES "^LF:([0-9]+)$")
            set(_f_lf "${CMAKE_MATCH_1}")
        elseif(_ln MATCHES "^LH:([0-9]+)$")
            set(_f_lh "${CMAKE_MATCH_1}")
        elseif(_ln MATCHES "^FNF:([0-9]+)$")
            set(_f_fnf "${CMAKE_MATCH_1}")
        elseif(_ln MATCHES "^FNH:([0-9]+)$")
            set(_f_fnh "${CMAKE_MATCH_1}")
        elseif(_ln MATCHES "^BRF:([0-9]+)$")
            set(_f_brf "${CMAKE_MATCH_1}")
        elseif(_ln MATCHES "^BRH:([0-9]+)$")
            set(_f_brh "${CMAKE_MATCH_1}")
        elseif(_ln STREQUAL "end_of_record")
            if(_cur_sf MATCHES [[sed_test_harness\.c$|sed_test_inproc\.c$]])
                get_filename_component(_bn "${_cur_sf}" NAME)
                if(NOT _f_lh EQUAL _f_lf OR NOT _f_fnh EQUAL _f_fnf OR NOT _f_brh EQUAL _f_brf)
                    message(FATAL_ERROR
                        "[VerifyCoverage] ${_bn} failed 100% coverage gate: "
                        "lines=${_f_lh}/${_f_lf}, functions=${_f_fnh}/${_f_fnf}, branches=${_f_brh}/${_f_brf}"
                    )
                endif()
                message(STATUS
                    "[VerifyCoverage] ${_bn}: 100% lines (${_f_lh}/${_f_lf}), "
                    "100% functions (${_f_fnh}/${_f_fnf}), 100% branches (${_f_brh}/${_f_brf})"
                )
            elseif(_cur_sf MATCHES [[/sed/[^/]+\.c$]])
                get_filename_component(_bn "${_cur_sf}" NAME)
                if(NOT _f_fnh EQUAL _f_fnf)
                    message(FATAL_ERROR
                        "[VerifyCoverage] sed/${_bn} failed 100% function coverage gate: "
                        "functions=${_f_fnh}/${_f_fnf}"
                    )
                endif()
                if(_bn STREQUAL "sed.c" OR _bn STREQUAL "mbcs.c")
                    if(NOT _f_lh EQUAL _f_lf OR NOT _f_brh EQUAL _f_brf)
                        message(FATAL_ERROR
                            "[VerifyCoverage] sed/${_bn} failed 100% line/branch gate: "
                            "lines=${_f_lh}/${_f_lf}, branches=${_f_brh}/${_f_brf}"
                        )
                    endif()
                endif()
                math(EXPR _sed_lf "${_sed_lf} + ${_f_lf}")
                math(EXPR _sed_lh "${_sed_lh} + ${_f_lh}")
                math(EXPR _sed_fnf "${_sed_fnf} + ${_f_fnf}")
                math(EXPR _sed_fnh "${_sed_fnh} + ${_f_fnh}")
            endif()
            set(_cur_sf "")
        endif()
    endforeach()

    if(_sed_fnf EQUAL 0 OR _sed_lf EQUAL 0)
        message(FATAL_ERROR "[VerifyCoverage] Zero upstream sed/*.c records found in ${_info_file}")
    endif()
    if(NOT _sed_fnh EQUAL _sed_fnf)
        message(FATAL_ERROR "[VerifyCoverage] Aggregate sed/*.c function coverage ${_sed_fnh}/${_sed_fnf} < 100%")
    endif()
    math(EXPR _sed_line_pct "(${_sed_lh} * 100) / ${_sed_lf}")
    if(_sed_line_pct LESS 97)
        message(FATAL_ERROR
            "[VerifyCoverage] Aggregate unmodified sed/*.c reachable line coverage "
            "${_sed_lh}/${_sed_lf} (${_sed_line_pct}%) fell below 97% floor"
        )
    endif()
    message(STATUS
        "[VerifyCoverage] Unmodified GNU sed 4.10 sed/*.c: "
        "100% functions (${_sed_fnh}/${_sed_fnf}), ${_sed_lh}/${_sed_lf} lines (${_sed_line_pct}%)"
    )
endfunction()

#[[.rst:
.. command:: sed_verify_cobertura_xml

  Validates an ``OpenCppCoverage`` Cobertura XML report produced on Windows MSVC
  and enforces a minimum aggregate ``line-rate`` percentage floor::

    sed_verify_cobertura_xml(<xml_path> [<min_pct>])
#]]
function(sed_verify_cobertura_xml _xml_file)
    if(NOT EXISTS "${_xml_file}")
        message(FATAL_ERROR "[VerifyCoverage] Missing Cobertura XML report: ${_xml_file}")
    endif()
    set(_min_pct 90)
    if(ARGC GREATER 1 AND NOT "${ARGV1}" STREQUAL "")
        set(_min_pct "${ARGV1}")
    endif()

    file(READ "${_xml_file}" _xml)
    if(NOT _xml MATCHES [=[line-rate="([0-9]+(\.[0-9]+)?)"]=])
        message(FATAL_ERROR "[VerifyCoverage] Cobertura XML missing line-rate attribute: ${_xml_file}")
    endif()
    set(_rate_str "${CMAKE_MATCH_1}")
    string(REGEX MATCH "^([0-9]+)(\\.([0-9]+))?$" _rate_match "${_rate_str}")
    set(_rate_int "${CMAKE_MATCH_1}")
    set(_rate_frac "${CMAKE_MATCH_3}00")
    string(SUBSTRING "${_rate_frac}" 0 2 _rate_frac2)
    string(REGEX REPLACE "^0+([0-9])$" "\\1" _rate_frac2 "${_rate_frac2}")
    math(EXPR _rate_pct "${_rate_int} * 100 + ${_rate_frac2}")
    if(_rate_pct LESS _min_pct)
        message(FATAL_ERROR
            "[VerifyCoverage] Cobertura XML line-rate ${_rate_str} (${_rate_pct}%) "
            "fell below ${_min_pct}% floor: ${_xml_file}"
        )
    endif()
    message(STATUS
        "[VerifyCoverage] Verified OpenCppCoverage Cobertura XML (${_xml_file}): "
        "line-rate=${_rate_str} (${_rate_pct}% >= ${_min_pct}%)"
    )
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if(NOT DEFINED PROJECT_ROOT)
        get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    else()
        get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" ABSOLUTE)
    endif()

    if(DEFINED COBERTURA_XML AND NOT "${COBERTURA_XML}" STREQUAL "")
        if(NOT DEFINED COBERTURA_MIN_PCT OR "${COBERTURA_MIN_PCT}" STREQUAL "")
            set(COBERTURA_MIN_PCT 90)
        endif()
        sed_verify_cobertura_xml("${COBERTURA_XML}" "${COBERTURA_MIN_PCT}")
        return()
    endif()

    if(NOT DEFINED BUILD_DIR)
        set(BUILD_DIR "${PROJECT_ROOT}/build_cov")
    endif()
    get_filename_component(BUILD_DIR "${BUILD_DIR}" ABSOLUTE)

    if(RESET_COUNTERS)
        file(GLOB_RECURSE _stale_gcda "${BUILD_DIR}/*.gcda")
        if(_stale_gcda)
            file(REMOVE ${_stale_gcda})
        endif()
        return()
    endif()

    if(NOT DEFINED LCOV_OUT)
        set(LCOV_OUT "${BUILD_DIR}/lcov.info")
    endif()

    find_program(_SED_LCOV_EXE NAMES lcov REQUIRED)
    set(_raw_info "${BUILD_DIR}/lcov_raw.info")

    execute_process(
        COMMAND "${_SED_LCOV_EXE}"
            --capture
            --directory "${BUILD_DIR}"
            --output-file "${_raw_info}"
            --rc branch_coverage=1
            --rc "lcov_excl_br_line=LCOV_EXCL_BR_LINE|LCOV_EXCL_BR"
            --ignore-errors inconsistent,unsupported,deprecated,unused,mismatch,negative,gcov
        RESULT_VARIABLE _cap_rc
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT _cap_rc EQUAL 0)
        message(FATAL_ERROR "[VerifyCoverage] lcov --capture failed with exit code ${_cap_rc}")
    endif()

    execute_process(
        COMMAND "${_SED_LCOV_EXE}"
            --extract "${_raw_info}"
            "*/sed/*.c"
            "*/tests/sed_test_harness.c"
            "*/tests/sed_test_inproc.c"
            --output-file "${LCOV_OUT}"
            --rc branch_coverage=1
            --rc "lcov_excl_br_line=LCOV_EXCL_BR_LINE|LCOV_EXCL_BR"
            --ignore-errors inconsistent,unsupported,deprecated,unused,mismatch,negative
        RESULT_VARIABLE _ext_rc
        OUTPUT_QUIET
        ERROR_QUIET
    )
    file(REMOVE "${_raw_info}")
    if(NOT _ext_rc EQUAL 0)
        message(FATAL_ERROR "[VerifyCoverage] lcov --extract failed with exit code ${_ext_rc}")
    endif()

    sed_verify_lcov_tracefile("${LCOV_OUT}")
endif()
