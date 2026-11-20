/**
 * @file sed_test_harness.c
 * @brief Strict ISO C89 implementation of error diagnostics, process execution,
 * binary file I/O, and FIPS 180-4 SHA-256 hashing.
 */

#include "sed_test_harness.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <process.h>
#include <windows.h>
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
#ifndef _WCRTLINK
/** @brief OpenWatcom CRT linkage qualifier fallback. */
#define _WCRTLINK
#endif
/** @brief Synchronous wait mode flag for OpenWatcom _spawnv. */
#define SED_DOS_P_WAIT 0
_WCRTLINK extern int _dup(int handle);
_WCRTLINK extern int _dup2(int handle1, int handle2);
_WCRTLINK extern int _close(int handle);
#ifndef _fileno
_WCRTLINK extern int _fileno(FILE *stream);
#endif
_WCRTLINK extern int _putenv(const char *env_string);
_WCRTLINK extern int spawnv(int mode, const char *path,
                            const char *const *argv);
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
/** @brief Host POSIX process environment vector. */
extern char **environ;
#endif

/** @brief ASCII NUL byte constant (0x00). */
#define SED_CH_NUL ((char)0)
/** @brief ASCII LF newline byte constant (0x0A). */
#define SED_CH_LF ((char)10)
/** @brief ASCII CR carriage-return byte constant (0x0D). */
#define SED_CH_CR ((char)13)
/** @brief ASCII backslash byte constant (0x5C). */
#define SED_CH_BS ((char)92)

#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__) ||               \
    defined(__WATCOMC__)
/** @brief Active path to the compiled sed executable under test. */
static char g_sed_bin_path[512] = "sed.exe";
#else
/** @brief Active path to the compiled sed executable under test. */
static char g_sed_bin_path[512] = "./sed";
#endif

/** @brief Monotonic counter for 8.3-safe scratch file names. */
static unsigned long g_sed_tmp_counter = 0UL;
/** @brief Optional in-process libsed_testable execution callback. */
static sed_inproc_runner_fn g_sed_inproc_runner = NULL;

/**
 * @brief Converts a sed_test_err enumerator to a static human-readable name.
 * @param[in] err Error code to describe.
 * @return Non-NULL constant string naming the error code.
 */
const char *sed_test_err_name(enum sed_test_err err) {
  switch (err) {
  case SED_TEST_OK:
    return "SED_TEST_OK";
  case SED_TEST_ERR_INVALID_ARG:
    return "SED_TEST_ERR_INVALID_ARG";
  case SED_TEST_ERR_ALLOC:
    return "SED_TEST_ERR_ALLOC";
  case SED_TEST_ERR_IO:
    return "SED_TEST_ERR_IO";
  case SED_TEST_ERR_SPAWN:
    return "SED_TEST_ERR_SPAWN";
  case SED_TEST_ERR_EXIT_MISMATCH:
    return "SED_TEST_ERR_EXIT_MISMATCH";
  case SED_TEST_ERR_STDOUT_MISMATCH:
    return "SED_TEST_ERR_STDOUT_MISMATCH";
  case SED_TEST_ERR_STDERR_MISMATCH:
    return "SED_TEST_ERR_STDERR_MISMATCH";
  case SED_TEST_ERR_FILE_MISMATCH:
    return "SED_TEST_ERR_FILE_MISMATCH";
  case SED_TEST_ERR_SHA256_MISMATCH:
    return "SED_TEST_ERR_SHA256_MISMATCH";
  case SED_TEST_ERR_HEADER_CONTRACT:
    return "SED_TEST_ERR_HEADER_CONTRACT";
  default:
    return "SED_TEST_ERR_UNKNOWN";
  }
}

/**
 * @brief Populates a diagnostic record with code, location, and message text.
 * @param[out] diag Destination diagnostic structure (ignored if NULL).
 * @param[in] code Error code to store.
 * @param[in] file Source filename string.
 * @param[in] line 1-based source line number.
 * @param[in] msg Optional message text (may be NULL).
 */
void sed_test_diag_set(struct sed_test_diag *diag, enum sed_test_err code,
                       const char *file, int line, const char *msg) {
  if (diag == NULL) {
    return;
  }
  diag->code = code;
  diag->file = file ? file : "<unknown>";
  diag->line = line;
  if (msg != NULL) {
    size_t len = strlen(msg);
    if (len >= sizeof(diag->detail)) {
      len = sizeof(diag->detail) - 1U;
    }
    memcpy(diag->detail, msg, len);
    diag->detail[len] = SED_CH_NUL;
  } else {
    diag->detail[0] = SED_CH_NUL;
  }
}

/**
 * @brief Configures the path to the sed binary under test (from argv[1]).
 * @param[in] path Filesystem path to sed / sed.exe (may be NULL).
 */
void sed_test_set_binary_path(const char *path) {
  size_t len;
  if (path == NULL || path[0] == SED_CH_NUL) {
    return;
  }
  len = strlen(path);
  if (len >= sizeof(g_sed_bin_path)) {
    len = sizeof(g_sed_bin_path) - 1U;
  }
  memcpy(g_sed_bin_path, path, len);
  g_sed_bin_path[len] = SED_CH_NUL;
}

/**
 * @brief Returns the configured sed binary path (never NULL).
 * @return Path string to sed executable.
 */
const char *sed_test_get_binary_path(void) { return g_sed_bin_path; }

/**
 * @brief Registers or clears an optional in-process sed runner callback.
 * @param[in] runner Callback used by sed_test_run_env (NULL restores spawn).
 */
void sed_test_set_inproc_runner(sed_inproc_runner_fn runner) {
  g_sed_inproc_runner = runner;
}

/**
 * @brief Normalizes CRLF ("\r\n") sequences to LF ("\n") in-place.
 * @param[in,out] buf Mutable byte buffer (may be NULL if len == 0).
 * @param[in] len Byte length of buf prior to normalization.
 * @return New byte length after collapsing "\r\n" pairs to "\n".
 */
size_t sed_test_normalize_crlf(char *buf, size_t len) {
  size_t r;
  size_t w = 0U;
  if (buf == NULL || len == 0U) {
    return 0U;
  }
  for (r = 0U; r < len; ++r) {
    if (buf[r] == SED_CH_CR && (r + 1U) < len && buf[r + 1U] == SED_CH_LF) {
      continue;
    }
    buf[w++] = buf[r];
  }
  buf[w] = SED_CH_NUL;
  return w;
}

/**
 * @brief Releases heap memory held by a sed_cmd_capture structure.
 * @param[in,out] cap Capture object to reset (ignored if NULL).
 */
void sed_cmd_capture_free(struct sed_cmd_capture *cap) {
  if (cap == NULL) {
    return;
  }
  if (cap->out_buf != NULL) {
    free(cap->out_buf);
    cap->out_buf = NULL;
  }
  if (cap->err_buf != NULL) {
    free(cap->err_buf);
    cap->err_buf = NULL;
  }
  cap->out_len = 0U;
  cap->err_len = 0U;
  cap->exit_code = 0;
  cap->err = SED_TEST_OK;
}

/**
 * @brief Writes exact binary bytes to a file on disk.
 * @param[in] path Destination file path.
 * @param[in] data Source byte buffer (may be NULL when len == 0).
 * @param[in] len Number of bytes to write.
 * @return SED_TEST_OK on success, error code otherwise.
 */
enum sed_test_err sed_test_write_file(const char *path, const char *data,
                                      size_t len) {
  FILE *fp;
  if (path == NULL || (len > 0U && data == NULL)) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    return SED_TEST_ERR_IO;
  }
  if (len > 0U && fwrite(data, 1U, len, fp) != len) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    fclose(fp);
    return SED_TEST_ERR_IO;
    /* LCOV_EXCL_STOP */
  }
  if (fclose(fp) != 0) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    return SED_TEST_ERR_IO;
    /* LCOV_EXCL_STOP */
  }
  return SED_TEST_OK;
}

/**
 * @brief Reads an entire file into a NUL-terminated binary heap buffer.
 * @param[in] path Source file path.
 * @param[out] out_buf Receives malloc'd buffer (caller frees with free()).
 * @param[out] out_len Receives exact byte length read.
 * @return SED_TEST_OK on success, error code otherwise.
 */
enum sed_test_err sed_test_read_file(const char *path, char **out_buf,
                                     size_t *out_len) {
  FILE *fp;
  size_t cap = 256U;
  size_t len = 0U;
  char *buf;

  if (path == NULL || out_buf == NULL || out_len == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  *out_buf = NULL;
  *out_len = 0U;

  fp = fopen(path, "rb");
  if (fp == NULL) {
    return SED_TEST_ERR_IO;
  }
  buf = (char *)malloc(cap);
  if (buf == NULL) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    fclose(fp);
    return SED_TEST_ERR_ALLOC;
    /* LCOV_EXCL_STOP */
  }

  for (;;) {
    size_t chunk;
    if (len + 128U >= cap) {
      char *nbuf;
      cap *= 2U;
      nbuf = (char *)realloc(buf, cap);
      if (nbuf == NULL) { /* LCOV_EXCL_BR_LINE */
        /* LCOV_EXCL_START */
        free(buf);
        fclose(fp);
        return SED_TEST_ERR_ALLOC;
        /* LCOV_EXCL_STOP */
      }
      buf = nbuf;
    }
    chunk = fread(buf + len, 1U, cap - len - 1U, fp);
    len += chunk;
    if (chunk == 0U) {
      if (ferror(fp)) { /* LCOV_EXCL_BR_LINE */
        /* LCOV_EXCL_START */
        free(buf);
        fclose(fp);
        return SED_TEST_ERR_IO;
        /* LCOV_EXCL_STOP */
      }
      break;
    }
  }
  fclose(fp);
  buf[len] = SED_CH_NUL;
  *out_buf = buf;
  *out_len = len;
  return SED_TEST_OK;
}

/**
 * @brief Generates strict 8.3-compatible temporary filenames for I/O capture.
 * @param[out] in_name 16-byte buffer for stdin temp file.
 * @param[out] out_name 16-byte buffer for stdout temp file.
 * @param[out] err_name 16-byte buffer for stderr temp file.
 */
static void sed_next_temp_names(char in_name[16], char out_name[16],
                                char err_name[16]) {
  unsigned long seq = (++g_sed_tmp_counter) % 10000UL;
  sprintf(in_name, "STI%04lu.TMP", seq);
  sprintf(out_name, "STO%04lu.TMP", seq);
  sprintf(err_name, "STE%04lu.TMP", seq);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
/**
 * @brief Appends one Windows CreateProcess command-line quoted argument.
 * @param[in,out] dst Destination command line buffer.
 * @param[in] cap Total capacity of dst.
 * @param[in] arg Argument string to quote and append.
 * @return 0 on success, -1 on buffer overflow.
 */
static int sed_win32_append_arg(char *dst, size_t cap, const char *arg) {
  size_t pos = strlen(dst);
  const char *p;
  if (pos + 4U >= cap) {
    return -1;
  }
  if (pos > 0U) {
    dst[pos++] = ' ';
  }
  dst[pos++] = '"';
  for (p = arg; *p != SED_CH_NUL; ++p) {
    unsigned int bs = 0U;
    while (*p == SED_CH_BS) {
      ++bs;
      ++p;
    }
    if (*p == SED_CH_NUL) {
      unsigned int k;
      for (k = 0U; k < bs * 2U; ++k) {
        if (pos + 2U >= cap) {
          return -1;
        }
        dst[pos++] = SED_CH_BS;
      }
      break;
    } else if (*p == '"') {
      unsigned int k;
      for (k = 0U; k < bs * 2U + 1U; ++k) {
        if (pos + 2U >= cap) {
          return -1;
        }
        dst[pos++] = SED_CH_BS;
      }
      dst[pos++] = '"';
    } else {
      unsigned int k;
      for (k = 0U; k < bs; ++k) {
        if (pos + 2U >= cap) {
          return -1;
        }
        dst[pos++] = SED_CH_BS;
      }
      if (pos + 2U >= cap) {
        return -1;
      }
      dst[pos++] = *p;
    }
  }
  dst[pos++] = '"';
  dst[pos] = SED_CH_NUL;
  return 0;
}
#endif

/**
 * @brief Spawns sed with optional environment variable override KEY=VALUE.
 * @param[in] env_kv Optional "KEY=VALUE" environment entry (or NULL).
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] stdin_data Raw bytes fed to child stdin (NULL if stdin_len == 0).
 * @param[in] stdin_len Number of bytes in stdin_data.
 * @param[out] cap Populated with captured stdout, stderr, and exit_code.
 * @return SED_TEST_OK if child ran and outputs were collected.
 */
enum sed_test_err sed_test_run_env(const char *env_kv, const char *const *args,
                                   const char *stdin_data, size_t stdin_len,
                                   struct sed_cmd_capture *cap) {
  char in_path[16];
  char out_path[16];
  char err_path[16];
  enum sed_test_err rc;

  if (args == NULL || cap == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  memset(cap, 0, sizeof(*cap));
  sed_next_temp_names(in_path, out_path, err_path);

  rc = sed_test_write_file(in_path, stdin_data ? stdin_data : "", stdin_len);
  if (rc != SED_TEST_OK) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    cap->err = rc;
    return rc;
    /* LCOV_EXCL_STOP */
  }

  if (g_sed_inproc_runner != NULL) {
    rc = g_sed_inproc_runner(env_kv, args, in_path, out_path, err_path,
                             &cap->exit_code);
    if (rc != SED_TEST_OK) {
      (void)remove(in_path);
      (void)remove(out_path);
      (void)remove(err_path);
      cap->err = rc;
      return rc;
    }
  } else {
#if defined(_WIN32) && !defined(__CYGWIN__)
    {
      SECURITY_ATTRIBUTES sa;
      STARTUPINFOA si;
      PROCESS_INFORMATION pi;
      HANDLE h_in;
      HANDLE h_out;
      HANDLE h_err;
      DWORD exit_dw = 1U;
      char cmdline[8192];
      size_t i;

      cmdline[0] = SED_CH_NUL;
      if (sed_win32_append_arg(cmdline, sizeof(cmdline), g_sed_bin_path) != 0) {
        (void)remove(in_path);
        cap->err = SED_TEST_ERR_INVALID_ARG;
        return cap->err;
      }
      for (i = 0U; args[i] != NULL; ++i) {
        if (sed_win32_append_arg(cmdline, sizeof(cmdline), args[i]) != 0) {
          (void)remove(in_path);
          cap->err = SED_TEST_ERR_INVALID_ARG;
          return cap->err;
        }
      }

      memset(&sa, 0, sizeof(sa));
      sa.nLength = sizeof(sa);
      sa.bInheritHandle = TRUE;

      h_in = CreateFileA(in_path, GENERIC_READ, FILE_SHARE_READ, &sa,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      h_out = CreateFileA(out_path, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      h_err = CreateFileA(err_path, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

      if (h_in == INVALID_HANDLE_VALUE || h_out == INVALID_HANDLE_VALUE ||
          h_err == INVALID_HANDLE_VALUE) {
        if (h_in != INVALID_HANDLE_VALUE) {
          CloseHandle(h_in);
        }
        if (h_out != INVALID_HANDLE_VALUE) {
          CloseHandle(h_out);
        }
        if (h_err != INVALID_HANDLE_VALUE) {
          CloseHandle(h_err);
        }
        (void)remove(in_path);
        cap->err = SED_TEST_ERR_IO;
        return cap->err;
      }

      if (env_kv != NULL) {
        _putenv(env_kv);
      }

      memset(&si, 0, sizeof(si));
      si.cb = sizeof(si);
      si.dwFlags = STARTF_USESTDHANDLES;
      si.hStdInput = h_in;
      si.hStdOutput = h_out;
      si.hStdError = h_err;
      memset(&pi, 0, sizeof(pi));

      if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                          &pi)) {
        if (env_kv != NULL) {
          const char *eq = strchr(env_kv, '=');
          char env_clr[128];
          if (eq != NULL && (size_t)(eq - env_kv) + 2U < sizeof(env_clr)) {
            size_t nlen = (size_t)(eq - env_kv);
            memcpy(env_clr, env_kv, nlen);
            env_clr[nlen] = '=';
            env_clr[nlen + 1U] = (char)0;
            (void)_putenv(env_clr);
          }
        }
        CloseHandle(h_in);
        CloseHandle(h_out);
        CloseHandle(h_err);
        (void)remove(in_path);
        (void)remove(out_path);
        (void)remove(err_path);
        cap->err = SED_TEST_ERR_SPAWN;
        return cap->err;
      }

      WaitForSingleObject(pi.hProcess, INFINITE);
      GetExitCodeProcess(pi.hProcess, &exit_dw);
      if (env_kv != NULL) {
        const char *eq = strchr(env_kv, '=');
        char env_clr[128];
        if (eq != NULL && (size_t)(eq - env_kv) + 2U < sizeof(env_clr)) {
          size_t nlen = (size_t)(eq - env_kv);
          memcpy(env_clr, env_kv, nlen);
          env_clr[nlen] = '=';
          env_clr[nlen + 1U] = (char)0;
          (void)_putenv(env_clr);
        }
      }
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      CloseHandle(h_in);
      CloseHandle(h_out);
      CloseHandle(h_err);
      cap->exit_code = (int)exit_dw;
    }
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
    {
      int saved_in = _dup(0);
      int saved_out = _dup(1);
      int saved_err = _dup(2);
      FILE *fi = fopen(in_path, "rb");
      FILE *fo = fopen(out_path, "wb");
      FILE *fe = fopen(err_path, "wb");
      const char *spawn_argv[64];
      size_t i;
      int sp_rc;

      if (fi == NULL || fo == NULL || fe == NULL) {
        if (fi) {
          fclose(fi);
        }
        if (fo) {
          fclose(fo);
        }
        if (fe) {
          fclose(fe);
        }
        (void)remove(in_path);
        cap->err = SED_TEST_ERR_IO;
        return cap->err;
      }
      if (env_kv != NULL) {
        (void)_putenv(env_kv);
      }
      (void)_dup2(_fileno(fi), 0);
      (void)_dup2(_fileno(fo), 1);
      (void)_dup2(_fileno(fe), 2);

      spawn_argv[0] = g_sed_bin_path;
      for (i = 0U; args[i] != NULL && i < 62U; ++i) {
        spawn_argv[i + 1U] = args[i];
      }
      spawn_argv[i + 1U] = NULL;
      sp_rc = spawnv(SED_DOS_P_WAIT, g_sed_bin_path, spawn_argv);

      fflush(stdout);
      fflush(stderr);
      (void)_dup2(saved_in, 0);
      (void)_dup2(saved_out, 1);
      (void)_dup2(saved_err, 2);
      (void)_close(saved_in);
      (void)_close(saved_out);
      (void)_close(saved_err);
      fclose(fi);
      fclose(fo);
      fclose(fe);

      if (sp_rc < 0) {
        (void)remove(in_path);
        (void)remove(out_path);
        (void)remove(err_path);
        cap->err = SED_TEST_ERR_SPAWN;
        return cap->err;
      }
      cap->exit_code = sp_rc;
    }
#else
    {
      pid_t pid;
      int status = 0;

      pid = fork();
      if (pid < 0) { /* LCOV_EXCL_BR_LINE */
        /* LCOV_EXCL_START */
        (void)remove(in_path);
        cap->err = SED_TEST_ERR_SPAWN;
        return cap->err;
        /* LCOV_EXCL_STOP */
      }
      /* LCOV_EXCL_START */
      if (pid == 0) {
        int fd_in = open(in_path, O_RDONLY);
        int fd_out = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int fd_err = open(err_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        char *child_argv[128];
        size_t i;

        if (fd_in < 0 || fd_out < 0 || fd_err < 0) {
          _exit(127);
        }
        (void)dup2(fd_in, 0);
        (void)dup2(fd_out, 1);
        (void)dup2(fd_err, 2);
        close(fd_in);
        close(fd_out);
        close(fd_err);

        child_argv[0] = g_sed_bin_path;
        for (i = 0U; args[i] != NULL && i < 126U; ++i) {
          size_t alen = strlen(args[i]) + 1U;
          child_argv[i + 1U] = (char *)malloc(alen);
          if (child_argv[i + 1U] == NULL) {
            _exit(127);
          }
          memcpy(child_argv[i + 1U], args[i], alen);
        }
        child_argv[i + 1U] = NULL;

        if (env_kv != NULL) {
          size_t env_count = 0U;
          size_t j;
          size_t eq_len = 0U;
          const char *eq = strchr(env_kv, '=');
          char **new_env;
          if (eq != NULL) {
            eq_len = (size_t)(eq - env_kv) + 1U;
          }
          while (environ != NULL && environ[env_count] != NULL) {
            ++env_count;
          }
          new_env = (char **)malloc((env_count + 2U) * sizeof(char *));
          if (new_env != NULL) {
            size_t out_i = 0U;
            for (j = 0U; j < env_count; ++j) {
              if (eq_len > 0U && strncmp(environ[j], env_kv, eq_len) == 0) {
                continue;
              }
              new_env[out_i++] = environ[j];
            }
            new_env[out_i] = (char *)malloc(strlen(env_kv) + 1U);
            if (new_env[out_i] != NULL) {
              strcpy(new_env[out_i], env_kv);
              ++out_i;
            }
            new_env[out_i] = NULL;
            execve(g_sed_bin_path, child_argv, new_env);
          }
        }
        execv(g_sed_bin_path, child_argv);
        _exit(127);
      }
      /* LCOV_EXCL_STOP */

      if (waitpid(pid, &status, 0) < 0) { /* LCOV_EXCL_BR_LINE */
        /* LCOV_EXCL_START */
        (void)remove(in_path);
        (void)remove(out_path);
        (void)remove(err_path);
        cap->err = SED_TEST_ERR_SPAWN;
        return cap->err;
        /* LCOV_EXCL_STOP */
      }
      if (WIFEXITED(status)) { /* LCOV_EXCL_BR_LINE */
        cap->exit_code = WEXITSTATUS(status);
      } else {
        /* LCOV_EXCL_START */
        cap->exit_code = 1;
        /* LCOV_EXCL_STOP */
      }
    }
#endif
  }

  (void)remove(in_path);
  rc = sed_test_read_file(out_path, &cap->out_buf, &cap->out_len);
  (void)remove(out_path);
  if (rc != SED_TEST_OK) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    (void)remove(err_path);
    cap->err = rc;
    return rc;
    /* LCOV_EXCL_STOP */
  }
  rc = sed_test_read_file(err_path, &cap->err_buf, &cap->err_len);
  (void)remove(err_path);
  if (rc != SED_TEST_OK) { /* LCOV_EXCL_BR_LINE */
    /* LCOV_EXCL_START */
    sed_cmd_capture_free(cap);
    cap->err = rc;
    return rc;
    /* LCOV_EXCL_STOP */
  }
  cap->err = SED_TEST_OK;
  return SED_TEST_OK;
}

/**
 * @brief Spawns sed with an explicit NULL-terminated argument list and stdin.
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] stdin_data Raw bytes fed to child stdin (NULL if stdin_len == 0).
 * @param[in] stdin_len Number of bytes in stdin_data.
 * @param[out] cap Populated with captured stdout, stderr, and exit_code.
 * @return SED_TEST_OK if child ran and outputs were collected.
 */
enum sed_test_err sed_test_run(const char *const *args, const char *stdin_data,
                               size_t stdin_len, struct sed_cmd_capture *cap) {
  return sed_test_run_env(NULL, args, stdin_data, stdin_len, cap);
}

/**
 * @brief Runs sed on NUL-terminated strings and asserts exact exit and stdout.
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] stdin_str Optional NUL-terminated input string (may be NULL).
 * @param[in] expected_out Expected exact stdout string.
 * @param[in] expected_exit Expected process exit status.
 * @return SED_TEST_OK on exact match, specific error code on mismatch.
 */
enum sed_test_err sed_test_expect_simple(const char *const *args,
                                         const char *stdin_str,
                                         const char *expected_out,
                                         int expected_exit) {
  struct sed_cmd_capture cap;
  size_t in_len = stdin_str ? strlen(stdin_str) : 0U;
  size_t exp_len;
  enum sed_test_err rc;

  if (args == NULL || expected_out == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  rc = sed_test_run(args, stdin_str, in_len, &cap);
  if (rc != SED_TEST_OK) {
    return rc;
  }
  if (cap.exit_code != expected_exit) {
    fprintf(stderr,
            "[sed_test_expect_simple] exit expected=%d actual=%d stderr='%s'%c",
            expected_exit, cap.exit_code,
            cap.err_buf ? cap.err_buf : "", /* LCOV_EXCL_BR_LINE */
            SED_CH_LF);
    sed_cmd_capture_free(&cap);
    return SED_TEST_ERR_EXIT_MISMATCH;
  }
#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__) ||               \
    defined(__WATCOMC__)
  do {
    size_t k;
    int is_bin = 0;
    for (k = 0U; args[k] != NULL; ++k) {
      if (strcmp(args[k], "-b") == 0 || strcmp(args[k], "--binary") == 0) {
        is_bin = 1;
        break;
      }
    }
    if (!is_bin && cap.out_buf != NULL) {
      cap.out_len = sed_test_normalize_crlf(cap.out_buf, cap.out_len);
    }
  } while (0);
#endif
  exp_len = strlen(expected_out);
  if (cap.out_len != exp_len ||
      (exp_len > 0U && memcmp(cap.out_buf, expected_out, exp_len) != 0)) {
    fprintf(stderr,
            "[sed_test_expect_simple] stdout mismatch:%c  expected: '%s'%c  "
            "actual:   '%s'%c",
            SED_CH_LF, expected_out, SED_CH_LF,
            cap.out_buf ? cap.out_buf : "", /* LCOV_EXCL_BR_LINE */
            SED_CH_LF);
    sed_cmd_capture_free(&cap);
    return SED_TEST_ERR_STDOUT_MISMATCH;
  }
  sed_cmd_capture_free(&cap);
  return SED_TEST_OK;
}

/**
 * @brief Expands tilde escapes (~n LF, ~r CR, ~t TAB, ~a BEL, ~f FF, ~v VT,
 * ~0 NUL, ~/ backslash, ~~ literal ~) into binary bytes.
 * @param[out] dst Output buffer (NUL-terminated when cap > 0).
 * @param[in] cap Capacity of dst in bytes.
 * @param[in] spec NUL-terminated tilde-encoded specification string.
 * @return Number of decoded bytes written (excluding trailing NUL).
 */
size_t sed_test_unescape(char *dst, size_t cap, const char *spec) {
  size_t w = 0U;
  const char *p;
  if (dst == NULL || cap == 0U) {
    return 0U;
  }
  if (spec == NULL) {
    dst[0] = SED_CH_NUL;
    return 0U;
  }
  for (p = spec; *p != SED_CH_NUL && (w + 1U) < cap; ++p) {
    if (*p == '~' && p[1] != SED_CH_NUL) {
      ++p;
      switch (*p) {
      case 'n':
        dst[w++] = SED_CH_LF;
        break;
      case 'r':
        dst[w++] = SED_CH_CR;
        break;
      case 't':
        dst[w++] = (char)9;
        break;
      case 'a':
        dst[w++] = (char)7;
        break;
      case 'f':
        dst[w++] = (char)12;
        break;
      case 'v':
        dst[w++] = (char)11;
        break;
      case '0':
        dst[w++] = SED_CH_NUL;
        break;
      case '/':
        dst[w++] = SED_CH_BS;
        break;
      case '~':
        dst[w++] = '~';
        break;
      default:
        dst[w++] = *p;
        break;
      }
    } else {
      dst[w++] = *p;
    }
  }
  dst[w] = SED_CH_NUL;
  return w;
}

/**
 * @brief Runs sed with tilde-decoded arguments, input, and expected output.
 * @param[in] args NULL-terminated tilde-encoded argument array AFTER argv[0].
 * @param[in] tilde_in Optional tilde-encoded stdin (NULL for empty stdin).
 * @param[in] tilde_exp_out Tilde-encoded expected stdout bytes.
 * @param[in] expected_exit Expected process exit code.
 * @return SED_TEST_OK on exact match, error code on mismatch.
 */
enum sed_test_err sed_test_expect_tilde(const char *const *args,
                                        const char *tilde_in,
                                        const char *tilde_exp_out,
                                        int expected_exit) {
  char arg_arena[32][1024];
  const char *dec_args[33];
  char in_buf[4096];
  char exp_buf[4096];
  size_t in_len = 0U;
  size_t exp_len = 0U;
  size_t i;
  struct sed_cmd_capture cap;
  enum sed_test_err rc;

  if (args == NULL || tilde_exp_out == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  for (i = 0U; args[i] != NULL && i < 32U; ++i) {
    (void)sed_test_unescape(arg_arena[i], sizeof(arg_arena[i]), args[i]);
    dec_args[i] = arg_arena[i];
  }
  dec_args[i] = NULL;
  if (tilde_in != NULL) {
    in_len = sed_test_unescape(in_buf, sizeof(in_buf), tilde_in);
  }
  exp_len = sed_test_unescape(exp_buf, sizeof(exp_buf), tilde_exp_out);

  rc = sed_test_run(dec_args, tilde_in ? in_buf : NULL, in_len, &cap);
  if (rc != SED_TEST_OK) {
    return rc;
  }
  if (cap.exit_code != expected_exit) {
    fprintf(stderr,
            "[sed_test_expect_tilde] exit expected=%d actual=%d stderr='%s'%c",
            expected_exit, cap.exit_code,
            cap.err_buf ? cap.err_buf : "", /* LCOV_EXCL_BR_LINE */
            SED_CH_LF);
    sed_cmd_capture_free(&cap);
    return SED_TEST_ERR_EXIT_MISMATCH;
  }
#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__) ||               \
    defined(__WATCOMC__)
  do {
    size_t k;
    int is_bin = 0;
    for (k = 0U; dec_args[k] != NULL; ++k) {
      if (strcmp(dec_args[k], "-b") == 0 ||
          strcmp(dec_args[k], "--binary") == 0) {
        is_bin = 1;
        break;
      }
    }
    if (!is_bin && cap.out_buf != NULL) {
      cap.out_len = sed_test_normalize_crlf(cap.out_buf, cap.out_len);
    }
  } while (0);
#endif
  if (cap.out_len != exp_len ||
      (exp_len > 0U && memcmp(cap.out_buf, exp_buf, exp_len) != 0)) {
    fprintf(stderr,
            "[sed_test_expect_tilde] stdout mismatch (exp_len=%lu "
            "act_len=%lu)%c",
            (unsigned long)exp_len, (unsigned long)cap.out_len, SED_CH_LF);
    sed_cmd_capture_free(&cap);
    return SED_TEST_ERR_STDOUT_MISMATCH;
  }
  sed_cmd_capture_free(&cap);
  return SED_TEST_OK;
}

/* -------------------------------------------------------------------------
 * Strict ISO C89 FIPS 180-4 SHA-256 Implementation (32-bit masked unsigned
 * long)
 * ------------------------------------------------------------------------- */

static const unsigned long g_sha256_k[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

/**
 * @brief Right-rotates a 32-bit unsigned value in portable C89.
 * @param[in] x 32-bit value masked to 0xFFFFFFFFUL.
 * @param[in] n Bit shift count (1..31).
 * @return Rotated 32-bit value.
 */
static unsigned long sed_rotr32(unsigned long x, unsigned int n) {
  x &= 0xffffffffUL;
  return ((x >> n) | ((x << (32U - n)) & 0xffffffffUL)) & 0xffffffffUL;
}

/**
 * @brief Compresses a single 64-byte SHA-256 message block.
 * @param[in,out] state Eight 32-bit chaining words.
 * @param[in] block 64-byte input block.
 */
static void sed_sha256_block(unsigned long state[8],
                             const unsigned char block[64]) {
  unsigned long w[64];
  unsigned long a, b, c, d, e, f, g, h;
  unsigned int i;

  for (i = 0U; i < 16U; ++i) {
    w[i] = (((unsigned long)block[i * 4U]) << 24) |
           (((unsigned long)block[i * 4U + 1U]) << 16) |
           (((unsigned long)block[i * 4U + 2U]) << 8) |
           ((unsigned long)block[i * 4U + 3U]);
  }
  for (i = 16U; i < 64U; ++i) {
    unsigned long s0 = sed_rotr32(w[i - 15U], 7U) ^
                       sed_rotr32(w[i - 15U], 18U) ^
                       ((w[i - 15U] & 0xffffffffUL) >> 3);
    unsigned long s1 = sed_rotr32(w[i - 2U], 17U) ^ sed_rotr32(w[i - 2U], 19U) ^
                       ((w[i - 2U] & 0xffffffffUL) >> 10);
    w[i] = (w[i - 16U] + s0 + w[i - 7U] + s1) & 0xffffffffUL;
  }

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  for (i = 0U; i < 64U; ++i) {
    unsigned long S1 =
        sed_rotr32(e, 6U) ^ sed_rotr32(e, 11U) ^ sed_rotr32(e, 25U);
    unsigned long ch = ((e & f) ^ ((~e & 0xffffffffUL) & g)) & 0xffffffffUL;
    unsigned long temp1 = (h + S1 + ch + g_sha256_k[i] + w[i]) & 0xffffffffUL;
    unsigned long S0 =
        sed_rotr32(a, 2U) ^ sed_rotr32(a, 13U) ^ sed_rotr32(a, 22U);
    unsigned long maj = ((a & b) ^ (a & c) ^ (b & c)) & 0xffffffffUL;
    unsigned long temp2 = (S0 + maj) & 0xffffffffUL;

    h = g;
    g = f;
    f = e;
    e = (d + temp1) & 0xffffffffUL;
    d = c;
    c = b;
    b = a;
    a = (temp1 + temp2) & 0xffffffffUL;
  }

  state[0] = (state[0] + a) & 0xffffffffUL;
  state[1] = (state[1] + b) & 0xffffffffUL;
  state[2] = (state[2] + c) & 0xffffffffUL;
  state[3] = (state[3] + d) & 0xffffffffUL;
  state[4] = (state[4] + e) & 0xffffffffUL;
  state[5] = (state[5] + f) & 0xffffffffUL;
  state[6] = (state[6] + g) & 0xffffffffUL;
  state[7] = (state[7] + h) & 0xffffffffUL;
}

/**
 * @brief Computes lowercase 64-character FIPS 180-4 SHA-256 hex digest.
 * @param[in] data Input byte buffer (may be NULL if len == 0).
 * @param[in] len Byte length of data.
 * @param[out] out_hex Destination buffer of at least 65 bytes.
 * @return SED_TEST_OK on success, SED_TEST_ERR_INVALID_ARG if out_hex is NULL.
 */
enum sed_test_err sed_sha256_mem(const unsigned char *data, size_t len,
                                 char out_hex[65]) {
  static const char hex_digits[] = "0123456789abcdef";
  unsigned long state[8];
  unsigned char tail[128];
  size_t full_len;
  size_t rem;
  size_t pad_len;
  unsigned long bit_hi;
  unsigned long bit_lo;
  unsigned int i;

  if (out_hex == NULL || (len > 0U && data == NULL)) {
    return SED_TEST_ERR_INVALID_ARG;
  }

  state[0] = 0x6a09e667UL;
  state[1] = 0xbb67ae85UL;
  state[2] = 0x3c6ef372UL;
  state[3] = 0xa54ff53aUL;
  state[4] = 0x510e527fUL;
  state[5] = 0x9b05688cUL;
  state[6] = 0x1f83d9abUL;
  state[7] = 0x5be0cd19UL;

  full_len = (len / 64U) * 64U;
  rem = len - full_len;
  if (full_len > 0U) {
    size_t off;
    for (off = 0U; off < full_len; off += 64U) {
      sed_sha256_block(state, data + off);
    }
  }

  memset(tail, 0, sizeof(tail));
  if (rem > 0U) {
    memcpy(tail, data + full_len, rem);
  }
  tail[rem] = 0x80U;
  pad_len = (rem < 56U) ? 64U : 128U;

  bit_hi = (unsigned long)((len >> 29) & 0xffffffffUL);
  bit_lo = ((unsigned long)(len & 0x1fffffffUL)) << 3;
  tail[pad_len - 8U] = (unsigned char)((bit_hi >> 24) & 0xffUL);
  tail[pad_len - 7U] = (unsigned char)((bit_hi >> 16) & 0xffUL);
  tail[pad_len - 6U] = (unsigned char)((bit_hi >> 8) & 0xffUL);
  tail[pad_len - 5U] = (unsigned char)(bit_hi & 0xffUL);
  tail[pad_len - 4U] = (unsigned char)((bit_lo >> 24) & 0xffUL);
  tail[pad_len - 3U] = (unsigned char)((bit_lo >> 16) & 0xffUL);
  tail[pad_len - 2U] = (unsigned char)((bit_lo >> 8) & 0xffUL);
  tail[pad_len - 1U] = (unsigned char)(bit_lo & 0xffUL);

  sed_sha256_block(state, tail);
  if (pad_len == 128U) {
    sed_sha256_block(state, tail + 64U);
  }

  for (i = 0U; i < 8U; ++i) {
    unsigned int b;
    for (b = 0U; b < 4U; ++b) {
      unsigned char byte_val =
          (unsigned char)((state[i] >> ((3U - b) * 8U)) & 0xffUL);
      out_hex[i * 8U + b * 2U] = hex_digits[(byte_val >> 4) & 0x0fU];
      out_hex[i * 8U + b * 2U + 1U] = hex_digits[byte_val & 0x0fU];
    }
  }
  out_hex[64] = SED_CH_NUL;
  return SED_TEST_OK;
}

/**
 * @brief Computes lowercase 64-character SHA-256 hex digest of a disk file.
 * @param[in] path Path to input file.
 * @param[out] out_hex Destination buffer of at least 65 bytes.
 * @return SED_TEST_OK on success, error code on invalid argument or I/O error.
 */
enum sed_test_err sed_sha256_file(const char *path, char out_hex[65]) {
  char *buf = NULL;
  size_t len = 0U;
  enum sed_test_err rc;

  if (path == NULL || out_hex == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }
  rc = sed_test_read_file(path, &buf, &len);
  if (rc != SED_TEST_OK) {
    return rc;
  }
  rc = sed_sha256_mem((const unsigned char *)buf, len, out_hex);
  free(buf);
  return rc;
}

/**
 * @brief Logs a failed assertion and returns 1 for test propagation.
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @param[in] expr Expression text.
 * @return Always 1.
 */
int sed_test_fail(const char *file, int line, const char *expr) {
  fprintf(stderr, "[FAIL] %s:%d: %s%c", file ? file : "?", line,
          expr ? expr : "assertion failed", SED_CH_LF);
  return 1;
}

/**
 * @brief Verifies integer equality and logs diagnostic details on mismatch.
 * @param[in] expected Expected integer value.
 * @param[in] actual Actual integer value.
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @return 0 when equal, 1 on mismatch.
 */
int sed_test_check_int_eq(long expected, long actual, const char *file,
                          int line) {
  if (expected != actual) {
    fprintf(stderr, "[FAIL] %s:%d: expected %ld, got %ld%c", file ? file : "?",
            line, expected, actual, SED_CH_LF);
    return 1;
  }
  return 0;
}

/**
 * @brief Verifies NUL-terminated string equality and logs diagnostics.
 * @param[in] expected Expected string (non-NULL).
 * @param[in] actual Actual string (non-NULL).
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @return 0 when equal, 1 on mismatch.
 */
int sed_test_check_str_eq(const char *expected, const char *actual,
                          const char *file, int line) {
  if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0) {
    fprintf(stderr, "[FAIL] %s:%d:%c  expected: '%s'%c  actual:   '%s'%c",
            file ? file : "?", line, SED_CH_LF, expected ? expected : "(null)",
            SED_CH_LF, actual ? actual : "(null)", SED_CH_LF);
    return 1;
  }
  return 0;
}

/**
 * @brief Executes all entries in a test suite and prints pass/fail summary.
 * @param[in] suite_name Human-readable suite title.
 * @param[in] entries Array of test descriptors.
 * @param[in] count Number of elements in entries.
 * @return 0 when every test passes, 1 otherwise.
 */
int sed_test_run_suite(const char *suite_name,
                       const struct sed_test_entry *entries, size_t count) {
  size_t i;
  size_t failed = 0U;

  if (suite_name == NULL || (count > 0U && entries == NULL)) {
    return 1;
  }
  printf("=== Suite: %s (%lu tests) ===%c", suite_name, (unsigned long)count,
         SED_CH_LF);
  for (i = 0U; i < count; ++i) {
    int rc = entries[i].fn();
    if (rc != 0) {
      fprintf(stderr, "  [FAIL] %s%c", entries[i].name, SED_CH_LF);
      ++failed;
    } else {
      printf("  [PASS] %s%c", entries[i].name, SED_CH_LF);
    }
  }
  printf("=== %s: %lu/%lu passed ===%c", suite_name,
         (unsigned long)(count - failed), (unsigned long)count, SED_CH_LF);
  return failed == 0U ? 0 : 1;
}
