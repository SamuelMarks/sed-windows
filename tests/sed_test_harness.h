/**
 * @file sed_test_harness.h
 * @brief Strict ISO C89 cross-platform test runner, process spawner, and
 * SHA-256 verifier.
 */

#ifndef SED_TEST_HARNESS_H
#define SED_TEST_HARNESS_H

#include "sed_test_error.h"
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Captured stdout, stderr, and exit status from a single sed invocation.
 */
struct sed_cmd_capture {
  char *out_buf;         /**< Heap-allocated NUL-terminated stdout bytes. */
  size_t out_len;        /**< Exact byte length of stdout payload. */
  char *err_buf;         /**< Heap-allocated NUL-terminated stderr bytes. */
  size_t err_len;        /**< Exact byte length of stderr payload. */
  int exit_code;         /**< Normalized process exit code. */
  enum sed_test_err err; /**< Transport status (SED_TEST_OK on success). */
};

/**
 * @brief Callback signature for an individual strict-C89 test case.
 * @return 0 on pass, non-zero on failure.
 */
typedef int (*sed_test_fn)(void);

/**
 * @brief Table entry registering a named C89 test case.
 */
struct sed_test_entry {
  const char *name; /**< Test case name printed in suite output. */
  sed_test_fn fn;   /**< Test function pointer. */
};

/**
 * @brief Signature of an optional in-process sed runner callback.
 * @param[in] env_kv Optional "KEY=VALUE" environment override (or NULL).
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] in_path Path to prepared binary stdin scratch file.
 * @param[in] out_path Path to destination stdout scratch file.
 * @param[in] err_path Path to destination stderr scratch file.
 * @param[out] exit_code_out Receives captured exit status code.
 * @return SED_TEST_OK on execution, error code on setup failure.
 */
typedef enum sed_test_err (*sed_inproc_runner_fn)(
    const char *env_kv, const char *const *args, const char *in_path,
    const char *out_path, const char *err_path, int *exit_code_out);

/**
 * @brief Configures the path to the sed binary under test (from argv[1]).
 * @param[in] path Filesystem path to sed / sed.exe (may be NULL).
 */
void sed_test_set_binary_path(const char *path);

/**
 * @brief Registers or clears an optional in-process sed runner callback.
 * @param[in] runner Callback used by sed_test_run_env (NULL restores spawn).
 */
void sed_test_set_inproc_runner(sed_inproc_runner_fn runner);

/**
 * @brief Normalizes CRLF ("\r\n") sequences to LF ("\n") in-place.
 * @param[in,out] buf Mutable byte buffer (may be NULL if len == 0).
 * @param[in] len Byte length of buf prior to normalization.
 * @return New byte length after collapsing "\r\n" pairs to "\n".
 */
size_t sed_test_normalize_crlf(char *buf, size_t len);

/**
 * @brief Expands tilde escapes (~n LF, ~r CR, ~t TAB, ~a BEL, ~f FF, ~v VT,
 * ~0 NUL, ~/ backslash, ~~ literal ~) into binary bytes.
 * @param[out] dst Output buffer (NUL-terminated when cap > 0).
 * @param[in] cap Capacity of dst in bytes.
 * @param[in] spec NUL-terminated tilde-encoded specification string.
 * @return Number of decoded bytes written (excluding trailing NUL).
 */
size_t sed_test_unescape(char *dst, size_t cap, const char *spec);

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
                                        int expected_exit);

/**
 * @brief Returns the configured sed binary path (never NULL).
 * @return Path string to sed executable.
 */
const char *sed_test_get_binary_path(void);

/**
 * @brief Releases heap memory held by a sed_cmd_capture structure.
 * @param[in,out] cap Capture object to reset (ignored if NULL).
 */
void sed_cmd_capture_free(struct sed_cmd_capture *cap);

/**
 * @brief Writes exact binary bytes to a file on disk.
 * @param[in] path Destination file path.
 * @param[in] data Source byte buffer (may be NULL when len == 0).
 * @param[in] len Number of bytes to write.
 * @return SED_TEST_OK on success, error code otherwise.
 */
enum sed_test_err sed_test_write_file(const char *path, const char *data,
                                      size_t len);

/**
 * @brief Reads an entire file into a NUL-terminated binary heap buffer.
 * @param[in] path Source file path.
 * @param[out] out_buf Receives malloc'd buffer (caller frees with free()).
 * @param[out] out_len Receives exact byte length read.
 * @return SED_TEST_OK on success, error code otherwise.
 */
enum sed_test_err sed_test_read_file(const char *path, char **out_buf,
                                     size_t *out_len);

/**
 * @brief Spawns sed with an explicit NULL-terminated argument list and stdin.
 * @param[in] args NULL-terminated array of arguments AFTER argv[0].
 * @param[in] stdin_data Raw bytes fed to child stdin (NULL if stdin_len == 0).
 * @param[in] stdin_len Number of bytes in stdin_data.
 * @param[out] cap Populated with captured stdout, stderr, and exit_code.
 * @return SED_TEST_OK if child ran and outputs were collected.
 */
enum sed_test_err sed_test_run(const char *const *args, const char *stdin_data,
                               size_t stdin_len, struct sed_cmd_capture *cap);

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
                                   struct sed_cmd_capture *cap);

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
                                         int expected_exit);

/**
 * @brief Computes lowercase 64-character FIPS 180-4 SHA-256 hex digest.
 * @param[in] data Input byte buffer (may be NULL if len == 0).
 * @param[in] len Byte length of data.
 * @param[out] out_hex Destination buffer of at least 65 bytes.
 * @return SED_TEST_OK on success, SED_TEST_ERR_INVALID_ARG if out_hex is NULL.
 */
enum sed_test_err sed_sha256_mem(const unsigned char *data, size_t len,
                                 char out_hex[65]);

/**
 * @brief Computes lowercase 64-character SHA-256 hex digest of a disk file.
 * @param[in] path Path to input file.
 * @param[out] out_hex Destination buffer of at least 65 bytes.
 * @return SED_TEST_OK on success, error code on invalid argument or I/O error.
 */
enum sed_test_err sed_sha256_file(const char *path, char out_hex[65]);

/**
 * @brief Logs a failed assertion and returns 1 for test propagation.
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @param[in] expr Expression text.
 * @return Always 1.
 */
int sed_test_fail(const char *file, int line, const char *expr);

/**
 * @brief Verifies integer equality and logs diagnostic details on mismatch.
 * @param[in] expected Expected integer value.
 * @param[in] actual Actual integer value.
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @return 0 when equal, 1 on mismatch.
 */
int sed_test_check_int_eq(long expected, long actual, const char *file,
                          int line);

/**
 * @brief Verifies NUL-terminated string equality and logs diagnostics.
 * @param[in] expected Expected string (non-NULL).
 * @param[in] actual Actual string (non-NULL).
 * @param[in] file Source filename (__FILE__).
 * @param[in] line 1-based line number (__LINE__).
 * @return 0 when equal, 1 on mismatch.
 */
int sed_test_check_str_eq(const char *expected, const char *actual,
                          const char *file, int line);

/**
 * @brief Executes all entries in a test suite and prints pass/fail summary.
 * @param[in] suite_name Human-readable suite title.
 * @param[in] entries Array of test descriptors.
 * @param[in] count Number of elements in entries.
 * @return 0 when every test passes, 1 otherwise.
 */
int sed_test_run_suite(const char *suite_name,
                       const struct sed_test_entry *entries, size_t count);

/** @brief Fails current test case if boolean condition is zero. */
#define SED_CHECK(cond)                                                        \
  if (!(cond))                                                                 \
  return sed_test_fail(__FILE__, __LINE__, #cond)

/** @brief Fails current test case if two integer values differ. */
#define SED_CHECK_INT_EQ(exp, act)                                             \
  if (sed_test_check_int_eq((long)(exp), (long)(act), __FILE__, __LINE__) !=   \
      0)                                                                       \
  return 1

/** @brief Fails current test case if two strings differ. */
#define SED_CHECK_STR_EQ(exp, act)                                             \
  if (sed_test_check_str_eq((exp), (act), __FILE__, __LINE__) != 0)            \
  return 1

#ifdef __cplusplus
}
#endif

#endif /* SED_TEST_HARNESS_H */
