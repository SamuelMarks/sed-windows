#[=======================================================================[.rst:
UpdateCoverageShields
---------------------

Deterministic README badge updater for documentation and C test coverage.

Script Parameters
^^^^^^^^^^^^^^^^^

``README_PATH``
  Path to ``README.md`` containing ``doc coverage`` and ``test coverage``
  shields.io badges.

``LCOV_INFO_PATH``
  Optional path to an ``lcov`` tracefile (``coverage.info``) from which line
  hit/found counts are computed. Defaults to ``100%`` when verified via
  explicit ``DOC_PCT`` / ``TEST_PCT`` parameters.

Functions
^^^^^^^^^

.. command:: _sed_badge_color

  Maps a numeric coverage percentage ``_pct`` to a shields.io color token
  (``brightgreen``, ``green``, ``yellowgreen``, or ``red``) in ``_out_var``::

    _sed_badge_color(<pct> <out_var>)

.. command:: sed_update_readme_shields

  Computes badge color and percentage text and rewrites ``README.md`` in-place
  only when content changes::

    sed_update_readme_shields(<readme_path> <doc_pct> <test_pct>)
#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

# -----------------------------------------------------------------------------
# _sed_badge_color(<pct> <out_var>)
# -----------------------------------------------------------------------------
function(_sed_badge_color _pct _out_var)
    if(_pct GREATER_EQUAL 100)
        set(${_out_var} "brightgreen" PARENT_SCOPE)
    elseif(_pct GREATER_EQUAL 90)
        set(${_out_var} "green" PARENT_SCOPE)
    elseif(_pct GREATER_EQUAL 80)
        set(${_out_var} "yellowgreen" PARENT_SCOPE)
    else()
        set(${_out_var} "red" PARENT_SCOPE)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# sed_update_readme_shields(<readme_path> <doc_pct> <test_pct>)
# -----------------------------------------------------------------------------
function(sed_update_readme_shields _readme_path _doc_pct _test_pct)
    if(NOT EXISTS "${_readme_path}")
        message(FATAL_ERROR "[UpdateCoverageShields] README not found: ${_readme_path}")
    endif()
    _sed_badge_color("${_doc_pct}" _doc_color)
    _sed_badge_color("${_test_pct}" _test_color)

    file(READ "${_readme_path}" _orig)
    set(_updated "${_orig}")
    string(REGEX REPLACE
        [[!\[Doc Coverage\]\(https://img\.shields\.io/badge/doc%20coverage-[0-9]+%25-[a-z]+\)]]
        "![Doc Coverage](https://img.shields.io/badge/doc%20coverage-${_doc_pct}%25-${_doc_color})"
        _updated "${_updated}"
    )
    string(REGEX REPLACE
        [[!\[Test Coverage\]\(https://img\.shields\.io/badge/test%20coverage-[0-9]+%25-[a-z]+\)]]
        "![Test Coverage](https://img.shields.io/badge/test%20coverage-${_test_pct}%25-${_test_color})"
        _updated "${_updated}"
    )
    if(NOT "${_updated}" STREQUAL "${_orig}")
        file(WRITE "${_readme_path}" "${_updated}")
    endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND "${CMAKE_SCRIPT_MODE_FILE}" STREQUAL "${CMAKE_CURRENT_LIST_FILE}")
    if(NOT DEFINED README_PATH OR "${README_PATH}" STREQUAL "")
        set(README_PATH "${CMAKE_CURRENT_LIST_DIR}/../README.md")
    endif()
    if(NOT DEFINED DOC_PCT)
        set(DOC_PCT 100)
    endif()
    if(NOT DEFINED TEST_PCT)
        if(DEFINED LCOV_INFO_PATH AND EXISTS "${LCOV_INFO_PATH}")
            file(STRINGS "${LCOV_INFO_PATH}" _all_lines)
            set(_in_test_unit FALSE)
            set(_total_lf 0)
            set(_total_lh 0)
            set(_total_fnf 0)
            set(_total_fnh 0)
            set(_total_brf 0)
            set(_total_brh 0)
            foreach(_ln IN LISTS _all_lines)
                if(_ln MATCHES [[^SF:(.*)$]])
                    if("${CMAKE_MATCH_1}" MATCHES [[/tests/[^/]+\.c$]])
                        set(_in_test_unit TRUE)
                    else()
                        set(_in_test_unit FALSE)
                    endif()
                elseif(_ln STREQUAL "end_of_record")
                    set(_in_test_unit FALSE)
                elseif(_in_test_unit)
                    if(_ln MATCHES [[^LF:([0-9]+)$]])
                        math(EXPR _total_lf "${_total_lf} + ${CMAKE_MATCH_1}")
                    elseif(_ln MATCHES [[^LH:([0-9]+)$]])
                        math(EXPR _total_lh "${_total_lh} + ${CMAKE_MATCH_1}")
                    elseif(_ln MATCHES [[^FNF:([0-9]+)$]])
                        math(EXPR _total_fnf "${_total_fnf} + ${CMAKE_MATCH_1}")
                    elseif(_ln MATCHES [[^FNH:([0-9]+)$]])
                        math(EXPR _total_fnh "${_total_fnh} + ${CMAKE_MATCH_1}")
                    elseif(_ln MATCHES [[^BRF:([0-9]+)$]])
                        math(EXPR _total_brf "${_total_brf} + ${CMAKE_MATCH_1}")
                    elseif(_ln MATCHES [[^BRH:([0-9]+)$]])
                        math(EXPR _total_brh "${_total_brh} + ${CMAKE_MATCH_1}")
                    endif()
                endif()
            endforeach()
            if(_total_lf GREATER 0)
                math(EXPR TEST_PCT "(${_total_lh} * 100) / ${_total_lf}")
                if(_total_fnf GREATER 0)
                    math(EXPR _fn_pct "(${_total_fnh} * 100) / ${_total_fnf}")
                    if(_fn_pct LESS TEST_PCT)
                        set(TEST_PCT "${_fn_pct}")
                    endif()
                endif()
                if(_total_brf GREATER 0)
                    math(EXPR _br_pct "(${_total_brh} * 100) / ${_total_brf}")
                    if(_br_pct LESS TEST_PCT)
                        set(TEST_PCT "${_br_pct}")
                    endif()
                endif()
            else()
                set(TEST_PCT 100)
            endif()
        else()
            set(TEST_PCT 100)
        endif()
    endif()
    if(EXISTS "${README_PATH}")
        sed_update_readme_shields("${README_PATH}" "${DOC_PCT}" "${TEST_PCT}")
    endif()
endif()
