/**
 * @file sed_test_inproc.h
 * @brief Strict C89 in-process libsed_testable runner and fault-injection
 * controller.
 */
#ifndef SED_TEST_INPROC_H
#define SED_TEST_INPROC_H

#include <stddef.h>

#include "sed_test_harness.h"
#include "sed_test_hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Deterministic fault-injection configuration for libsed_testable.
 */
struct sed_fault_config {
  int fail_fwrite_after; /**< Countdown (>=1) to short-write failure; 0 off. */
  int fail_fread_after;  /**< Countdown (>=1) to fread error; 0 off. */
  int fail_fflush_after; /**< Countdown (>=1) to fflush EOF; 0 off. */
  int fail_fflush_errno; /**< Errno set when fail_fflush_after triggers. */
  int force_ferror;      /**< Non-zero forces sed_hook_ferror() != 0. */
  int fail_fclose_after; /**< Countdown (>=1) to fclose EOF; 0 off. */
  int fail_mkostemp;     /**< Non-zero forces sed_hook_mkostemp() == -1. */
  int fail_rename;       /**< Non-zero forces sed_hook_rename() == -1. */
  int force_mbrtowc_ret; /**< If != 0, forced return value of mbrtowc(). */
  int force_isatty;      /**< Non-zero forces sed_hook_isatty() == 1. */
  int fail_popen;        /**< Non-zero forces sed_hook_popen() == NULL. */
};

/**
 * @brief Resets all fault-injection counters and flags to disabled defaults.
 */
void sed_fault_reset(void);

/**
 * @brief Returns a mutable pointer to the active fault-injection configuration.
 * @return Non-NULL pointer to global sed_fault_config state.
 */
struct sed_fault_config *sed_fault_state(void);

/**
 * @brief Entry point of unmodified GNU sed 4.10 compiled into libsed_testable.
 * @param[in] argc Argument count.
 * @param[in] argv Mutable argument vector.
 * @return Process exit code when returning normally.
 */
int sed_testable_main(int argc, char **argv);

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
                                    const char *err_path, int *exit_code_out);

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
                                       int *exit_code_out);

#ifdef __cplusplus
}
#endif

#endif /* SED_TEST_INPROC_H */
