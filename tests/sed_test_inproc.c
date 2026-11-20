/**
 * @file sed_test_inproc.c
 * @brief Strict ISO C89 implementation of in-process sed_testable_main
 * execution and deterministic fault-injection hooks.
 */

#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <io.h>
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
#ifndef _WCRTLINK
/** @brief OpenWatcom CRT linkage qualifier fallback. */
#define _WCRTLINK
#endif
_WCRTLINK extern int _dup(int handle);
_WCRTLINK extern int _dup2(int handle1, int handle2);
_WCRTLINK extern int _close(int handle);
#ifndef _fileno
_WCRTLINK extern int _fileno(FILE *stream);
#endif
_WCRTLINK extern int _putenv(const char *env_string);
FILE *popen(const char *command, const char *type);
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "sed_test_inproc.h"

/** @brief Opaque compiled sed program vector from sed/sed.h. */
struct vector;
/**
 * @brief Creates a temporary file via auto-win-msvc mkostemp polyfill.
 * @param[in,out] tmpl Mutable template path ending in XXXXXX.
 * @param[in] flags Extra open flags.
 * @return Open file descriptor or -1 on error.
 */
extern int posix_mkostemp(char *tmpl, int flags);
/**
 * @brief Atomically renames a file via auto-win-msvc rename polyfill.
 * @param[in] oldpath Source path.
 * @param[in] newpath Destination path.
 * @return 0 on success, -1 on error.
 */
extern int posix_rename(const char *oldpath, const char *newpath);
/**
 * @brief Closes a tracked sed stream or all streams when NULL.
 * @param[in,out] stream Stream pointer or NULL.
 */
extern void ck_fclose(FILE *stream);
/**
 * @brief Resets static execution state inside libsed_testable.
 */
extern void sed_testable_reset_execute(void);
/**
 * @brief Releases compiled sed program obstacks and open read/write files.
 * @param[in,out] program Optional program vector pointer.
 */
extern void finish_program(struct vector *program);
/**
 * @brief Unlinks any active temporary in-place output file on panic.
 */
extern void remove_cleanup_file(void);
/**
 * @brief Clears active in-place cleanup registration state.
 */
extern void cancel_cleanup(void);
#if defined(_WIN32) || defined(__WATCOMC__)
/**
 * @brief Converts a multibyte sequence via gnulib rpl_mbrtowc.
 * @param[out] pwc Optional wide-character output pointer.
 * @param[in] s Input byte buffer.
 * @param[in] n Maximum input byte count.
 * @param[in,out] ps Conversion shift state.
 * @return Consumed byte count or (size_t)-1 / (size_t)-2.
 */
extern size_t rpl_mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
#endif

/** @brief Global deterministic fault-injection configuration. */
static struct sed_fault_config g_fault;
/** @brief Non-local jump target trapping exit() and panic() in-process. */
static jmp_buf g_exit_jmp;
/** @brief Non-zero while g_exit_jmp is armed to trap sed_hook_exit(). */
static int g_jmp_active = 0;
/** @brief Last exit status captured from sed_testable_main or sed_hook_exit. */
static int g_exit_status = 0;

/**
 * @brief Resets all fault-injection counters and flags to disabled defaults.
 */
void sed_fault_reset(void) { memset(&g_fault, 0, sizeof(g_fault)); }

/**
 * @brief Returns a mutable pointer to the active fault-injection configuration.
 * @return Non-NULL pointer to global sed_fault_config state.
 */
struct sed_fault_config *sed_fault_state(void) { return &g_fault; }

/**
 * @brief Intercepts exit() inside libsed_testable for in-process white-box
 * tests.
 * @param[in] status Process exit status passed to exit().
 */
void sed_hook_exit(int status) {
  g_exit_status = status;
  if (g_jmp_active) { /* LCOV_EXCL_BR_LINE */
    longjmp(g_exit_jmp, status == 0 ? 256 : status);
  }
  /* LCOV_EXCL_START */
  exit(status);
  /* LCOV_EXCL_STOP */
}

/**
 * @brief Intercepts fwrite() inside libsed_testable for write-error injection.
 * @param[in] ptr Source buffer.
 * @param[in] size Element byte size.
 * @param[in] nmemb Element count.
 * @param[in,out] stream Output FILE stream.
 * @return Number of elements written.
 */
size_t sed_hook_fwrite(const void *ptr, size_t size, size_t nmemb,
                       FILE *stream) {
  if (g_fault.fail_fwrite_after > 0) {
    --g_fault.fail_fwrite_after;
    if (g_fault.fail_fwrite_after == 0) {
      errno = ENOSPC;
      return 0U;
    }
  }
  return fwrite(ptr, size, nmemb, stream);
}

/**
 * @brief Intercepts fread() inside libsed_testable for read-error injection.
 * @param[out] ptr Destination buffer.
 * @param[in] size Element byte size.
 * @param[in] nmemb Element count.
 * @param[in,out] stream Input FILE stream.
 * @return Number of elements read.
 */
size_t sed_hook_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (g_fault.fail_fread_after > 0) {
    --g_fault.fail_fread_after;
    if (g_fault.fail_fread_after == 0) {
      g_fault.force_ferror = 1;
      errno = EIO;
      return 0U;
    }
  }
  return fread(ptr, size, nmemb, stream);
}

/**
 * @brief Intercepts fflush() inside libsed_testable for flush-error injection.
 * @param[in,out] stream Output FILE stream.
 * @return 0 on success, EOF on injected or underlying failure.
 */
int sed_hook_fflush(FILE *stream) {
  if (g_fault.fail_fflush_after > 0) {
    --g_fault.fail_fflush_after;
    if (g_fault.fail_fflush_after == 0) {
      errno = g_fault.fail_fflush_errno ? g_fault.fail_fflush_errno : EIO;
      return EOF;
    }
  }
  return fflush(stream);
}

/**
 * @brief Intercepts ferror() inside libsed_testable for stream-error injection.
 * @param[in] stream Inspected FILE stream.
 * @return Non-zero if error indicator is active.
 */
int sed_hook_ferror(FILE *stream) {
  if (g_fault.force_ferror) {
    return 1;
  }
  return ferror(stream);
}

/**
 * @brief Intercepts fclose() inside libsed_testable for close-error injection.
 * @param[in,out] stream Stream to close.
 * @return 0 on success, EOF on injected or underlying failure.
 */
int sed_hook_fclose(FILE *stream) {
  int rc = 0;
  if (stream == stdin || stream == stdout || stream == stderr) {
    rc = fflush(stream);
  } else {
    rc = fclose(stream);
  }
  if (g_fault.fail_fclose_after > 0) {
    --g_fault.fail_fclose_after;
    if (g_fault.fail_fclose_after == 0) {
      errno = EIO;
      return EOF;
    }
  }
  return rc;
}

/**
 * @brief Intercepts mkostemp() inside libsed_testable for temp-file fault
 * injection.
 * @param[in,out] tmpl Mutable template path ending in XXXXXX.
 * @param[in] flags Additional open flags.
 * @return Non-negative file descriptor on success, -1 on failure.
 */
int sed_hook_mkostemp(char *tmpl, int flags) {
  if (g_fault.fail_mkostemp) {
    errno = EACCES;
    return -1;
  }
  return posix_mkostemp(tmpl, flags);
}

/**
 * @brief Intercepts rename() inside libsed_testable for atomic-rename fault
 * injection.
 * @param[in] oldpath Source path.
 * @param[in] newpath Destination path.
 * @return 0 on success, -1 on failure.
 */
int sed_hook_rename(const char *oldpath, const char *newpath) {
  if (g_fault.fail_rename) {
    errno = EXDEV;
    return -1;
  }
  return posix_rename(oldpath, newpath);
}

/**
 * @brief Intercepts mbrtowc() inside libsed_testable for multibyte panic
 * injection.
 * @param[out] pwc Optional wide-character output pointer.
 * @param[in] s Input byte pointer.
 * @param[in] n Maximum byte count to inspect.
 * @param[in,out] ps Multibyte conversion state.
 * @return Decoded byte count or special (size_t)-1 / (size_t)-2 / injected
 * code.
 */
size_t sed_hook_mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
  if (g_fault.force_mbrtowc_ret != 0) {
    return (size_t)g_fault.force_mbrtowc_ret;
  }
#if defined(_WIN32) || defined(__WATCOMC__)
  return rpl_mbrtowc(pwc, s, n, ps);
#else
  return mbrtowc(pwc, s, n, ps);
#endif
}

/**
 * @brief Intercepts isatty() inside libsed_testable for terminal panic testing.
 * @param[in] fd File descriptor to check.
 * @return Non-zero when fd is (or is simulated as) a terminal.
 */
int sed_hook_isatty(int fd) {
  if (g_fault.force_isatty) {
    return 1;
  }
#if defined(_WIN32) && !defined(__CYGWIN__)
  return _isatty(fd);
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
  (void)fd;
  return 0;
#else
  return isatty(fd);
#endif
}

#if defined(__WATCOMC__)
/**
 * @brief Executes an MS-DOS scratch-file popen command under OpenWatcom.
 * @param[in] cmd Shell command string.
 * @param[in] type Stream mode ("r" or "w").
 * @return Stream pointer or NULL on failure.
 */
extern FILE *sed_watcom_popen(const char *cmd, const char *type);
/**
 * @brief Opens and binds in-process stdin/stdout/stderr on OpenWatcom DOS.
 * @param[in] args NULL-terminated CLI arguments array.
 * @param[in] in_path Input scratch file path.
 * @param[in] out_path Output scratch file path.
 * @param[in] err_path Error scratch file path.
 * @param[out] saved Saved descriptor array of length 4.
 * @return 0 on success, -1 on I/O failure.
 */
extern int sed_watcom_inproc_begin(const char *const *args, const char *in_path,
                                   const char *out_path, const char *err_path,
                                   int saved[4]);
/**
 * @brief Flushes, restores, and closes redirected streams on OpenWatcom DOS.
 * @param[in] saved Saved descriptor array of length 4.
 */
extern void sed_watcom_inproc_end(const int saved[4]);
/**
 * @brief Binds in-process stderr capture on OpenWatcom DOS.
 * @param[in] err_path Error scratch file path.
 * @param[out] saved Saved descriptor array of length 2.
 * @return 0 on success, -1 on I/O failure.
 */
extern int sed_watcom_err_begin(const char *err_path, int saved[2]);
/**
 * @brief Restores and closes redirected stderr on OpenWatcom DOS.
 * @param[in] saved Saved descriptor array of length 2.
 */
extern void sed_watcom_err_end(const int saved[2]);
#endif

/**
 * @brief Intercepts popen() inside libsed_testable for subprocess fault
 * injection.
 * @param[in] command Shell command string.
 * @param[in] type Stream mode ("r" or "w").
 * @return Pipe stream pointer, or NULL on injected failure.
 */
FILE *sed_hook_popen(const char *command, const char *type) {
  if (g_fault.fail_popen) {
    errno = EAGAIN;
    return NULL;
  }
#if defined(_WIN32) && !defined(__CYGWIN__)
  return _popen(command, type);
#elif defined(__WATCOMC__)
  return sed_watcom_popen(command, type);
#else
  return popen(command, type);
#endif
}

/**
 * @brief Executes sed_testable_main in-process with stdio file redirection.
 * @param[in] env_kv Optional "KEY=VALUE" environment override (or NULL).
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] in_path Path to prepared binary stdin scratch file.
 * @param[in] out_path Path to destination stdout scratch file.
 * @param[in] err_path Path to destination stderr scratch file.
 * @param[out] exit_code_out Receives captured exit status code.
 * @return SED_TEST_OK on execution, error code on redirection failure.
 */
enum sed_test_err sed_inproc_runner(const char *env_kv, const char *const *args,
                                    const char *in_path, const char *out_path,
                                    const char *err_path, int *exit_code_out) {
  char argv_storage[64][512];
  char *mut_argv[65];
  char env_set_buf[256];
  char env_clr_buf[128];
  int has_env = 0;
  int argc = 1;
  size_t i;
#if defined(__WATCOMC__)
  int watcom_saved[4];
#else
  int saved_in;
  int saved_out;
  int saved_err;
  FILE *fi;
  FILE *fo;
  FILE *fe;
#endif
  int jrc;

  if (args == NULL || in_path == NULL || out_path == NULL || err_path == NULL ||
      exit_code_out == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }

  strcpy(argv_storage[0], "sed");
  mut_argv[0] = argv_storage[0];
  for (i = 0U; args[i] != NULL && i < 63U; ++i) {
    size_t alen = strlen(args[i]);
    if (alen >= sizeof(argv_storage[0])) {
      alen = sizeof(argv_storage[0]) - 1U;
    }
    memcpy(argv_storage[i + 1U], args[i], alen);
    argv_storage[i + 1U][alen] = (char)0;
    mut_argv[i + 1U] = argv_storage[i + 1U];
    ++argc;
  }
  mut_argv[argc] = NULL;

#if defined(__WATCOMC__)
  fflush(stdout);
  fflush(stderr);
  clearerr(stdin);
  clearerr(stdout);
  clearerr(stderr);
  if (sed_watcom_inproc_begin(args, in_path, out_path, err_path,
                              watcom_saved) != 0) {
    return SED_TEST_ERR_IO;
  }
#else
  fi = fopen(in_path, "rb");
  fo = fopen(out_path, "wb");
  fe = fopen(err_path, "wb");
  if (fi == NULL || fo == NULL || fe == NULL) {
    if (fi != NULL) {
      fclose(fi);
    }
    if (fo != NULL) {
      fclose(fo);
    }
    if (fe != NULL) {
      fclose(fe);
    }
    return SED_TEST_ERR_IO;
  }

  fflush(stdout);
  fflush(stderr);
  clearerr(stdin);
  clearerr(stdout);
  clearerr(stderr);

#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__)
  saved_in = _dup(0);
  saved_out = _dup(1);
  saved_err = _dup(2);
  (void)_dup2(_fileno(fi), 0);
  (void)_dup2(_fileno(fo), 1);
  (void)_dup2(_fileno(fe), 2);
#else
  saved_in = dup(0);
  saved_out = dup(1);
  saved_err = dup(2);
  (void)dup2(fileno(fi), 0);
  (void)dup2(fileno(fo), 1);
  (void)dup2(fileno(fe), 2);
#endif
#endif

  if (env_kv != NULL) {
    const char *eq = strchr(env_kv, '=');
    size_t klen = strlen(env_kv);
    if (klen < sizeof(env_set_buf) && eq != NULL &&
        (size_t)(eq - env_kv) + 2U < sizeof(env_clr_buf)) {
      size_t nlen = (size_t)(eq - env_kv);
      memcpy(env_set_buf, env_kv, klen + 1U);
      memcpy(env_clr_buf, env_kv, nlen);
      env_clr_buf[nlen] = '=';
      env_clr_buf[nlen + 1U] = (char)0;
      has_env = 1;
#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__) ||               \
    defined(__WATCOMC__)
      (void)_putenv(env_set_buf);
#else
      env_clr_buf[nlen] = (char)0;
      (void)setenv(env_clr_buf, eq + 1, 1);
#endif
    }
  }

  g_exit_status = 0;
  sed_testable_reset_execute();
  g_jmp_active = 1;
  jrc = setjmp(g_exit_jmp);
  if (jrc == 0) {
    g_exit_status = sed_testable_main(argc, mut_argv);
  } else if (jrc == 256) {
    g_exit_status = 0;
  } else {
    g_exit_status = jrc;
  }
  g_jmp_active = 0;

  /* Reset fault state and reclaim any open streams/compiler globals after jump
   */
  sed_fault_reset();
  sed_testable_reset_execute();
  g_jmp_active = 1;
  if (setjmp(g_exit_jmp) == 0) { /* LCOV_EXCL_BR_LINE */
    finish_program(NULL);
    ck_fclose(NULL);
  }
  g_jmp_active = 0;
  remove_cleanup_file();
  cancel_cleanup();

#if defined(__WATCOMC__)
  sed_watcom_inproc_end(watcom_saved);
  clearerr(stdin);
  if (has_env) {
    (void)_putenv(env_clr_buf);
  }
#elif defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__)
  fflush(stdout);
  fflush(stderr);
  clearerr(stdin);
  (void)_dup2(saved_in, 0);
  (void)_dup2(saved_out, 1);
  (void)_dup2(saved_err, 2);
  (void)_close(saved_in);
  (void)_close(saved_out);
  (void)_close(saved_err);
  if (has_env) {
    (void)_putenv(env_clr_buf);
  }
  fclose(fi);
  fclose(fo);
  fclose(fe);
#else
  fflush(stdout);
  fflush(stderr);
  clearerr(stdin);
  (void)dup2(saved_in, 0);
  (void)dup2(saved_out, 1);
  (void)dup2(saved_err, 2);
  (void)close(saved_in);
  (void)close(saved_out);
  (void)close(saved_err);
  if (has_env) {
    (void)unsetenv(env_clr_buf);
  }
  fclose(fi);
  fclose(fo);
  fclose(fe);
#endif

  *exit_code_out = g_exit_status;
  return SED_TEST_OK;
}

/**
 * @brief Executes a white-box callback under the libsed_testable panic/exit
 * trap with stderr redirected to err_path.
 * @param[in] fn Callback function to invoke.
 * @param[in,out] ctx Opaque context pointer passed to fn.
 * @param[in] err_path Path to destination stderr scratch file.
 * @param[out] exit_code_out Receives 0 on normal return or trapped exit code.
 * @return SED_TEST_OK on execution, error code on invalid args or I/O failure.
 */
enum sed_test_err sed_inproc_call_trap(void (*fn)(void *), void *ctx,
                                       const char *err_path,
                                       int *exit_code_out) {
#if defined(__WATCOMC__)
  int watcom_err_saved[2];
#else
  FILE *fe;
  int saved_err;
#endif
  int jrc;

  if (fn == NULL || err_path == NULL || exit_code_out == NULL) {
    return SED_TEST_ERR_INVALID_ARG;
  }

#if defined(__WATCOMC__)
  fflush(stdout);
  fflush(stderr);
  if (sed_watcom_err_begin(err_path, watcom_err_saved) != 0) {
    return SED_TEST_ERR_IO;
  }
#else
  fe = fopen(err_path, "wb");
  if (fe == NULL) {
    return SED_TEST_ERR_IO;
  }

  fflush(stdout);
  fflush(stderr);

#if defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__)
  saved_err = _dup(2);
  (void)_dup2(_fileno(fe), 2);
#else
  saved_err = dup(2);
  (void)dup2(fileno(fe), 2);
#endif
#endif

  g_exit_status = 0;
  g_jmp_active = 1;
  jrc = setjmp(g_exit_jmp);
  if (jrc == 0) {
    fn(ctx);
    g_exit_status = 0;
  } else if (jrc == 256) {
    g_exit_status = 0;
  } else {
    g_exit_status = jrc;
  }
  g_jmp_active = 0;

  sed_fault_reset();

#if defined(__WATCOMC__)
  sed_watcom_err_end(watcom_err_saved);
#elif defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__)
  fflush(stdout);
  fflush(stderr);
  (void)_dup2(saved_err, 2);
  (void)_close(saved_err);
  fclose(fe);
#else
  fflush(stdout);
  fflush(stderr);
  (void)dup2(saved_err, 2);
  (void)close(saved_err);
  fclose(fe);
#endif

  *exit_code_out = g_exit_status;
  return SED_TEST_OK;
}
