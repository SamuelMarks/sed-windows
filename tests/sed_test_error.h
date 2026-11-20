/**
 * @file sed_test_error.h
 * @brief Unified strict-C89 error enumeration and diagnostic types for
 * sed-windows tests.
 */

#ifndef SED_TEST_ERROR_H
#define SED_TEST_ERROR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Canonical error codes returned across all sed-windows C89 test
 * utilities.
 */
enum sed_test_err {
  SED_TEST_OK = 0,                  /**< Operation succeeded. */
  SED_TEST_ERR_INVALID_ARG = 1,     /**< NULL or out-of-range argument. */
  SED_TEST_ERR_ALLOC = 2,           /**< Heap allocation failed. */
  SED_TEST_ERR_IO = 3,              /**< File or descriptor I/O failed. */
  SED_TEST_ERR_SPAWN = 4,           /**< Child process creation failed. */
  SED_TEST_ERR_EXIT_MISMATCH = 5,   /**< Unexpected child exit status. */
  SED_TEST_ERR_STDOUT_MISMATCH = 6, /**< Captured stdout differed. */
  SED_TEST_ERR_STDERR_MISMATCH = 7, /**< Captured stderr differed. */
  SED_TEST_ERR_FILE_MISMATCH = 8,   /**< Output file contents differed. */
  SED_TEST_ERR_SHA256_MISMATCH = 9, /**< Upstream SHA-256 check failed. */
  SED_TEST_ERR_HEADER_CONTRACT = 10 /**< Generated header check failed. */
};

/**
 * @brief Structured diagnostic record pairing an error code with source
 * context.
 */
struct sed_test_diag {
  enum sed_test_err code; /**< Canonical error code. */
  const char *file;       /**< Source file where failure originated. */
  int line;               /**< 1-based source line number. */
  char detail[256];       /**< Human-readable diagnostic message. */
};

/**
 * @brief Converts a sed_test_err enumerator to a static human-readable name.
 * @param[in] err Error code to describe.
 * @return Non-NULL constant string naming the error code.
 */
const char *sed_test_err_name(enum sed_test_err err);

/**
 * @brief Populates a diagnostic record with code, location, and message text.
 * @param[out] diag Destination diagnostic structure (ignored if NULL).
 * @param[in] code Error code to store.
 * @param[in] file Source filename string.
 * @param[in] line 1-based source line number.
 * @param[in] msg Optional message text (may be NULL).
 */
void sed_test_diag_set(struct sed_test_diag *diag, enum sed_test_err code,
                       const char *file, int line, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* SED_TEST_ERROR_H */
