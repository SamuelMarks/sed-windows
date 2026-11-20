/**
 * @file test_sed_whitebox.c
 * @brief Strict ISO C89 white-box and deterministic fault-injection suite
 * (t_white) linked against libsed_testable.
 */

#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <direct.h>
#include <io.h>
#elif defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
#ifndef _WCRTLINK
/** @brief Fallback empty OpenWatcom C runtime linkage qualifier. */
#define _WCRTLINK
#endif
_WCRTLINK extern int _dup(int handle);
_WCRTLINK extern int _close(int handle);
_WCRTLINK extern int mkdir(const char *path);
_WCRTLINK extern int rmdir(const char *path);
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "sed_test_harness.h"
#include "sed_test_inproc.h"

/** @brief Opaque dynamic byte buffer from sed/utils.h. */
struct buffer;
/** @brief Opaque compiled command vector from sed/sed.h. */
struct vector;
/** @brief Opaque compiled command instruction from sed/sed.h. */
struct sed_cmd;

/**
 * @brief Wraps a file descriptor in a tracked FILE stream inside sed/utils.c.
 * @param[in] fd Open OS file descriptor.
 * @param[in] name Diagnostic stream name.
 * @param[in] mode Standard stdio mode string.
 * @param[in] fail Non-zero to panic on failure, 0 to return NULL.
 * @return Stream pointer on success, NULL when fail==0 and fdopen fails.
 */
extern FILE *ck_fdopen(int fd, const char *name, const char *mode, int fail);
/**
 * @brief Writes nmemb elements of size bytes or panics on short write.
 * @param[in] ptr Source buffer.
 * @param[in] size Element byte count.
 * @param[in] nmemb Element count.
 * @param[in,out] stream Destination FILE stream.
 */
extern void ck_fwrite(const void *ptr, ptrdiff_t size, ptrdiff_t nmemb,
                      FILE *stream);
/**
 * @brief Reads up to nmemb elements of size bytes or panics on stream error.
 * @param[out] ptr Destination buffer.
 * @param[in] size Element byte count.
 * @param[in] nmemb Maximum element count.
 * @param[in,out] stream Source FILE stream.
 * @return Number of elements read.
 */
extern ptrdiff_t ck_fread(void *ptr, ptrdiff_t size, ptrdiff_t nmemb,
                          FILE *stream);
/**
 * @brief Reads a delimited record from stream or panics on stream error.
 * @param[in,out] text Heap buffer pointer updated by getdelim.
 * @param[in,out] buflen Allocated capacity of *text.
 * @param[in] buffer_delimiter Record delimiter byte.
 * @param[in,out] stream Source FILE stream.
 * @return Byte count read or -1 on EOF.
 */
extern ptrdiff_t ck_getdelim(char **text, size_t *buflen, char buffer_delimiter,
                             FILE *stream);
/**
 * @brief Flushes and closes stream (or all tracked streams when NULL).
 * @param[in,out] stream Target stream or NULL.
 */
extern void ck_fclose(FILE *stream);
/**
 * @brief Allocates an empty sed/utils.c growth buffer.
 * @return Newly allocated struct buffer pointer.
 */
extern struct buffer *init_buffer(void);
/**
 * @brief Returns the raw character array backing a struct buffer.
 * @param[in] b Target buffer.
 * @return Pointer to internal byte array.
 */
extern char *get_buffer(const struct buffer *b);
/**
 * @brief Returns the current byte length stored in a struct buffer.
 * @param[in] b Target buffer.
 * @return Byte count currently buffered.
 */
extern ptrdiff_t size_buffer(const struct buffer *b);
/**
 * @brief Appends n bytes from p to a struct buffer, growing if needed.
 * @param[in,out] b Target buffer.
 * @param[in] p Source byte slice.
 * @param[in] n Byte count to append.
 * @return Updated internal byte array pointer.
 */
extern char *add_buffer(struct buffer *b, const char *p, ptrdiff_t n);
/**
 * @brief Appends a single byte c (if not EOF) to a struct buffer.
 * @param[in,out] b Target buffer.
 * @param[in] c Unsigned byte value or EOF.
 * @return Updated internal byte array pointer, or NULL when c == EOF.
 */
extern char *add1_buffer(struct buffer *b, int c);
/**
 * @brief Releases heap storage owned by a struct buffer.
 * @param[in,out] b Buffer to free (or NULL).
 */
extern void free_buffer(struct buffer *b);
/**
 * @brief Classifies whether byte ch contributes to a multibyte sequence.
 * @param[in] ch Unsigned input byte.
 * @param[in,out] ps Multibyte shift state.
 * @return Non-zero if part of a multibyte sequence, 0 otherwise.
 */
extern int is_mb_char(int ch, mbstate_t *ps);
/**
 * @brief Prints a single compiled sed command in --debug format.
 * @param[in] program Enclosing compiled program vector.
 * @param[in] sc Command entry to print (or NULL).
 */
extern void debug_print_command(const struct vector *program,
                                const struct sed_cmd *sc);
/**
 * @brief Prints an entire compiled sed program in --debug format.
 * @param[in] program Compiled program vector (or NULL).
 */
extern void debug_print_program(const struct vector *program);

/**
 * @brief Context passed to white-box trap callbacks.
 */
struct wb_trap_ctx {
  FILE *fp;   /**< Optional FILE pointer for stream fault callbacks. */
  int code;   /**< Selector or status code for callback dispatch. */
  int ret_mb; /**< Captured return value from is_mb_char(). */
};

/**
 * @brief Writes a tilde-escaped payload to disk in binary mode.
 * @param[in] path Destination path.
 * @param[in] tilde_str Tilde-escaped payload string.
 * @return SED_TEST_OK on success.
 */
static enum sed_test_err wb_write_tilde(const char *path,
                                        const char *tilde_str) {
  char buf[512];
  size_t n = sed_test_unescape(buf, sizeof(buf), tilde_str);
  return sed_test_write_file(path, buf, n);
}

/**
 * @brief Runs sed with tilde-escaped stdin and captures exit code + streams.
 * @param[in] args NULL-terminated argv array after argv[0].
 * @param[in] tilde_in Tilde-escaped stdin payload.
 * @param[out] cap Output capture struct.
 * @return SED_TEST_OK on success.
 */
static enum sed_test_err wb_run_tilde(const char *const *args,
                                      const char *tilde_in,
                                      struct sed_cmd_capture *cap) {
  char buf[512];
  size_t n = sed_test_unescape(buf, sizeof(buf), tilde_in);
  return sed_test_run(args, buf, n, cap);
}

/**
 * @brief Callback invoking sed_hook_exit(ctx->code) directly inside trap.
 * @param[in,out] arg Pointer to wb_trap_ctx.
 */
static void wb_cb_exit(void *arg) {
  struct wb_trap_ctx *ctx = (struct wb_trap_ctx *)arg;
  if (ctx->code >= 0) {
    sed_hook_exit(ctx->code);
  }
}

/**
 * @brief Callback invoking ck_fdopen(-1, "bad_fd", "r", 1) to trigger panic.
 * @param[in,out] arg Unused context pointer.
 */
static void wb_cb_bad_fdopen(void *arg) {
  (void)arg;
  (void)ck_fdopen(-1, "bad_fd", "r", 1);
}

/**
 * @brief Callback invoking ck_fwrite on an unregistered stream (<unknown>).
 * @param[in,out] arg Pointer to wb_trap_ctx holding open FILE stream.
 */
static void wb_cb_unknown_fwrite(void *arg) {
  struct wb_trap_ctx *ctx = (struct wb_trap_ctx *)arg;
  sed_fault_state()->fail_fwrite_after = 1;
  ck_fwrite("xy", 1, 2, ctx->fp);
}

/**
 * @brief Callback invoking ck_getdelim on a write-only stream to trigger
 * mid-read EBADF panic.
 * @param[in,out] arg Pointer to wb_trap_ctx holding write-only FILE stream.
 */
static void wb_cb_bad_getdelim(void *arg) {
  struct wb_trap_ctx *ctx = (struct wb_trap_ctx *)arg;
  char *lbuf = NULL;
  size_t lcap = 0U;
  (void)ck_getdelim(&lbuf, &lcap, (char)10, ctx->fp);
  free(lbuf);
}

/**
 * @brief Callback invoking is_mb_char() with injected unexpected mbrtowc code.
 * @param[in,out] arg Pointer to wb_trap_ctx receiving return value if any.
 */
static void wb_cb_mb_panic(void *arg) {
  struct wb_trap_ctx *ctx = (struct wb_trap_ctx *)arg;
  mbstate_t st;
  memset(&st, 0, sizeof(st));
  sed_fault_state()->force_mbrtowc_ret = 5;
  ctx->ret_mb = is_mb_char('a', &st);
}

/**
 * @brief Creates a temporary subdirectory portably across Host, Win32, and DOS.
 * @param[in] path Directory path to create.
 * @return 0 on success or if directory already exists.
 */
static int wb_mkdir(const char *path) {
#if defined(_WIN32) && !defined(__CYGWIN__)
  return _mkdir(path);
#elif defined(__WATCOMC__)
  extern unsigned char _osmajor;
  if (_osmajor < 2U) {
    return -1;
  }
  return mkdir(path);
#elif defined(__DOS__) || defined(__MSDOS__)
  return mkdir(path);
#else
  return mkdir(path, 0700);
#endif
}

/**
 * @brief Removes a temporary subdirectory portably across Host, Win32, and DOS.
 * @param[in] path Directory path to remove.
 */
static void wb_rmdir(const char *path) {
#if defined(_WIN32) && !defined(__CYGWIN__)
  (void)_rmdir(path);
#else
  (void)rmdir(path);
#endif
}

/**
 * @brief Verifies all branches of sed_test_inproc.c (invalid arguments, file
 * open failure rollbacks, >511-byte argument clamping, env overrides, and
 * sed_inproc_call_trap).
 * @return 0 on pass, 1 on failure.
 */
static int test_wb_inproc_runner_edges(void) {
  static const char *const ok_args[] = {"--version", NULL};
  static const char *const no_prev_re[] = {"//p", NULL};
  struct sed_cmd_capture cap;
  struct wb_trap_ctx ctx;
  char long_script[600];
  const char *long_args[3];
  int ec = -1;
  size_t i;

  /* 0. Verify "//p" before any regex has ever been matched in this process */
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(no_prev_re, "a~n", &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* 1. Invalid NULL parameter guards on sed_inproc_runner */
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_INVALID_ARG,
      sed_inproc_runner(NULL, NULL, "A.TXT", "B.TXT", "C.TXT", &ec));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_INVALID_ARG,
      sed_inproc_runner(NULL, ok_args, NULL, "B.TXT", "C.TXT", &ec));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_INVALID_ARG,
      sed_inproc_runner(NULL, ok_args, "A.TXT", NULL, "C.TXT", &ec));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_INVALID_ARG,
      sed_inproc_runner(NULL, ok_args, "A.TXT", "B.TXT", NULL, &ec));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_INVALID_ARG,
      sed_inproc_runner(NULL, ok_args, "A.TXT", "B.TXT", "C.TXT", NULL));

  /* 2. File-open failure cleanup paths: fi==NULL, fo==NULL, fe==NULL */
  (void)remove("TWBNOIN.TXT");
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_inproc_runner(NULL, ok_args, "TWBNOIN.TXT", "TWBO.TXT",
                                     "TWBE.TXT", &ec));
  (void)remove("TWBO.TXT");
  (void)remove("TWBE.TXT");

  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBIN.TXT", "a~n"));
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_inproc_runner(NULL, ok_args, "TWBIN.TXT",
                                     "no_such_dir_83/?.?", "TWBE.TXT", &ec));
  (void)remove("TWBE.TXT");

  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_inproc_runner(NULL, ok_args, "TWBIN.TXT", "TWBO.TXT",
                                     "no_such_dir_83/?.?", &ec));

  /* 3. >511-char comment argument truncation + env_kv edge cases */
  long_script[0] = '#';
  for (i = 1U; i < sizeof(long_script) - 1U; ++i) {
    long_script[i] = 'x';
  }
  long_script[sizeof(long_script) - 1U] = (char)0;
  long_args[0] = "-e";
  long_args[1] = long_script;
  long_args[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_inproc_runner("NOEQUALS", long_args, "TWBIN.TXT",
                                     "TWBO.TXT", "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(0, ec);

  {
    char big_env[300];
    char long_key_env[200];
    memset(big_env, 'K', 298U);
    big_env[10] = '=';
    big_env[298] = (char)0;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_inproc_runner(big_env, ok_args, "TWBIN.TXT",
                                       "TWBO.TXT", "TWBE.TXT", &ec));
    memset(long_key_env, 'K', 180U);
    long_key_env[140] = '=';
    long_key_env[180] = (char)0;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_inproc_runner(long_key_env, ok_args, "TWBIN.TXT",
                                       "TWBO.TXT", "TWBE.TXT", &ec));
  }

  (void)remove("TWBIN.TXT");
  (void)remove("TWBO.TXT");
  (void)remove("TWBE.TXT");

  /* 4. sed_inproc_call_trap guards & normal/zero/non-zero exit traps */
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_inproc_call_trap(NULL, &ctx, "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_inproc_call_trap(wb_cb_exit, &ctx, NULL, &ec));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_inproc_call_trap(wb_cb_exit, &ctx, "TWBE.TXT", NULL));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_IO,
      sed_inproc_call_trap(wb_cb_exit, &ctx, "no_such_dir_83/?.?", &ec));

  ctx.code = -1;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_inproc_call_trap(wb_cb_exit, &ctx, "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(0, ec);

  ctx.code = 0;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_inproc_call_trap(wb_cb_exit, &ctx, "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(0, ec);

  ctx.code = 4;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_inproc_call_trap(wb_cb_exit, &ctx, "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(4, ec);
  (void)remove("TWBE.TXT");

  return 0;
}

/**
 * @brief Verifies sed/utils.c deterministic fault injection and buffer APIs
 * (ck_fwrite, ck_fread, ck_getdelim, ck_fflush, ck_fclose, ck_mkstemp,
 * ck_rename, ck_fdopen, utils_fp_name, remove_cleanup_file, struct buffer).
 * @return 0 on pass, 1 on failure.
 */
static int test_wb_utils_fault_injection(void) {
  static const char *const p_cmd[] = {"p", NULL};
  static const char *const u_cmd[] = {"-u", "p", NULL};
  static const char *const w_file[] = {"w TWBFW.TXT", NULL};
  static const char *const w_err[] = {"w /dev/stderr", NULL};
  static const char *const r_file[] = {"r TWBFR.TXT", NULL};
  static const char *const ip_cmd[] = {"-i", "s/a/b/", "TWBIP.TXT", NULL};
  struct sed_cmd_capture cap;
  struct wb_trap_ctx ctx;
  struct buffer *b;
  char chunk[128];
  char rbuf[8];
  int ec = 0;
  int dup_fd;
  FILE *fp;
  size_t i;

  /* 1. ck_fwrite: fail on stdout (1 item & multi-item), open_files, stderr,
   *    and unregistered FILE* (<unknown>), plus size==0 no-op */
  sed_fault_state()->fail_fwrite_after = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(p_cmd, "hello~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf,
                   sizeof(ptrdiff_t) >= 8U ? "stdout" : "couldn't write") !=
                NULL);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_fwrite_after = 2;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(p_cmd, "hello~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_fwrite_after = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(w_file, "hello~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf,
                   sizeof(ptrdiff_t) >= 8U ? "TWBFW.TXT" : "couldn't write") !=
                NULL);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBFW.TXT");

  sed_fault_state()->fail_fwrite_after = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(w_err, "hello~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf,
                   sizeof(ptrdiff_t) >= 8U ? "stderr" : "couldn't write") !=
                NULL);
  sed_cmd_capture_free(&cap);

  fp = fopen("TWBUNK.TXT", "wb");
  SED_CHECK(fp != NULL);
  ck_fwrite("x", 0, 1, fp);
  ctx.fp = fp;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_inproc_call_trap(wb_cb_unknown_fwrite, &ctx,
                                                     "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(4, ec);
  fclose(fp);
  (void)remove("TWBUNK.TXT");
  (void)remove("TWBE.TXT");

  /* 2. ck_fread (via 'r' print_file & size==0) and ck_getdelim (pre-ferror &
   *    mid-getdelim fread error) */
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBFR.TXT", "read_data~n"));
  fp = fopen("TWBFR.TXT", "rb");
  SED_CHECK(fp != NULL);
  SED_CHECK_INT_EQ(1L, (long)ck_fread(rbuf, 0, 1, fp));
  fclose(fp);

  sed_fault_state()->fail_fread_after = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(r_file, "in~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "TWBFR.TXT") != NULL);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_fread_after = 2;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(r_file, "in~nb~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBFR.TXT");

  sed_fault_state()->force_ferror = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(p_cmd, "in~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "stdin") != NULL);
  sed_cmd_capture_free(&cap);

  fp = fopen("TWBWO.TXT", "wb");
  SED_CHECK(fp != NULL);
  ctx.fp = fp;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_inproc_call_trap(wb_cb_bad_getdelim, &ctx,
                                                     "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(4, ec);
  fclose(fp);
  (void)remove("TWBWO.TXT");
  (void)remove("TWBE.TXT");

  /* 3. ck_fflush (-u with EBADF ignored vs EIO panic) & do_ck_fclose failure */
  sed_fault_state()->fail_fflush_after = 1;
  sed_fault_state()->fail_fflush_errno = EBADF;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(u_cmd, "in~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_fflush_after = 2;
  sed_fault_state()->fail_fflush_errno = 0;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(u_cmd, "in~nb~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "flush") != NULL);
  sed_cmd_capture_free(&cap);

  {
    static const char *const w2_cmd[] = {"-e", "w TWBFW.TXT", "-e",
                                         "w TWBFW2.TXT", NULL};
    sed_fault_state()->fail_fwrite_after = 2;
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(w2_cmd, "in~n", &cap));
    SED_CHECK_INT_EQ(4, cap.exit_code);
    sed_cmd_capture_free(&cap);

    sed_fault_state()->fail_fclose_after = 2;
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(w2_cmd, "in~n", &cap));
    SED_CHECK_INT_EQ(4, cap.exit_code);
    SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "close") != NULL);
    sed_cmd_capture_free(&cap);
    (void)remove("TWBFW.TXT");
    (void)remove("TWBFW2.TXT");
  }
  (void)sed_hook_fclose(stdin);
  (void)sed_hook_fclose(stderr);

  /* 4. ck_mkstemp & ck_rename + remove_cleanup_file during sed -i */
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBIP.TXT", "a~n"));
  sed_fault_state()->fail_mkostemp = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_cmd, "", 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_rename = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_cmd, "", 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf, "cannot rename") != NULL);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBIP.TXT");

  /* 5. ck_fdopen valid fd, invalid fd (fail=0 -> NULL), invalid fd (fail=1) */
#if defined(__WATCOMC__)
  {
    extern unsigned char _osmajor;
    dup_fd = (_osmajor >= 2U) ? _dup(0) : -1;
  }
#elif defined(_WIN32) || defined(__DOS__) || defined(__MSDOS__)
  dup_fd = _dup(0);
#else
  dup_fd = dup(0);
#endif
  if (dup_fd >= 0) {
    fp = ck_fdopen(dup_fd, "dup_stdin", "r", 1);
    SED_CHECK(fp != NULL);
    ck_fclose(fp);
  }
  SED_CHECK(ck_fdopen(-1, "bad_fd", "r", 0) == NULL);
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_inproc_call_trap(wb_cb_bad_fdopen, NULL,
                                                     "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(4, ec);
  (void)remove("TWBE.TXT");

  /* 6. struct buffer API: init_buffer, add_buffer, add1_buffer(EOF), free */
  b = init_buffer();
  SED_CHECK(b != NULL);
  memset(chunk, 'B', sizeof(chunk));
  SED_CHECK(add_buffer(b, "hi", 2) != NULL);
  SED_CHECK(add_buffer(b, chunk, (ptrdiff_t)sizeof(chunk)) != NULL);
  for (i = 0U; i < 128U; ++i) {
    SED_CHECK(add1_buffer(b, 'z') != NULL);
  }
  SED_CHECK(add1_buffer(b, EOF) == NULL);
  SED_CHECK_INT_EQ((long)(2U + sizeof(chunk) + 128U), (long)size_buffer(b));
  SED_CHECK(get_buffer(b)[0] == 'h');
  free_buffer(b);
  free_buffer(NULL);

  return 0;
}

/**
 * @brief Verifies sed/execute.c edge paths: terminal/directory -i panics,
 * subdirectory -i, popen failures in e and s-e, s-ep, T reset, multi-slot
 * append queue, missing newline between files, resize_line compaction, and l
 * control-byte escapes.
 * @return 0 on pass, 1 on failure.
 */
static int test_wb_execute_and_popen_faults(void) {
  static const char *const ip_f[] = {"-i", "s/a/b/", "TWBEX.TXT", NULL};
  static const char *const ip_dir[] = {"-i", "s/a/b/", "TWBDIR", NULL};
  static const char *const ip_sub[] = {"-i", "s/a/b/", "TWBDIR/S.TXT", NULL};
  static const char *const e_cmd[] = {"e echo ok", NULL};
  static const char *const se_cmd[] = {"s/a/echo ok/e", NULL};
  static const char *const sep_cmd[] = {"-n", "s/.*/echo ok/ep", NULL};
  static const char *const t_reset[] = {"s/a/b/;T skp;s/b/c/;:skp", NULL};
  static const char *const multi_a[] = {"-e", "a~/~n1", "-e", "a~/~n2", NULL};
  static const char *const miss_nl[] = {"p", "TWBN1.TXT", "TWBN2.TXT", NULL};
  static const char *const eof_2f[] = {"$p", "TWBN1.TXT", "TWBEMP.TXT", NULL};
  static const char *const ovf_step[] = {"-n", "1,+9223372036854775807p", NULL};
  static const char *const ovf_mod[] = {"-n", "1,~~9223372036854775807p", NULL};
  static const char *const list_ctl[] = {"-n", "l", NULL};
  struct sed_cmd_capture cap;
  char ctl_in[12];
  char esc_pat[8];

  /* 1. force_isatty on -i, -i on directory, and -i in subdirectory */
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBEX.TXT", "a~n"));
  sed_fault_state()->force_isatty = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_f, "", 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "terminal") != NULL);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBEX.TXT");

  if (wb_mkdir("TWBDIR") == 0) {
#if !defined(_WIN32) && !defined(__DOS__) && !defined(__MSDOS__) &&            \
    !defined(__WATCOMC__)
    /* On POSIX fopen("TWBDIR", "r") succeeds and fstat(!S_ISREG) panics */
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_dir, "", 0U, &cap));
    SED_CHECK_INT_EQ(4, cap.exit_code);
    SED_CHECK(cap.err_buf != NULL &&
              strstr(cap.err_buf, "not a regular file") != NULL);
    sed_cmd_capture_free(&cap);
#else
    (void)ip_dir;
#endif
    if (wb_write_tilde("TWBDIR/S.TXT", "a~n") == SED_TEST_OK) {
      SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_sub, "", 0U, &cap));
      SED_CHECK_INT_EQ(0, cap.exit_code);
      sed_cmd_capture_free(&cap);
      (void)remove("TWBDIR/S.TXT");
    }
    wb_rmdir("TWBDIR");
  } else {
    (void)ip_dir;
    (void)ip_sub;
  }

  /* 2. popen failures on 'e' and 's/a/b/e', plus 's/a/b/ep' post-eval print */
  sed_fault_state()->fail_popen = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(e_cmd, "x~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  sed_fault_state()->fail_popen = 1;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(se_cmd, "a~n", &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(sep_cmd, "a~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "ok") != NULL);
  sed_cmd_capture_free(&cap);

  /* 3. T command when replaced==true, multi-append queue, missing newline */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(t_reset, "a~n", "c~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(multi_a, "x~n", "x~n1~n2~n", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TWBN1.TXT", "no_nl", 5U));
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBN2.TXT", "l2~n"));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TWBEMP.TXT", "", 0U));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(miss_nl, "", 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(eof_2f, "", 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBN1.TXT");
  (void)remove("TWBN2.TXT");
  (void)remove("TWBEMP.TXT");

  /* 4. resize_line inactive buffer compaction + step overflow + l escapes */
  {
    char many_lines[1024];
    const char *nd_args[2];
    size_t k;
    for (k = 0U; k < 180U; ++k) {
      memcpy(many_lines + (k * 5U), "aaaa\0", 4U);
      many_lines[(k * 5U) + 4U] = (char)10;
    }
    nd_args[0] = "$!{N;D};s/$/ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ/";
    nd_args[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_run(nd_args, many_lines, 900U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);

    nd_args[0] = "1,$p;1,0p;N;$p";
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(nd_args, "1~n2~n3~n", &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }
  {
    static const char *const sep_eof[] = {"-s",        "-n",        "$p",
                                          "TWBS1.TXT", "TWBS2.TXT", NULL};
    static const char *const dbg_exec[] = {"--debug",
                                           "n;N;x;y/a/A/;z;s/a//;N;D", NULL};
    static const char *const dbg_n_eof[] = {"--debug", "n", NULL};
    static const char *const zero_re_l1[] = {"-n", "0,/a/p", NULL};
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBS1.TXT", "1~n"));
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBS2.TXT", "2~n"));
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sep_eof, "", 0U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
    (void)remove("TWBS1.TXT");
    (void)remove("TWBS2.TXT");

    SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(dbg_exec, "a~na~na~na~n", &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(dbg_n_eof, "a~n", &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_expect_tilde(zero_re_l1, "a~nb~n", "a~n", 0));
  }

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(ovf_step, "1~n2~n", "1~n2~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(ovf_mod, "1~n2~n", "1~n2~n", 0));

  ctl_in[0] = (char)7;
  ctl_in[1] = (char)8;
  ctl_in[2] = (char)12;
  ctl_in[3] = (char)13;
  ctl_in[4] = (char)9;
  ctl_in[5] = (char)11;
  ctl_in[6] = (char)92;
  ctl_in[7] = (char)1;
  ctl_in[8] = (char)10;
  ctl_in[9] = 'z';
  ctl_in[10] = (char)10;
  (void)sed_test_unescape(esc_pat, sizeof(esc_pat), "~/001");
  {
    const char *nl_l[2];
    nl_l[0] = "N;l";
    nl_l[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(list_ctl, ctl_in, 9U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, esc_pat) != NULL);
    sed_cmd_capture_free(&cap);
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(nl_l, ctl_in, 11U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }

  return 0;
}

/**
 * @brief Verifies sed/mbcs.c, sed/regexp.c, and POSIX follow_symlink() paths.
 * @return 0 on pass, 1 on failure.
 */
static int test_wb_mbcs_regexp_and_symlinks(void) {
  static const char *const dfa_warn_re[] = {"-n", "/[:space:]/p", NULL};
  static const char *const no_prev_re[] = {"//p", NULL};
  static const char *const beg_g[] = {"N;s/^/X/g", NULL};
  static const char *const beg_mg[] = {"N;s/^/X/mg", NULL};
  static const char *const addr_m[] = {"-n", "/foo/Mp", NULL};
  static const char *const z_m1[] = {"-z", "-E", "N;N;s/(a)(x)?$/[~/1]/mg",
                                     NULL};
  static const char *const z_m2[] = {"-z", "s/a$/X/m", NULL};
  struct sed_cmd_capture cap;
  struct wb_trap_ctx ctx;
  char sp_in[4];
  mbstate_t st;
  int ec = 0;

  /* 1. mbcs.c: NUL byte (case 0 -> 1) and unexpected mbrtowc panic */
  memset(&st, 0, sizeof(st));
  SED_CHECK_INT_EQ(1, is_mb_char(0, &st));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_inproc_call_trap(wb_cb_mb_panic, &ctx, "TWBE.TXT", &ec));
  SED_CHECK_INT_EQ(4, ec);
  (void)remove("TWBE.TXT");

  /* UTF-8 invalid byte (-1) and multibyte char class / y-errors if UTF-8 host
   */
  if (setlocale(LC_ALL, "en_US.UTF-8") != NULL ||
      setlocale(LC_ALL, "C.UTF-8") != NULL) {
    char bad_mb[32];
    char line_a[4];
    const char *mb_args[3];
    size_t la_len = sed_test_unescape(line_a, sizeof(line_a), "a~n");
    (void)setlocale(LC_ALL, "C");

    /* Multibyte delimiter error */
    bad_mb[0] = 's';
    ((unsigned char *)bad_mb)[1] = 0xceU;
    ((unsigned char *)bad_mb)[2] = 0xa6U;
    bad_mb[3] = 'a';
    bad_mb[4] = (char)0;
    mb_args[0] = bad_mb;
    mb_args[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run_env("LC_ALL=en_US.UTF-8",
                                                   mb_args, "", 0U, &cap));
    if (cap.exit_code == 0) {
      sed_cmd_capture_free(&cap);
    } else {
      sed_cmd_capture_free(&cap);
      /* Invalid byte 0xff inside regex & bracket class & case replacement */
      bad_mb[0] = 's';
      bad_mb[1] = '/';
      bad_mb[2] = '[';
      ((unsigned char *)bad_mb)[3] = 0xceU;
      ((unsigned char *)bad_mb)[4] = 0xa6U;
      bad_mb[5] = '[';
      bad_mb[6] = ':';
      ((unsigned char *)bad_mb)[7] = 0xceU;
      ((unsigned char *)bad_mb)[8] = 0xa6U;
      bad_mb[9] = ':';
      bad_mb[10] = ']';
      bad_mb[11] = '[';
      bad_mb[12] = '.';
      ((unsigned char *)bad_mb)[13] = 0xceU;
      ((unsigned char *)bad_mb)[14] = 0xa6U;
      bad_mb[15] = '.';
      bad_mb[16] = ']';
      ((unsigned char *)bad_mb)[17] = 0xffU;
      bad_mb[18] = ']';
      bad_mb[19] = '/';
      bad_mb[20] = 'X';
      bad_mb[21] = '/';
      bad_mb[22] = (char)0;
      SED_CHECK_INT_EQ(SED_TEST_OK,
                       sed_test_run_env("LC_ALL=en_US.UTF-8", mb_args, line_a,
                                        la_len, &cap));
      sed_cmd_capture_free(&cap);

      /* Long UTF-8 y expansion triggering resize_line + invalid 0xff byte */
      if (la_len > 0U) {
        char y_big[200];
        bad_mb[0] = 'y';
        bad_mb[1] = '/';
        bad_mb[2] = 'a';
        bad_mb[3] = '/';
        ((unsigned char *)bad_mb)[4] = 0xceU;
        ((unsigned char *)bad_mb)[5] = 0xa6U;
        bad_mb[6] = '/';
        bad_mb[7] = (char)0;
        ((unsigned char *)y_big)[0] = 0xffU;
        memset(y_big + 1, 'a', 190U);
        y_big[191] = (char)10;
        SED_CHECK_INT_EQ(
            SED_TEST_OK,
            sed_test_run_env("LC_ALL=en_US.UTF-8", mb_args, y_big, 192U, &cap));
        SED_CHECK_INT_EQ(0, cap.exit_code);
        sed_cmd_capture_free(&cap);
      }

      bad_mb[0] = 's';
      bad_mb[1] = '/';
      bad_mb[2] = '.';
      bad_mb[3] = '*';
      bad_mb[4] = '/';
      bad_mb[5] = (char)92;
      bad_mb[6] = 'U';
      bad_mb[7] = '&';
      bad_mb[8] = '/';
      bad_mb[9] = (char)0;
      {
        char inv_in[6];
        ((unsigned char *)inv_in)[0] = 0xffU;
        inv_in[1] = 'a';
        ((unsigned char *)inv_in)[2] = 0xceU;
        inv_in[3] = (char)10;
        SED_CHECK_INT_EQ(
            SED_TEST_OK,
            sed_test_run_env("LC_ALL=en_US.UTF-8", mb_args, inv_in, 4U, &cap));
        SED_CHECK_INT_EQ(0, cap.exit_code);
        sed_cmd_capture_free(&cap);

        bad_mb[5] = (char)92;
        bad_mb[6] = 'u';
        SED_CHECK_INT_EQ(
            SED_TEST_OK,
            sed_test_run_env("LC_ALL=en_US.UTF-8", mb_args, inv_in, 4U, &cap));
        SED_CHECK_INT_EQ(0, cap.exit_code);
        sed_cmd_capture_free(&cap);
      }

      /* y command multibyte length mismatches & invalid bytes */
      bad_mb[0] = 'y';
      bad_mb[1] = '/';
      ((unsigned char *)bad_mb)[2] = 0xffU;
      bad_mb[3] = 'b';
      bad_mb[4] = '/';
      bad_mb[5] = 'c';
      bad_mb[6] = '/';
      bad_mb[7] = (char)0;
      SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run_env("LC_ALL=en_US.UTF-8",
                                                     mb_args, "", 0U, &cap));
      SED_CHECK_INT_EQ(1, cap.exit_code);
      sed_cmd_capture_free(&cap);

      bad_mb[0] = 'y';
      bad_mb[1] = '/';
      bad_mb[2] = 'a';
      bad_mb[3] = '/';
      ((unsigned char *)bad_mb)[4] = 0xffU;
      bad_mb[5] = 'b';
      bad_mb[6] = '/';
      bad_mb[7] = (char)0;
      SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run_env("LC_ALL=en_US.UTF-8",
                                                     mb_args, "", 0U, &cap));
      SED_CHECK_INT_EQ(1, cap.exit_code);
      sed_cmd_capture_free(&cap);
    }
  }

  /* 2. regexp.c: dfawarn/dfaerror with and without POSIXLY_CORRECT */
  (void)sed_test_unescape(sp_in, sizeof(sp_in), " ~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dfa_warn_re, sp_in, 2U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run_env("POSIXLY_CORRECT=1",
                                                 dfa_warn_re, sp_in, 2U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)no_prev_re;

  /* begline optimizations & /M address & -z /m embedded buffer_delimiter */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(beg_g, "1~n2~n", "X1~n2~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(beg_mg, "~n2~n", "X~nX2~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(beg_mg, "1~n2~n", "X1~nX2~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(addr_m, "a~nfoo~n", "foo~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(z_m1, "b~0a~0a~0",
                                                      "b~0[a]~0[a]~0", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(z_m2, "ba~0", "bX~0", 0));
  {
    static const char *const z_m3[] = {"-z", "s/a/X/mg", NULL};
    static const char *const z_m4[] = {"-z", "-n", "/a$/Mp", NULL};
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_expect_tilde(z_m3, "aa~0", "XX~0", 0));
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_expect_tilde(z_m4, "ba~0", "ba~0", 0));
  }

#if !defined(_WIN32) && !defined(__DOS__) && !defined(__MSDOS__) &&            \
    !defined(__WATCOMC__)
  /* 3. POSIX follow_symlink(): regular file, subdir chain, long link, ELOOP,
   *    and ENOTDIR readlink error */
  if (sp_in[0] != (char)0) {
    const char *fs_args[5];
    char long_rel[192];
    char cwd_abs[512];
    size_t k;

    (void)wb_mkdir("TWBSUB");
    (void)wb_mkdir("TWBSUB/DEEPLONGDIR");
    SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBSUB/REAL.TXT", "a~n"));
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     wb_write_tilde("TWBSUB/DEEPLONGDIR/REAL.TXT", "a~n"));

    /* Regular file under --follow-symlinks -i (EINVAL break) */
    fs_args[0] = "--follow-symlinks";
    fs_args[1] = "-i";
    fs_args[2] = "s/a/b/";
    fs_args[3] = "TWBSUB/REAL.TXT";
    fs_args[4] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);

    /* Relative symlink in CWD without '/' (strrchr(fn, '/') == NULL) +
     * >128-byte directory prefix with short relative target (buf_size <=
     * newlen) */
    (void)unlink("TWBCWDL");
    SED_CHECK_INT_EQ(0, symlink("TWBSUB/REAL.TXT", "TWBCWDL"));
    fs_args[3] = "TWBCWDL";
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
    (void)unlink("TWBCWDL");

    {
      const char *d1 =
          "TWBSUB/"
          "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD";
      const char *d2 =
          "TWBSUB/"
          "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD/"
          "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE";
      const char *d2_real =
          "TWBSUB/"
          "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD/"
          "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE/"
          "R.TXT";
      const char *d2_sym =
          "TWBSUB/"
          "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD/"
          "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE/"
          "S.TXT";
      (void)wb_mkdir(d1);
      (void)wb_mkdir(d2);
      SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde(d2_real, "a~n"));
      (void)unlink(d2_sym);
      SED_CHECK_INT_EQ(0, symlink("R.TXT", d2_sym));
      fs_args[3] = d2_sym;
      SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
      SED_CHECK_INT_EQ(0, cap.exit_code);
      sed_cmd_capture_free(&cap);
      (void)unlink(d2_sym);
      (void)remove(d2_real);
      wb_rmdir(d2);
      wb_rmdir(d1);
    }

    /* Hop 1 short (TWBSUB/S -> DEEPLONGDIR/L2, grows newlen on hop 1),
     * Hop 2 in DEEPLONGDIR -> L3 (num_links>=1 grows newlen on hop 2),
     * Hop 3 -> long_rel (num_links>=1 grows readlink loop buffer!) */
    strcpy(long_rel, "./");
    for (k = 0U; k < 60U; ++k) {
      strcat(long_rel, "./");
    }
    strcat(long_rel, "REAL.TXT");
    (void)unlink("TWBSUB/DEEPLONGDIR/L3");
    (void)unlink("TWBSUB/DEEPLONGDIR/L2");
    (void)unlink("TWBSUB/S");
    SED_CHECK_INT_EQ(0, symlink(long_rel, "TWBSUB/DEEPLONGDIR/L3"));
    SED_CHECK_INT_EQ(0, symlink("./././././././L3", "TWBSUB/DEEPLONGDIR/L2"));
    SED_CHECK_INT_EQ(0, symlink("DEEPLONGDIR/L2", "TWBSUB/S"));
    fs_args[3] = "TWBSUB/S";
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);

    if (getcwd(cwd_abs, sizeof(cwd_abs) - 32U) != NULL) {
      strcat(cwd_abs, "/TWBSUB/REAL.TXT");
      (void)unlink("TWBSUB/ABS");
      SED_CHECK_INT_EQ(0, symlink(cwd_abs, "TWBSUB/ABS"));
      fs_args[3] = "TWBSUB/ABS";
      SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
      SED_CHECK_INT_EQ(0, cap.exit_code);
      sed_cmd_capture_free(&cap);
      (void)unlink("TWBSUB/ABS");
    }

    /* Symlink loop -> ELOOP panic (exit 4) */
    (void)unlink("TWBSUB/LPA");
    (void)unlink("TWBSUB/LPB");
    SED_CHECK_INT_EQ(0, symlink("LPB", "TWBSUB/LPA"));
    SED_CHECK_INT_EQ(0, symlink("LPA", "TWBSUB/LPB"));
    fs_args[3] = "TWBSUB/LPA";
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
    SED_CHECK_INT_EQ(4, cap.exit_code);
    sed_cmd_capture_free(&cap);

    /* ENOTDIR on readlink("TWBSUB/REAL.TXT/CHILD") -> panic (exit 4) */
    fs_args[3] = "TWBSUB/REAL.TXT/CHILD";
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(fs_args, "", 0U, &cap));
    SED_CHECK_INT_EQ(4, cap.exit_code);
    sed_cmd_capture_free(&cap);

    (void)unlink("TWBSUB/LPA");
    (void)unlink("TWBSUB/LPB");
    (void)unlink("TWBSUB/S");
    (void)unlink("TWBSUB/DEEPLONGDIR/L2");
    (void)unlink("TWBSUB/DEEPLONGDIR/L3");
    (void)remove("TWBSUB/DEEPLONGDIR/REAL.TXT");
    (void)remove("TWBSUB/REAL.TXT");
    wb_rmdir("TWBSUB/DEEPLONGDIR");
    wb_rmdir("TWBSUB");
  }
#endif

  return 0;
}

/**
 * @brief Verifies sed/compile.c syntax/diagnostic branches and sed/debug.c
 * edge formatting paths.
 * @return 0 on pass, 1 on failure.
 */
static int test_wb_compile_and_debug_edges(void) {
  static const char *const bad_exprs[] = {
      "= extra",   "+2p",       "~2p",   "1,p",     "0p",  "0,2p",
      "1!!p",      "1#comment", "}",     "1}",      "{1}", "{",
      "a",         "1:lbl",     "1,2q",  "/unterm", "s/a", "s/a/b",
      "s/[abc/x/", "y/a",       "y/a/b", "1",       NULL};
  static const char *const bad_posix[] = {
      "s/a/b/m", "s/a/b/e", "0,/a/p", "1,2=", "a hi", "e", NULL};
  struct sed_cmd_capture cap;
  char dec1[128];
  char dec2[128];
  const char *args[5];
  size_t i;

  /* 1. All bad_prog() compilation error diagnostics */
  for (i = 0U; bad_exprs[i] != NULL; ++i) {
    args[0] = bad_exprs[i];
    args[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
    SED_CHECK_INT_EQ(1, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }

  for (i = 0U; bad_posix[i] != NULL; ++i) {
    args[0] = "--posix";
    args[1] = bad_posix[i];
    args[2] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
    SED_CHECK_INT_EQ(1, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }

  /* Missing jump label -> panic (exit 4); --posix incomplete a~/ -> exit 1 */
  args[0] = "b no_such_lbl";
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBINC.SED", "a~/~nhi~/"));
  args[0] = "--posix";
  args[1] = "-f";
  args[2] = "TWBINC.SED";
  args[3] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBINC.SED");

  /* 2. -f script file with error report, ungetc, CRLF s/a/b/CRLF, multi -e a */
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBBAD.SED", "s/[unterm~np~n"));
  args[0] = "-f";
  args[1] = "TWBBAD.SED";
  args[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "TWBBAD.SED") != NULL);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      wb_write_tilde("TWBOK.SED",
                     "s/a/b/~r~n1{~np~n}~n9999999999999999999999999=~n"));
  args[0] = "-f";
  args[1] = "TWBOK.SED";
  args[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "a~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBBAD.SED");
  (void)remove("TWBOK.SED");

  /* Hex/oct/dec escapes, [^]a], trailing backslash in a and s/, & d38/d92 */
  (void)sed_test_unescape(dec1, sizeof(dec1), "a~/");
  (void)sed_test_unescape(dec2, sizeof(dec2), "line_after~/");
  args[0] = "-e";
  args[1] = dec1;
  args[2] = "-e";
  args[3] = dec2;
  args[4] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "x~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(dec1, sizeof(dec1), "a~/eof_text~/");
  args[0] = dec1;
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "x~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(dec1, sizeof(dec1), "s/a~/");
  args[0] = dec1;
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(dec1, sizeof(dec1), "s/~/c");
  args[0] = dec1;
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(dec1, sizeof(dec1),
                          "s/[^]a]/~/x79~/x9a~/xbc~/xde~/xfF~/xAB~/xCD~/xEF~/"
                          "xZZ~/o7~/o9~/d38~/d92~/q/");
  args[0] = dec1;
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "b~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  args[0] = "/abc/Ip";
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "ABC~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* 3. debug.c full coverage: NULL guards + all address/escape/cmd forms */
  debug_print_program(NULL);
  debug_print_command(NULL, NULL);

  /* s/a/b/\rX, --posix s/a/\c/, 2-file -f pending_text, mid-line a\ */
  (void)sed_test_unescape(dec1, sizeof(dec1), "s/a/b/~rX");
  args[0] = dec1;
  args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(args, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(dec1, sizeof(dec1), "s/a/~/c/");
  args[0] = "--posix";
  args[1] = dec1;
  args[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "a~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBP1.SED", "a~/~nhi~/b~/"));
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBP2.SED", "more~n"));
  args[0] = "-f";
  args[1] = "TWBP1.SED";
  args[2] = "-f";
  args[3] = "TWBP2.SED";
  args[4] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, wb_run_tilde(args, "x~n", &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TWBP1.SED");
  (void)remove("TWBP2.SED");

  SED_CHECK_INT_EQ(SED_TEST_OK, wb_write_tilde("TWBR.TXT", "r1~n"));
  {
    char dbg_prog[256];
    (void)sed_test_unescape(
        dbg_prog, sizeof(dbg_prog),
        "1~~2p;0~~2p;1,+2p;1,~~2p;$p;/~/a~/f~/r~/t~/v~/n~/~/~/x01~///IMp;"
        "//p;y/ab/AB/;s/~/(a~/)/~/l~/1/;a~/~n~n"
        "e echo hi~nr TWBR.TXT~nR TWBR.TXT~nw TWBW.TXT~nW TWBW.TXT~n"
        "s/a/echo b/epw TWBW.TXT");
    args[0] = "--debug";
    args[1] = dbg_prog;
    args[2] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_run_env("LC_ALL=C", args, "a\nb\n", 4U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_buf != NULL &&
              strstr(cap.out_buf, "SED PROGRAM:") != NULL);
    sed_cmd_capture_free(&cap);
  }
  (void)remove("TWBR.TXT");
  (void)remove("TWBW.TXT");

  return 0;
}

/**
 * @brief Entry point for t_white strict-C89 white-box and fault-injection
 * suite.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector (optional argv[1] = sed binary path).
 * @return 0 if all tests pass, 1 otherwise.
 */
int main(int argc, char **argv) {
  static const struct sed_test_entry entries[] = {
      {"wb_inproc_runner_edges", test_wb_inproc_runner_edges},
      {"wb_utils_fault_injection", test_wb_utils_fault_injection},
      {"wb_execute_and_popen_faults", test_wb_execute_and_popen_faults},
      {"wb_mbcs_regexp_and_symlinks", test_wb_mbcs_regexp_and_symlinks},
      {"wb_compile_and_debug_edges", test_wb_compile_and_debug_edges}};

  if (argc > 1) {
    sed_test_set_binary_path(argv[1]);
  }
  sed_test_set_inproc_runner(sed_inproc_runner);
  return sed_test_run_suite("t_white (White-Box & Fault Injection)", entries,
                            sizeof(entries) / sizeof(entries[0]));
}
