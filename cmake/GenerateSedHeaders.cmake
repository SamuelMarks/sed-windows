#[=======================================================================[.rst:
GenerateSedHeaders
------------------

Deterministic CMake build-time header and manifest generator for GNU ``sed``
``4.10`` across Native Windows (MSVC), 32-bit Protected-Mode MS-DOS
(OpenWatcom v2), and POSIX coverage hosts.

Reads upstream ``sed-4.10`` templates (``config_h.in``, ``configure.ac``,
``.tarball-version``, ``lib/*.in.h``, and ``lib/malloc/*.h``) without modifying
a single byte in ``SED_SOURCE_DIR`` and writes all generated build headers into
``OUT_DIR`` (typically ``${CMAKE_CURRENT_BINARY_DIR}/sed_generated``).

Functions
^^^^^^^^^

.. command:: _sed_write_if_different

  Writes ``_content`` to ``_out_path`` only when the destination file is absent
  or differs byte-for-byte, preserving timestamps across incremental builds::

    _sed_write_if_different(<out_path> <content>)

.. command:: _sed_splice_gnulib_snippets

  Splices upstream ``lib/c++defs.h``, ``lib/arg-nonnull.h``, and
  ``lib/warn-on-use.h`` marker blocks into a gnulib ``.in.h`` template string::

    _sed_splice_gnulib_snippets(<sed_dir> <in_var> <out_var>)

.. command:: _sed_set_cfg_define

  Replaces ``#undef <macro>`` with ``#define <macro> <val>`` inside an in-memory
  ``config_h.in`` buffer variable::

    _sed_set_cfg_define(<var_name> <macro> <val>)

.. command:: sed_extract_upstream_version

  Reads ``.tarball-version`` or ``configure.ac`` from ``SED_DIR`` and stores
  the upstream GNU ``sed`` version string (default ``4.10``) in ``OUT_VAR``::

    sed_extract_upstream_version(<sed_dir> <out_var>)

.. command:: sed_generate_build_files

  Generates ``config.h``, ``version.h``, ``version.c``, ``upstream_manifest.h``
  (SHA-256 table of every upstream ``.c`` and ``.h`` file), ``eloop-threshold.h``,
  and all required gnulib template headers (``stdckdint.h``, ``stdcountof.h``,
  ``obstack.h``, ``error.h``, ``uchar.h``, ``unitypes.h``, ``unicase.h``,
  ``unictype.h``, ``uninorm.h``, ``selinux/*.h``, ``malloc/*.gl.h``, and
  conditional ``alloca.h`` on Windows/DOS) inside ``OUT_DIR``::

    sed_generate_build_files(
        SED_DIR <dir>
        OUT_DIR <dir>
        [TARGET_DOS]
        [TARGET_WIN32]
        [HAVE_FWRITING]
        [NO_HOST_GETPROGNAME]
    )
#]=======================================================================]

cmake_minimum_required(VERSION 3.16)

string(ASCII 10 _SED_GH_NL)
string(ASCII 34 _SED_GH_DQ)
string(ASCII 92 _SED_GH_BS)

# -----------------------------------------------------------------------------
# _sed_write_if_different(<path> <content>)
# -----------------------------------------------------------------------------
function(_sed_write_if_different _out_path _content)
    get_filename_component(_parent "${_out_path}" DIRECTORY)
    if(_parent AND NOT EXISTS "${_parent}")
        file(MAKE_DIRECTORY "${_parent}")
    endif()
    if(EXISTS "${_out_path}")
        file(READ "${_out_path}" _existing)
        if("${_existing}" STREQUAL "${_content}")
            return()
        endif()
    endif()
    file(WRITE "${_out_path}" "${_content}")
endfunction()

# -----------------------------------------------------------------------------
# _sed_splice_gnulib_snippets(<sed_dir> <in_var> <out_var>)
# -----------------------------------------------------------------------------
function(_sed_splice_gnulib_snippets _sed_dir _in_var _out_var)
    set(_txt "${${_in_var}}")
    if(EXISTS "${_sed_dir}/lib/c++defs.h")
        file(READ "${_sed_dir}/lib/c++defs.h" _cxxdefs)
        string(REPLACE
            "/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */"
            "${_cxxdefs}"
            _txt "${_txt}"
        )
    endif()
    if(EXISTS "${_sed_dir}/lib/arg-nonnull.h")
        file(READ "${_sed_dir}/lib/arg-nonnull.h" _argnn)
        string(REPLACE
            "/* The definition of _GL_ARG_NONNULL is copied here.  */"
            "${_argnn}"
            _txt "${_txt}"
        )
    endif()
    if(EXISTS "${_sed_dir}/lib/warn-on-use.h")
        file(READ "${_sed_dir}/lib/warn-on-use.h" _warnuse)
        string(REPLACE
            "/* The definition of _GL_WARN_ON_USE is copied here.  */"
            "${_warnuse}"
            _txt "${_txt}"
        )
    endif()
    set(${_out_var} "${_txt}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# sed_extract_upstream_version(<sed_dir> <out_var>)
# -----------------------------------------------------------------------------
function(sed_extract_upstream_version _sed_dir _out_var)
    set(_ver "4.10")
    if(EXISTS "${_sed_dir}/.tarball-version")
        file(STRINGS "${_sed_dir}/.tarball-version" _lines LIMIT_COUNT 1)
        if(_lines)
            list(GET _lines 0 _cand)
            string(STRIP "${_cand}" _cand)
            if(_cand MATCHES [[^[0-9]+\.[0-9]+]])
                set(_ver "${_cand}")
            endif()
        endif()
    elseif(EXISTS "${_sed_dir}/configure.ac")
        file(READ "${_sed_dir}/configure.ac" _ac)
        if(_ac MATCHES [=[AC_INIT[(][[]GNU sed[]],[
]*[[]([0-9]+[.][0-9]+([.][0-9]+)?)[]]]=])
            set(_ver "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${_out_var} "${_ver}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# _sed_set_cfg_define(<var_name> <macro> <val>)
# -----------------------------------------------------------------------------
function(_sed_set_cfg_define _var_name _macro _val)
    set(_t "${${_var_name}}")
    string(REPLACE
        "${_SED_GH_NL}#undef ${_macro}${_SED_GH_NL}"
        "${_SED_GH_NL}#define ${_macro} ${_val}${_SED_GH_NL}"
        _t "${_t}"
    )
    set(${_var_name} "${_t}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# sed_generate_build_files(SED_DIR <dir> OUT_DIR <dir> [TARGET_DOS] [TARGET_WIN32] [HAVE_FWRITING] [NO_HOST_GETPROGNAME])
# -----------------------------------------------------------------------------
function(sed_generate_build_files)
    set(_options TARGET_DOS TARGET_WIN32 HAVE_FWRITING NO_HOST_GETPROGNAME)
    set(_oneValueArgs SED_DIR OUT_DIR)
    cmake_parse_arguments(_ARG "${_options}" "${_oneValueArgs}" "" ${ARGN})

    if(NOT _ARG_SED_DIR OR NOT EXISTS "${_ARG_SED_DIR}/sed/sed.h")
        message(FATAL_ERROR "[GenerateSedHeaders] Invalid SED_DIR '${_ARG_SED_DIR}': missing sed/sed.h")
    endif()
    if(NOT _ARG_OUT_DIR)
        message(FATAL_ERROR "[GenerateSedHeaders] OUT_DIR is required")
    endif()

    file(MAKE_DIRECTORY "${_ARG_OUT_DIR}")
    file(MAKE_DIRECTORY "${_ARG_OUT_DIR}/selinux")
    file(MAKE_DIRECTORY "${_ARG_OUT_DIR}/malloc")

    sed_extract_upstream_version("${_ARG_SED_DIR}" _sed_ver)

    # 1. Generate Automake BUILT_SOURCES: version.h and version.c
    _sed_write_if_different(
        "${_ARG_OUT_DIR}/version.h"
        "extern char const *Version;${_SED_GH_NL}"
    )
    set(_ver_c "#include <config.h>
char const *Version = ${_SED_GH_DQ}${_sed_ver}${_SED_GH_DQ};
#if defined(SED_TESTABLE_BUILD) && defined(lint)
#include ${_SED_GH_DQ}sed.h${_SED_GH_DQ}
#undef finish_program
void finish_program(struct vector *program);
void finish_program(struct vector *program) {
  struct vector empty_v;
  empty_v.v = NULL;
  empty_v.v_length = 0;
  empty_v.v_allocated = 0;
  _sed_real_finish_program(program != NULL ? program : &empty_v);
}
#endif
#if defined(__WATCOMC__)
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef fread
#undef fwrite
#undef fflush
#undef fclose
#undef ferror
#undef popen
#undef pclose
static FILE *g_sed_dos_echo_fp = NULL;
static unsigned long g_sed_watcom_pre_fds = 0UL;
static char g_sed_w_in_path[32];
static char g_sed_w_out_path[32];
static FILE *g_sed_w_fi = NULL;
static FILE *g_sed_w_fo = NULL;
static FILE *g_sed_w_fe = NULL;
void sed_watcom_prep_inproc_stdio(void) {
  if (_osmajor >= 2) {
    (void)setmode(stdin->_handle, O_BINARY);
    (void)setmode(stdout->_handle, O_BINARY);
    (void)setmode(stderr->_handle, O_BINARY);
  }
  stdin->_cnt = 0;
  stdin->_ungotten = 0;
  stdin->_flag &= ~(_EOF | _SFERR | _ISTTY | _UNGET | _BINARY);
  stdin->_flag |= _READ;
  stdout->_cnt = 0;
  stdout->_flag &= ~(_EOF | _SFERR | _ISTTY | _BINARY);
  stdout->_flag |= _WRITE;
  stderr->_cnt = 0;
  stderr->_flag &= ~(_EOF | _SFERR | _ISTTY | _BINARY);
  stderr->_flag |= _WRITE | _IONBF;
}
static int _sed_w_count_file_cmds(const char *const *args) {
  int cnt = 0;
  size_t i;
  if (args == NULL) {
    return 0;
  }
  for (i = 0U; args[i] != NULL; ++i) {
    const char *p = args[i];
    for (; *p != (char)0; ++p) {
      if ((p[0] == 'w' || p[0] == 'W' || p[0] == 'R') && p[1] == ' ') {
        ++cnt;
      }
    }
  }
  return cnt;
}
int sed_watcom_inproc_begin(const char *const *args, const char *in_path,
                            const char *out_path, const char *err_path,
                            int saved[4]) {
  int idx;
  int reserve_two_files = (_sed_w_count_file_cmds(args) >= 2);
  if (_osmajor >= 2) {
    g_sed_watcom_pre_fds = 0UL;
    for (idx = 5; idx < 29; ++idx) {
      if (lseek(idx, 0L, SEEK_CUR) != -1L) {
        g_sed_watcom_pre_fds |= (1UL << idx);
      }
    }
    g_sed_w_fi = fopen(in_path, ${_SED_GH_DQ}rb${_SED_GH_DQ});
    g_sed_w_fo = fopen(out_path, ${_SED_GH_DQ}wb${_SED_GH_DQ});
    g_sed_w_fe = fopen(err_path, ${_SED_GH_DQ}wb${_SED_GH_DQ});
    if (g_sed_w_fi == NULL || g_sed_w_fo == NULL || g_sed_w_fe == NULL) {
      if (g_sed_w_fi != NULL) {
        fclose(g_sed_w_fi);
        g_sed_w_fi = NULL;
      }
      if (g_sed_w_fo != NULL) {
        fclose(g_sed_w_fo);
        g_sed_w_fo = NULL;
      }
      if (g_sed_w_fe != NULL) {
        fclose(g_sed_w_fe);
        g_sed_w_fe = NULL;
      }
      return -1;
    }
    saved[0] = _dup(0);
    saved[1] = _dup(1);
    saved[2] = _dup(2);
    saved[3] = 0;
    (void)_dup2(_fileno(g_sed_w_fi), 0);
    (void)_dup2(_fileno(g_sed_w_fo), 1);
    (void)_dup2(_fileno(g_sed_w_fe), 2);
    sed_watcom_prep_inproc_stdio();
    return 0;
  }
  {
    int has_input = 0;
    FILE *probe;
    for (idx = 5; idx <= 8; ++idx) {
      (void)_close(idx);
    }
    probe = fopen(in_path, ${_SED_GH_DQ}rb${_SED_GH_DQ});
    if (probe == NULL) {
      return -1;
    }
    if (fgetc(probe) != EOF) {
      has_input = 1;
    }
    fclose(probe);
    if (has_input) {
      if (freopen(in_path, ${_SED_GH_DQ}rb${_SED_GH_DQ}, stdin) == NULL) {
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
        return -1;
      }
    } else {
      (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
    }
    if (has_input && reserve_two_files && args != NULL && args[0] != NULL &&
        strcmp(args[0], ${_SED_GH_DQ}--debug${_SED_GH_DQ}) == 0) {
      FILE *ef = fopen(err_path, ${_SED_GH_DQ}wb${_SED_GH_DQ});
      if (ef == NULL) {
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
        return -1;
      }
      fclose(ef);
      (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
      if (freopen(out_path, ${_SED_GH_DQ}wb${_SED_GH_DQ}, stdout) == NULL) {
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stdout);
        return -1;
      }
    } else {
      if (freopen(err_path, ${_SED_GH_DQ}wb${_SED_GH_DQ}, stderr) == NULL) {
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
        return -1;
      }
      if (has_input && reserve_two_files) {
        FILE *of = fopen(out_path, ${_SED_GH_DQ}wb${_SED_GH_DQ});
        if (of == NULL) {
          (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
          (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
          return -1;
        }
        fclose(of);
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stdout);
      } else if (freopen(out_path, ${_SED_GH_DQ}wb${_SED_GH_DQ}, stdout) == NULL) {
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
        (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stdout);
        return -1;
      }
    }
    stderr->_flag |= _IONBF;
    strncpy(g_sed_w_in_path, in_path, sizeof(g_sed_w_in_path) - 1U);
    g_sed_w_in_path[sizeof(g_sed_w_in_path) - 1U] = (char)0;
    strncpy(g_sed_w_out_path, out_path, sizeof(g_sed_w_out_path) - 1U);
    g_sed_w_out_path[sizeof(g_sed_w_out_path) - 1U] = (char)0;
    saved[0] = 0;
    saved[1] = 1;
    saved[2] = 2;
    saved[3] = 1;
    return 0;
  }
}
void sed_watcom_inproc_end(const int saved[4]) {
  int idx;
  (void)fflush(stdout);
  (void)fflush(stderr);
  if (saved[3] == 0) {
    (void)_dup2(saved[0], 0);
    (void)_dup2(saved[1], 1);
    (void)_dup2(saved[2], 2);
    (void)_close(saved[0]);
    (void)_close(saved[1]);
    (void)_close(saved[2]);
    if (g_sed_w_fi != NULL) {
      fclose(g_sed_w_fi);
      g_sed_w_fi = NULL;
    }
    if (g_sed_w_fo != NULL) {
      fclose(g_sed_w_fo);
      g_sed_w_fo = NULL;
    }
    if (g_sed_w_fe != NULL) {
      fclose(g_sed_w_fe);
      g_sed_w_fe = NULL;
    }
    for (idx = 5; idx < 29; ++idx) {
      if ((g_sed_watcom_pre_fds & (1UL << idx)) == 0UL &&
          lseek(idx, 0L, SEEK_CUR) != -1L) {
        (void)_close(idx);
      }
    }
    return;
  }
  (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}r${_SED_GH_DQ}, stdin);
  (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
  (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stdout);
  stderr->_flag |= _IONBF;
  {
    char sync_buf[64];
    FILE *sync_fp = fopen(g_sed_w_out_path, ${_SED_GH_DQ}rb${_SED_GH_DQ});
    if (sync_fp != NULL) {
      (void)fread(sync_buf, 1U, sizeof(sync_buf), sync_fp);
      fclose(sync_fp);
    }
    if (strncmp(g_sed_w_in_path, ${_SED_GH_DQ}STI${_SED_GH_DQ}, 3) == 0) {
      (void)remove(g_sed_w_in_path);
    }
    sync_fp = fopen(g_sed_w_out_path, ${_SED_GH_DQ}rb${_SED_GH_DQ});
    if (sync_fp != NULL) {
      (void)fread(sync_buf, 1U, sizeof(sync_buf), sync_fp);
      fclose(sync_fp);
    }
  }
}
int sed_watcom_err_begin(const char *err_path, int saved[2]) {
  if (_osmajor >= 2) {
    g_sed_w_fe = fopen(err_path, ${_SED_GH_DQ}wb${_SED_GH_DQ});
    if (g_sed_w_fe == NULL) {
      return -1;
    }
    saved[0] = _dup(2);
    saved[1] = 0;
    (void)_dup2(_fileno(g_sed_w_fe), 2);
    stderr->_cnt = 0;
    stderr->_flag &= ~(_EOF | _SFERR | _ISTTY | _BINARY);
    stderr->_flag |= _WRITE | _IONBF;
    return 0;
  }
  if (freopen(err_path, ${_SED_GH_DQ}wb${_SED_GH_DQ}, stderr) == NULL) {
    (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
    return -1;
  }
  stderr->_flag |= _IONBF;
  saved[0] = 2;
  saved[1] = 1;
  return 0;
}
void sed_watcom_err_end(const int saved[2]) {
  (void)fflush(stderr);
  if (saved[1] == 0) {
    (void)_dup2(saved[0], 2);
    (void)_close(saved[0]);
    if (g_sed_w_fe != NULL) {
      fclose(g_sed_w_fe);
      g_sed_w_fe = NULL;
    }
    return;
  }
  (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stderr);
  stderr->_flag |= _IONBF;
  (void)freopen(${_SED_GH_DQ}CON${_SED_GH_DQ}, ${_SED_GH_DQ}w${_SED_GH_DQ}, stdout);
}
FILE *sed_watcom_popen(const char *cmd, const char *type) {
  const char *p = cmd;
  if (p == NULL || type == NULL) {
    return NULL;
  }
  while (*p == ' ' || *p == 9) {
    ++p;
  }
  if (type[0] == 'r' && strncmp(p, ${_SED_GH_DQ}echo${_SED_GH_DQ}, 4) == 0 &&
      (p[4] == (char)0 || p[4] == ' ' || p[4] == 9)) {
    FILE *tf = fopen(${_SED_GH_DQ}SEDP0000.TMP${_SED_GH_DQ}, ${_SED_GH_DQ}wb${_SED_GH_DQ});
    if (tf == NULL) {
      return NULL;
    }
    p += 4;
    if (*p == ' ' || *p == 9) {
      ++p;
    }
    while (*p != (char)0) {
      if (*p != 34 && *p != 39) {
        (void)fputc((unsigned char)*p, tf);
      }
      ++p;
    }
    (void)fputc(13, tf);
    (void)fputc(10, tf);
    fclose(tf);
    tf = fopen(${_SED_GH_DQ}SEDP0000.TMP${_SED_GH_DQ}, ${_SED_GH_DQ}rb${_SED_GH_DQ});
    g_sed_dos_echo_fp = tf;
    return tf;
  }
  return popen(cmd, type);
}
int sed_watcom_pclose(FILE *fp) {
  if (fp == NULL) {
    return -1;
  }
  if (fp == g_sed_dos_echo_fp) {
    g_sed_dos_echo_fp = NULL;
    (void)fclose(fp);
    (void)unlink(${_SED_GH_DQ}SEDP0000.TMP${_SED_GH_DQ});
    return 0;
  }
  return pclose(fp);
}
#endif
")
    _sed_write_if_different("${_ARG_OUT_DIR}/version.c" "${_ver_c}")

    # 2. Generate config.h directly from upstream config_h.in
    if(NOT EXISTS "${_ARG_SED_DIR}/config_h.in")
        message(FATAL_ERROR "[GenerateSedHeaders] Missing ${_ARG_SED_DIR}/config_h.in")
    endif()
    file(READ "${_ARG_SED_DIR}/config_h.in" _cfg)

    _sed_set_cfg_define(_cfg "PACKAGE" "${_SED_GH_DQ}sed${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_NAME" "${_SED_GH_DQ}GNU sed${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_TARNAME" "${_SED_GH_DQ}sed${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_VERSION" "${_SED_GH_DQ}${_sed_ver}${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_STRING" "${_SED_GH_DQ}GNU sed ${_sed_ver}${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_BUGREPORT" "${_SED_GH_DQ}bug-sed@gnu.org${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "PACKAGE_URL" "${_SED_GH_DQ}https://www.gnu.org/software/sed/${_SED_GH_DQ}")
    _sed_set_cfg_define(_cfg "VERSION" "${_SED_GH_DQ}${_sed_ver}${_SED_GH_DQ}")

    _sed_set_cfg_define(_cfg "STDC_HEADERS" "1")
    _sed_set_cfg_define(_cfg "AVOID_ANY_THREADS" "1")
    _sed_set_cfg_define(_cfg "GNULIB_NO_VLA" "1")
    _sed_set_cfg_define(_cfg "GNULIB_XALLOC" "1")
    _sed_set_cfg_define(_cfg "GNULIB_XALLOC_DIE" "1")
    _sed_set_cfg_define(_cfg "GNULIB_MKOSTEMP" "1")
    _sed_set_cfg_define(_cfg "GNULIB_MBRTOC32_REGULAR" "1")
    _sed_set_cfg_define(_cfg "GNULIB_REALLOCARRAY" "1")
    _sed_set_cfg_define(_cfg "_REGEX_INCLUDE_LIMITS_H" "1")
    _sed_set_cfg_define(_cfg "_REGEX_LARGE_OFFSETS" "1")
    _sed_set_cfg_define(_cfg "SETLOCALE_NULL_ALL_MTSAFE" "1")
    _sed_set_cfg_define(_cfg "SETLOCALE_NULL_ONE_MTSAFE" "1")

    foreach(_hdr_def
        HAVE_ALLOCA
        HAVE_ALLOCA_H
        HAVE_ERRNO_H
        HAVE_GETCWD
        HAVE_GETOPT_H
        HAVE_INTTYPES_H
        HAVE_ISASCII
        HAVE_ISATTY
        HAVE_ISBLANK
        HAVE_ISWBLANK
        HAVE_ISWCNTRL
        HAVE_ISWCTYPE
        HAVE_LIMITS_H
        HAVE_LOCALE_H
        HAVE_MALLOC_POSIX
        HAVE_MALLOC_PTRDIFF
        HAVE_REALLOC_POSIX
        HAVE_FREE_POSIX
        HAVE_MBRTOWC
        HAVE_MBSINIT
        HAVE_MBTOWC
        HAVE_MKOSTEMP
        HAVE_POPEN
        HAVE_SETLOCALE
        HAVE_STDBOOL_H
        HAVE_STDDEF_H
        HAVE_STDINT_H
        HAVE_STDIO_H
        HAVE_STDLIB_H
        HAVE_STRING_H
        HAVE_STRTOLL
        HAVE_STRTOUL
        HAVE_SYS_STAT_H
        HAVE_SYS_TYPES_H
        HAVE_TIME_H
        HAVE_UNISTD_H
        HAVE_WCHAR_H
        HAVE_WCRTOMB
        HAVE_WCTOB
        HAVE_WCTOMB
        HAVE_WCTYPE_H
        HAVE_WINT_T
        HAVE_DECL_STRDUP
        HAVE_DECL_STRTOLL
        HAVE_DECL_STRTOIMAX
        HAVE_DECL_ISBLANK
    )
        _sed_set_cfg_define(_cfg "${_hdr_def}" "1")
    endforeach()

    if(NOT _ARG_NO_HOST_GETPROGNAME)
        _sed_set_cfg_define(_cfg "HAVE_GETPROGNAME" "1")
    else()
        _sed_set_cfg_define(_cfg "HAVE_DECL_PROGRAM_INVOCATION_NAME" "1")
        _sed_set_cfg_define(_cfg "HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME" "1")
    endif()

    if(_ARG_HAVE_FWRITING)
        _sed_set_cfg_define(_cfg "HAVE_STDIO_EXT_H" "1")
        _sed_set_cfg_define(_cfg "HAVE___FWRITING" "1")
    endif()

    if(_ARG_TARGET_DOS)
        _sed_set_cfg_define(_cfg "HAVE_FOPEN_RT" "1")
        _sed_set_cfg_define(_cfg "RENAME_DEST_EXISTS_BUG" "1")
        _sed_set_cfg_define(_cfg "HAVE_DECL_STRERROR_R" "0")
    elseif(_ARG_TARGET_WIN32)
        _sed_set_cfg_define(_cfg "HAVE_FOPEN_RT" "1")
        _sed_set_cfg_define(_cfg "HAVE_CRTDEFS_H" "1")
        _sed_set_cfg_define(_cfg "HAVE_WINSOCK2_H" "1")
        _sed_set_cfg_define(_cfg "HAVE_SDKDDKVER_H" "1")
        _sed_set_cfg_define(_cfg "HAVE_MSVC_INVALID_PARAMETER_HANDLER" "1")
        _sed_set_cfg_define(_cfg "GNULIB_MSVC_NOTHROW" "1")
        _sed_set_cfg_define(_cfg "RENAME_DEST_EXISTS_BUG" "1")
        _sed_set_cfg_define(_cfg "HAVE_DECL_STRERROR_R" "0")
    else()
        _sed_set_cfg_define(_cfg "HAVE_LANGINFO_H" "1")
        _sed_set_cfg_define(_cfg "HAVE_LANGINFO_CODESET" "1")
        _sed_set_cfg_define(_cfg "HAVE_NL_LANGINFO" "1")
        _sed_set_cfg_define(_cfg "HAVE_LSTAT" "1")
        _sed_set_cfg_define(_cfg "HAVE_READLINK" "1")
        _sed_set_cfg_define(_cfg "HAVE_SYMLINK" "1")
        _sed_set_cfg_define(_cfg "HAVE_FCHMOD" "1")
        _sed_set_cfg_define(_cfg "HAVE_REALPATH" "1")
        _sed_set_cfg_define(_cfg "HAVE_DECL_GETDELIM" "1")
        _sed_set_cfg_define(_cfg "HAVE_DECL_STRERROR_R" "1")
        _sed_set_cfg_define(_cfg "HAVE_STRERROR_R" "1")
    endif()

    # Isolate GNU regex symbols (re_compile_pattern, re_search, regcomp, etc.)
    # so lib/regex.c never collides with auto-win-msvc's posix-regex (pcre2).
    foreach(_re_sym
        re_compile_fastmap
        re_compile_pattern
        re_match
        re_match_2
        re_search
        re_search_2
        re_set_registers
        re_set_syntax
        re_syntax_options
        regcomp
        regerror
        regexec
        regfree
    )
        _sed_set_cfg_define(_cfg "${_re_sym}" "rpl_${_re_sym}")
    endforeach()

    # Comment out remaining #undef directives just like config.status
    string(REGEX REPLACE
        "(^|${_SED_GH_NL})#[ \t]*undef[ \t]+([A-Za-z0-9_]+)"
        [[\1/* #undef \2 */]]
        _cfg "${_cfg}"
    )

    set(_cfg_header
"/* Generated deterministically by cmake/GenerateSedHeaders.cmake for GNU sed ${_sed_ver}. */
#ifndef SED_WINDOWS_CONFIG_H_INCLUDED
#define SED_WINDOWS_CONFIG_H_INCLUDED 1
#define _GL_CONFIG_H_INCLUDED 1
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE 1
#endif
#ifndef LOCALEDIR
#define LOCALEDIR ${_SED_GH_DQ}${_SED_GH_DQ}
#endif
#if defined(__WATCOMC__) || (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)
#define FLEXIBLE_ARRAY_MEMBER 1
#else
#define FLEXIBLE_ARRAY_MEMBER /**/
#endif
#if defined(_MSC_VER) && !defined(restrict)
#define restrict __restrict
#elif defined(__WATCOMC__) && !defined(restrict)
#define restrict
#endif
#if (defined(_MSC_VER) || defined(__WATCOMC__)) && !defined(_Noreturn)
#if defined(_MSC_VER)
#define _Noreturn __declspec(noreturn)
#else
#define _Noreturn
#endif
#endif

${_cfg}

/* Force-bind OS polyfills from ../auto-win-msvc (posix-core) */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 95
#endif
#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif
#ifndef EOVERFLOW
#define EOVERFLOW 75
#endif
#ifndef ELOOP
#define ELOOP 40
#endif
#ifndef EMLINK
#define EMLINK 31
#endif
#ifndef ETXTBSY
#define ETXTBSY 26
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
#ifndef EILSEQ
#define EILSEQ 84
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY 39
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 36
#endif
#ifndef EROFS
#define EROFS 30
#endif
#ifndef ESPIPE
#define ESPIPE 29
#endif
#ifndef EPIPE
#define EPIPE 32
#endif
#ifndef EBUSY
#define EBUSY 16
#endif
#ifndef ENOTDIR
#define ENOTDIR 20
#endif
#ifndef EISDIR
#define EISDIR 21
#endif
#ifndef EFAULT
#define EFAULT 14
#endif

#if (defined(_MSC_VER) || defined(__WATCOMC__)) && !defined(__cplusplus)
#ifndef _Restrict_arr_
#define _Restrict_arr_
#endif
#ifndef max_align_t
typedef union {
  long __sed_align_l;
  double __sed_align_d;
  void *__sed_align_p;
} sed_max_align_t;
#define max_align_t sed_max_align_t
#endif
#endif

#if defined(__WATCOMC__) && !defined(__cplusplus)
#define _SED_SA_1(expr) extern int _sed_static_assert_check[(expr) ? 1 : -1]
#define _SED_SA_2(expr, msg) extern int _sed_static_assert_check[(expr) ? 1 : -1]
#define _SED_SA_PICK(_1, _2, NAME, ...) NAME
#undef static_assert
#define static_assert(...) _SED_SA_PICK(__VA_ARGS__, _SED_SA_2, _SED_SA_1)(__VA_ARGS__)
#undef _Static_assert
#define _Static_assert(expr, msg) _SED_SA_2(expr, msg)
#endif

#if defined(_WIN32) || defined(__WATCOMC__)
int rpl_mbsinit(const mbstate_t *ps);
size_t rpl_mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t rpl_wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
wint_t rpl_btowc(int c);
int rpl_wctob(wint_t wc);
#undef mbsinit
#define mbsinit rpl_mbsinit
#undef mbrtowc
#define mbrtowc rpl_mbrtowc
#undef wcrtomb
#define wcrtomb rpl_wcrtomb
#undef btowc
#define btowc rpl_btowc
#undef wctob
#define wctob rpl_wctob
#endif

#ifndef UCHAR_WIDTH
#define UCHAR_WIDTH CHAR_BIT
#endif
#ifndef UINTPTR_WIDTH
#define UINTPTR_WIDTH ((int)(sizeof(uintptr_t) * CHAR_BIT))
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX PTRDIFF_MAX
#endif
#ifndef __dead2
#define __dead2
#endif

#ifndef _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD
#define _GL_ATTRIBUTE_SPEC_PRINTF_STANDARD __printf__
#endif
#ifndef _GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM
#define _GL_ATTRIBUTE_SPEC_PRINTF_SYSTEM __printf__
#endif
#ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD
#define _GL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(fmt, first)
#endif
#ifndef _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM
#define _GL_ATTRIBUTE_FORMAT_PRINTF_SYSTEM(fmt, first)
#endif
#ifndef _GL_ARG_NONNULL
#define _GL_ARG_NONNULL(params)
#endif
#ifndef gl_unreachable
#define gl_unreachable() abort()
#endif
#ifndef unreachable
#define unreachable() abort()
#endif
#ifndef _GL_DT_NOTDIR
#define _GL_DT_NOTDIR 0x100
#endif
#ifndef IFTODT
#define IFTODT(mode) (S_ISDIR(mode) ? 4 : 8)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

#if defined(_MSC_VER) || defined(__WATCOMC__)
#define _SED_STATIC_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
#define _SED_STATIC_INLINE static inline __attribute__((__unused__))
#else
#define _SED_STATIC_INLINE static
#endif

#ifndef GNULIB_defined_mbszero
_SED_STATIC_INLINE void mbszero(mbstate_t *ps) {
  memset(ps, 0, sizeof(mbstate_t));
}
#define GNULIB_defined_mbszero 1
#endif

_SED_STATIC_INLINE bool memeq(void const *s1, void const *s2, size_t n) {
  return memcmp(s1, s2, n) == 0;
}

_SED_STATIC_INLINE bool streq(char const *s1, char const *s2) {
  return strcmp(s1, s2) == 0;
}

#ifndef strnul
#define strnul(s) ((s) + strlen(s))
#endif

void *mempcpy(void *dest, const void *src, size_t n);
void *memrchr(const void *s, int c, size_t n);
void *rawmemchr(const void *s, int c);
int strverscmp(const char *s1, const char *s2);
const char *getprogname(void);

#include <setlocale_null.h>

int posix_rename(const char *oldpath, const char *newpath);
int posix_mkostemp(char *tmpl, int flags);
int awm_fwriting(FILE *stream);

#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
#undef rename
#define rename(oldpath, newpath) posix_rename((oldpath), (newpath))
#ifndef mkostemp
#define mkostemp(tmpl, flags) posix_mkostemp((tmpl), (flags))
#endif
#endif

#if defined(_MSC_VER)
#include <ctype.h>
#include <locale.h>
_SED_STATIC_INLINE char *_sed_msvc_setlocale(int cat, const char *loc) {
  if (loc != NULL && loc[0] == (char)0) {
    const char *ev = getenv(${_SED_GH_DQ}LC_ALL${_SED_GH_DQ});
    if (ev != NULL && ev[0] != (char)0) {
      if (strstr(ev, ${_SED_GH_DQ}UTF-8${_SED_GH_DQ}) != NULL ||
          strstr(ev, ${_SED_GH_DQ}utf8${_SED_GH_DQ}) != NULL ||
          strstr(ev, ${_SED_GH_DQ}utf-8${_SED_GH_DQ}) != NULL) {
        char *r = setlocale(cat, ${_SED_GH_DQ}.UTF-8${_SED_GH_DQ});
        if (r != NULL) {
          return r;
        }
      } else if (strcmp(ev, ${_SED_GH_DQ}POSIX${_SED_GH_DQ}) == 0) {
        return setlocale(cat, ${_SED_GH_DQ}C${_SED_GH_DQ});
      } else {
        return setlocale(cat, ev);
      }
    }
  }
  return setlocale(cat, loc);
}
#undef isprint
#define isprint(c) ((c) == 9 ? 0 : (isprint)(c))
#undef setlocale
#define setlocale(cat, loc) _sed_msvc_setlocale((cat), (loc))
#ifndef popen
#define popen _popen
#endif
#ifndef pclose
#define pclose _pclose
#endif
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
#endif

#if defined(__WATCOMC__)
#include <io.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(_SFERR) && !defined(_IOERR)
#define _IOERR _SFERR
#endif
#define HAVE___FWRITING 1
#define __fwriting(stream) awm_fwriting((stream))
_SED_STATIC_INLINE void *_sed_watcom_malloc(size_t n) {
  return (malloc)(n != 0U ? n : 1U);
}
_SED_STATIC_INLINE void *_sed_watcom_calloc(size_t n, size_t s) {
  return (calloc)((n != 0U && s != 0U) ? n : 1U,
                  (n != 0U && s != 0U) ? s : 1U);
}
_SED_STATIC_INLINE void *_sed_watcom_realloc(void *p, size_t n) {
  return (realloc)(p, n != 0U ? n : 1U);
}
_SED_STATIC_INLINE FILE *_sed_watcom_fdopen(int fd, const char *mode) {
  if (fd == stdin->_handle && fd >= 0 && mode != NULL) {
    int is_bin = (strchr(mode, 'b') != NULL);
    if (_osmajor >= 2 || stdin->_handle >= 5) {
      (void)setmode(stdin->_handle, O_BINARY);
    }
    stdin->_cnt = 0;
    stdin->_ungotten = 0;
    stdin->_flag &= ~(_EOF | _SFERR | _ISTTY | _UNGET | _BINARY);
    stdin->_flag |= _READ | (is_bin ? _BINARY : 0);
    return stdin;
  }
  if (fd > 0 && mode != NULL && (_osmajor >= 2 || fd >= 5)) {
    (void)setmode(fd, strchr(mode, 'b') != NULL ? O_BINARY : O_TEXT);
  }
  return posix_fdopen(fd, mode);
}
_SED_STATIC_INLINE int _sed_watcom_setmode(int fd, int mode) {
  int rc = (_osmajor >= 2 || fd >= 5) ? setmode(fd, O_BINARY) : 0;
  if (fd == 0 || fd == stdin->_handle) {
    if ((mode & O_BINARY) != 0) {
      stdin->_flag |= _BINARY;
    } else {
      stdin->_flag &= ~_BINARY;
    }
    return 0;
  } else if (fd == 1 || fd == stdout->_handle) {
    if ((mode & O_BINARY) != 0) {
      stdout->_flag |= _BINARY;
    } else {
      stdout->_flag &= ~_BINARY;
    }
    return 0;
  }
  return rc;
}
#undef fstat
_SED_STATIC_INLINE int _sed_watcom_fstat(int fd, struct stat *st) {
  int rc;
  if (fd >= 0 && st != NULL && _osmajor < 2) {
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | S_IREAD | S_IWRITE;
    return 0;
  }
  rc = fstat(fd, st);
  if (rc == 0 && st != NULL) {
    st->st_mode |= S_IWRITE;
  }
  return rc;
}
FILE *sed_watcom_popen(const char *cmd, const char *type);
int sed_watcom_pclose(FILE *fp);
#undef malloc
#define malloc(n) _sed_watcom_malloc((n))
#undef calloc
#define calloc(n, s) _sed_watcom_calloc((n), (s))
#undef realloc
#define realloc(p, n) _sed_watcom_realloc((p), (n))
#undef fdopen
#define fdopen(fd, mode) _sed_watcom_fdopen((fd), (mode))
#undef _setmode
#define _setmode(fd, mode) _sed_watcom_setmode((fd), (mode))
#undef fstat
#define fstat(fd, st) _sed_watcom_fstat((fd), (st))
#undef popen
#define popen(cmd, type) sed_watcom_popen((cmd), (type))
#undef pclose
#define pclose(fp) sed_watcom_pclose((fp))
#ifndef SED_TESTABLE_BUILD
extern const char *program_name;
static char _sed_dos_argv0[] = ${_SED_GH_DQ}sed${_SED_GH_DQ};
#define initialize_main(ac, av)                                                \
  do {                                                                         \
    if (_osmajor < 3 || *(av) == NULL || (*(av))[0] == NULL ||                 \
        (unsigned char)(*(av))[0][0] <= 32U ||                                 \
        (unsigned char)(*(av))[0][0] >= 127U) {                                \
      if (*(av) != NULL) {                                                     \
        (*(av))[0] = _sed_dos_argv0;                                           \
      }                                                                        \
      program_name = _sed_dos_argv0;                                           \
    }                                                                          \
  } while (0)
#endif
#endif

void *rpl_reallocarray(void *ptr, size_t nmemb, size_t size);
#undef reallocarray
#define reallocarray rpl_reallocarray

#if defined(_WIN32) || defined(__WATCOMC__)
ssize_t rpl_getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream);
int rpl_strverscmp(const char *s1, const char *s2);
#undef getdelim
#define getdelim rpl_getdelim
#undef strverscmp
#define strverscmp rpl_strverscmp
#endif

/* Testable build hooks (inactive in production sed binary) */
#ifdef SED_TESTABLE_BUILD
#include <sed_test_hooks.h>
#ifdef lint
#define finish_program _sed_real_finish_program
#ifdef SED_RST_C
static int _sed_obs_live = 0;
#undef obstack_init
#define obstack_init(h) (SED_RST_C(), (_sed_obs_live = 1), obstack_begin((h), 0))
#undef obstack_free
#define obstack_free(h, o)                                                     \
  do {                                                                         \
    if (_sed_obs_live) {                                                       \
      _sed_obs_live = 0;                                                       \
      __obstack_free((h), (o));                                                \
    }                                                                          \
  } while (0)
#endif
#endif
#endif

#endif /* SED_WINDOWS_CONFIG_H_INCLUDED */
")
    _sed_write_if_different("${_ARG_OUT_DIR}/config.h" "${_cfg_header}")

    file(READ "${_ARG_SED_DIR}/lib/eloop-threshold.h" _eloop_h)
    string(APPEND _eloop_h "
#ifdef SED_TESTABLE_BUILD
#undef lint
#endif
")
    _sed_write_if_different("${_ARG_OUT_DIR}/eloop-threshold.h" "${_eloop_h}")

    # 3. Instantiate gnulib .in.h and .gl.h templates into OUT_DIR
    # 3a. stdckdint.h
    file(READ "${_ARG_SED_DIR}/lib/stdckdint.in.h" _stdckd)
    string(REPLACE "@GUARD_PREFIX@" "GL" _stdckd "${_stdckd}")
    string(REPLACE "@PRAGMA_SYSTEM_HEADER@" "" _stdckd "${_stdckd}")
    string(REPLACE "@PRAGMA_COLUMNS@" "" _stdckd "${_stdckd}")
    string(REPLACE "@HAVE_C_STDCKDINT_H@" "0" _stdckd "${_stdckd}")
    string(REPLACE "@HAVE_CXX_STDCKDINT_H@" "0" _stdckd "${_stdckd}")
    string(REPLACE "@HAVE_WORKING_C_STDCKDINT_H@" "0" _stdckd "${_stdckd}")
    string(REPLACE "@HAVE_WORKING_CXX_STDCKDINT_H@" "0" _stdckd "${_stdckd}")
    string(REPLACE "# @INCLUDE_NEXT@ @NEXT_STDCKDINT_H@" "" _stdckd "${_stdckd}")
    _sed_write_if_different("${_ARG_OUT_DIR}/stdckdint.h" "${_stdckd}")

    # 3b. stdcountof.h
    file(READ "${_ARG_SED_DIR}/lib/stdcountof.in.h" _stdcnt)
    string(REPLACE "@GUARD_PREFIX@" "GL" _stdcnt "${_stdcnt}")
    string(REPLACE "@PRAGMA_SYSTEM_HEADER@" "" _stdcnt "${_stdcnt}")
    string(REPLACE "@PRAGMA_COLUMNS@" "" _stdcnt "${_stdcnt}")
    string(REPLACE "@HAVE_STDCOUNTOF_H@" "0" _stdcnt "${_stdcnt}")
    string(REPLACE "@CXX_HAVE_STDCOUNTOF_H@" "0" _stdcnt "${_stdcnt}")
    string(REPLACE "# @INCLUDE_NEXT@ @NEXT_STDCOUNTOF_H@" "" _stdcnt "${_stdcnt}")
    _sed_write_if_different("${_ARG_OUT_DIR}/stdcountof.h" "${_stdcnt}")

    # 3c. obstack.h
    file(READ "${_ARG_SED_DIR}/lib/obstack.in.h" _obst)
    string(REPLACE "defined __GL_GNULIB_HEADER" "1" _obst "${_obst}")
    string(REPLACE "defined __GL_REPLACE_OBSTACK__" "1" _obst "${_obst}")
    string(REPLACE "defined __GL_SMALL_PTRDIFF_T__" "0" _obst "${_obst}")
    _sed_write_if_different("${_ARG_OUT_DIR}/obstack.h" "${_obst}")

    # 3d. error.h
    file(READ "${_ARG_SED_DIR}/lib/error.in.h" _errh)
    string(REPLACE "@GUARD_PREFIX@" "GL" _errh "${_errh}")
    string(REPLACE "@PRAGMA_SYSTEM_HEADER@" "" _errh "${_errh}")
    string(REPLACE "@HAVE_ERROR_H@" "0" _errh "${_errh}")
    string(REPLACE "# @INCLUDE_NEXT@ @NEXT_ERROR_H@" "" _errh "${_errh}")
    string(REPLACE "@HAVE_ERROR@" "0" _errh "${_errh}")
    string(REPLACE "@HAVE_ERROR_AT_LINE@" "0" _errh "${_errh}")
    string(REPLACE "@REPLACE_ERROR@" "0" _errh "${_errh}")
    string(REPLACE "@REPLACE_ERROR_AT_LINE@" "0" _errh "${_errh}")
    _sed_splice_gnulib_snippets("${_ARG_SED_DIR}" _errh _errh)
    _sed_write_if_different("${_ARG_OUT_DIR}/error.h" "${_errh}")

    # 3e. uchar.h
    file(READ "${_ARG_SED_DIR}/lib/uchar.in.h" _uchar)
    string(REPLACE "@GUARD_PREFIX@" "GL" _uchar "${_uchar}")
    string(REPLACE "@PRAGMA_SYSTEM_HEADER@" "" _uchar "${_uchar}")
    string(REPLACE "@PRAGMA_COLUMNS@" "" _uchar "${_uchar}")
    string(REPLACE "@HAVE_UCHAR_H@" "0" _uchar "${_uchar}")
    string(REPLACE "@CXX_HAVE_UCHAR_H@" "0" _uchar "${_uchar}")
    string(REPLACE "# @INCLUDE_NEXT@ @NEXT_UCHAR_H@" "" _uchar "${_uchar}")
    string(REPLACE "@CXX_HAS_CHAR8_TYPE@" "0" _uchar "${_uchar}")
    string(REPLACE "@CXX_HAS_UCHAR_TYPES@" "0" _uchar "${_uchar}")
    if(_ARG_TARGET_DOS OR _ARG_TARGET_WIN32)
        string(REPLACE "@SMALL_WCHAR_T@" "1" _uchar "${_uchar}")
    else()
        string(REPLACE "@SMALL_WCHAR_T@" "0" _uchar "${_uchar}")
    endif()
    string(REPLACE "@GNULIBHEADERS_OVERRIDE_CHAR8_T@" "0" _uchar "${_uchar}")
    string(REPLACE "@GNULIBHEADERS_OVERRIDE_CHAR16_T@" "0" _uchar "${_uchar}")
    string(REPLACE "@GNULIBHEADERS_OVERRIDE_CHAR32_T@" "0" _uchar "${_uchar}")
    string(REPLACE "@HAVE_MBRTOC16@" "0" _uchar "${_uchar}")
    string(REPLACE "@HAVE_MBRTOC32@" "0" _uchar "${_uchar}")
    string(REPLACE "@HAVE_C32RTOMB@" "0" _uchar "${_uchar}")
    string(REPLACE "@REPLACE_MBRTOC16@" "0" _uchar "${_uchar}")
    string(REPLACE "@REPLACE_MBRTOC32@" "1" _uchar "${_uchar}")
    string(REPLACE "@REPLACE_C32RTOMB@" "1" _uchar "${_uchar}")
    foreach(_uc_on
        GNULIB_BTOWC
        GNULIB_C32ISPRINT
        GNULIB_C32ISSPACE
        GNULIB_C32TOLOWER
        GNULIB_C32TOUPPER
        GNULIB_C32RTOMB
        GNULIB_C32TOB
        GNULIB_MBRTOC32
        GNULIB_WCTOB
    )
        string(REPLACE "@${_uc_on}@" "1" _uchar "${_uchar}")
    endforeach()
    string(REGEX REPLACE [[@GNULIB_[A-Z0-9_]+@]] "0" _uchar "${_uchar}")
    _sed_splice_gnulib_snippets("${_ARG_SED_DIR}" _uchar _uchar)
    _sed_write_if_different("${_ARG_OUT_DIR}/uchar.h" "${_uchar}")

    # 3f. unitypes.h, unicase.h, unictype.h, uninorm.h
    foreach(_uni unitypes unicase unictype uninorm)
        file(READ "${_ARG_SED_DIR}/lib/${_uni}.in.h" _utxt)
        string(REPLACE "@HAVE_UNISTRING_WOE32DLL_H@" "0" _utxt "${_utxt}")
        string(REGEX REPLACE [[@GNULIB_UNI[A-Z0-9_]+_DLL_VARIABLE@]] "" _utxt "${_utxt}")
        _sed_write_if_different("${_ARG_OUT_DIR}/${_uni}.h" "${_utxt}")
    endforeach()

    # 3g. selinux/selinux.h, selinux/context.h, selinux/label.h
    file(READ "${_ARG_SED_DIR}/lib/se-selinux.in.h" _sesel)
    string(REPLACE "@GUARD_PREFIX@" "GL" _sesel "${_sesel}")
    string(REPLACE "@PRAGMA_SYSTEM_HEADER@" "" _sesel "${_sesel}")
    string(REPLACE "@PRAGMA_COLUMNS@" "" _sesel "${_sesel}")
    string(REPLACE "@USE_SELINUX_SELINUX_H@" "0" _sesel "${_sesel}")
    string(REPLACE "# @INCLUDE_NEXT@ @NEXT_SELINUX_SELINUX_H@" "" _sesel "${_sesel}")
    _sed_splice_gnulib_snippets("${_ARG_SED_DIR}" _sesel _sesel)
    _sed_write_if_different("${_ARG_OUT_DIR}/selinux/selinux.h" "${_sesel}")

    file(READ "${_ARG_SED_DIR}/lib/se-context.in.h" _sectx)
    _sed_splice_gnulib_snippets("${_ARG_SED_DIR}" _sectx _sectx)
    string(APPEND _sectx "
#if defined(SED_TESTABLE_BUILD) && defined(INITIAL_BUFFER_SIZE)
#undef lint
static bool replaced;
static struct output output_file;
static void release_append_queue(void);
void sed_testable_reset_execute(void);
void sed_testable_reset_execute(void) {
  replaced = false;
  output_file.missing_newline = 0;
  output_file.fp = NULL;
  release_append_queue();
}
#endif
")
    _sed_write_if_different("${_ARG_OUT_DIR}/selinux/context.h" "${_sectx}")

    file(READ "${_ARG_SED_DIR}/lib/se-label.in.h" _selbl)
    _sed_splice_gnulib_snippets("${_ARG_SED_DIR}" _selbl _selbl)
    _sed_write_if_different("${_ARG_OUT_DIR}/selinux/label.h" "${_selbl}")

    # 3h. malloc/dynarray.gl.h, malloc/dynarray-skeleton.gl.h, malloc/scratch_buffer.gl.h
    file(READ "${_ARG_SED_DIR}/lib/malloc/dynarray.h" _dynh)
    string(REGEX REPLACE "[^${_SED_GH_NL}]*libc_hidden_proto[^${_SED_GH_NL}]*${_SED_GH_NL}" "" _dynh "${_dynh}")
    _sed_write_if_different("${_ARG_OUT_DIR}/malloc/dynarray.gl.h" "${_dynh}")

    file(READ "${_ARG_SED_DIR}/lib/malloc/dynarray-skeleton.c" _dyns)
    string(REPLACE "<malloc/dynarray.h>" "<malloc/dynarray.gl.h>" _dyns "${_dyns}")
    string(REPLACE "__attribute_maybe_unused__" "_GL_ATTRIBUTE_MAYBE_UNUSED" _dyns "${_dyns}")
    string(REPLACE "__attribute_nonnull__" "_GL_ATTRIBUTE_NONNULL" _dyns "${_dyns}")
    string(REPLACE "__attribute_warn_unused_result__" "_GL_ATTRIBUTE_NODISCARD" _dyns "${_dyns}")
    string(REPLACE "__glibc_likely" "_GL_LIKELY" _dyns "${_dyns}")
    string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" _dyns "${_dyns}")
    _sed_write_if_different("${_ARG_OUT_DIR}/malloc/dynarray-skeleton.gl.h" "${_dyns}")

    file(READ "${_ARG_SED_DIR}/lib/malloc/scratch_buffer.h" _scrb)
    string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" _scrb "${_scrb}")
    string(REPLACE "__glibc_likely" "_GL_LIKELY" _scrb "${_scrb}")
    string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" _scrb "${_scrb}")
    string(REGEX REPLACE "[^${_SED_GH_NL}]*libc_hidden_proto[^${_SED_GH_NL}]*${_SED_GH_NL}" "" _scrb "${_scrb}")
    _sed_write_if_different("${_ARG_OUT_DIR}/malloc/scratch_buffer.gl.h" "${_scrb}")

    # 3i. alloca.h (when compiling on MSVC/Watcom where <malloc.h> provides alloca)
    if(_ARG_TARGET_DOS OR _ARG_TARGET_WIN32)
        file(READ "${_ARG_SED_DIR}/lib/alloca.in.h" _alloca_h)
        string(REPLACE "@HAVE_ALLOCA_H@" "0" _alloca_h "${_alloca_h}")
        _sed_write_if_different("${_ARG_OUT_DIR}/alloca.h" "${_alloca_h}")
    endif()

    # 4. Generate strict C89 SHA-256 manifest of all upstream .c and .h files
    file(GLOB_RECURSE _up_files
        RELATIVE "${_ARG_SED_DIR}"
        "${_ARG_SED_DIR}/basicdefs.h"
        "${_ARG_SED_DIR}/sed/*.c"
        "${_ARG_SED_DIR}/sed/*.h"
        "${_ARG_SED_DIR}/lib/*.c"
        "${_ARG_SED_DIR}/lib/*.h"
    )
    list(SORT _up_files)

    set(_manifest_entries "")
    foreach(_rel IN LISTS _up_files)
        file(SHA256 "${_ARG_SED_DIR}/${_rel}" _sha)
        string(APPEND _manifest_entries
            "  { ${_SED_GH_DQ}${_rel}${_SED_GH_DQ}, ${_SED_GH_DQ}${_sha}${_SED_GH_DQ} },${_SED_GH_NL}"
        )
    endforeach()
    list(LENGTH _up_files _up_count)

    set(_manifest_h
"/**
 * @file upstream_manifest.h
 * @brief Deterministic SHA-256 immutability table for GNU sed ${_sed_ver}.
 */
#ifndef SED_UPSTREAM_MANIFEST_H
#define SED_UPSTREAM_MANIFEST_H

/** @brief Total count of tracked upstream .c and .h files. */
#define SED_UPSTREAM_FILE_COUNT ${_up_count}U

/** @brief Upstream source directory path resolved at configure time. */
#define SED_UPSTREAM_SOURCE_DIR ${_SED_GH_DQ}${_ARG_SED_DIR}${_SED_GH_DQ}

/** @brief Generated build header directory path resolved at configure time. */
#define SED_GENERATED_DIR ${_SED_GH_DQ}${_ARG_OUT_DIR}${_SED_GH_DQ}

/** @brief Entry mapping a relative upstream path to its SHA-256 hex digest. */
struct sed_upstream_sha256_entry {
  const char *rel_path;   /**< Path relative to SED_UPSTREAM_SOURCE_DIR. */
  const char *sha256_hex; /**< Lowercase 64-char SHA-256 digest. */
};

/** @brief Immutable manifest of every upstream .c and .h file. */
static const struct sed_upstream_sha256_entry g_sed_upstream_manifest[] = {
${_manifest_entries}  { (const char *)0, (const char *)0 }
};

#endif /* SED_UPSTREAM_MANIFEST_H */
")
    _sed_write_if_different("${_ARG_OUT_DIR}/upstream_manifest.h" "${_manifest_h}")
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND "${CMAKE_SCRIPT_MODE_FILE}" STREQUAL "${CMAKE_CURRENT_LIST_FILE}")
    if(DEFINED SED_DIR AND DEFINED OUT_DIR)
        set(_extra "")
        if(TARGET_DOS)
            list(APPEND _extra TARGET_DOS)
        endif()
        if(TARGET_WIN32)
            list(APPEND _extra TARGET_WIN32)
        endif()
        sed_generate_build_files(SED_DIR "${SED_DIR}" OUT_DIR "${OUT_DIR}" ${_extra})
    endif()
endif()
