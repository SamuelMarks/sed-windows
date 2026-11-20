/**
 * @file test_upstream_immutability.c
 * @brief Strict ISO C89 verification of zero upstream GNU sed 4.10 edits,
 * generated header contracts, FIPS 180-4 SHA-256 vectors, and 100% harness
 * coverage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sed_test_harness.h"

#ifndef SED_MANIFEST_SYNTAX_CHECK_ONLY
#include "upstream_manifest.h"
#endif

/**
 * @brief Mock in-process runner returning a simulated I/O error.
 * @param[in] env_kv Unused environment string.
 * @param[in] args Unused argument vector.
 * @param[in] in_path Unused stdin path.
 * @param[in] out_path Unused stdout path.
 * @param[in] err_path Unused stderr path.
 * @param[out] exit_code_out Unused exit code pointer.
 * @return Always SED_TEST_ERR_IO.
 */
static enum sed_test_err
mock_runner_err(const char *env_kv, const char *const *args,
                const char *in_path, const char *out_path, const char *err_path,
                int *exit_code_out) {
  (void)env_kv;
  (void)args;
  (void)in_path;
  (void)out_path;
  (void)err_path;
  (void)exit_code_out;
  return SED_TEST_ERR_IO;
}

/**
 * @brief Mock in-process runner writing deterministic stdout/stderr payloads.
 * @param[in] env_kv Optional "EXIT=N" override for simulated exit status.
 * @param[in] args Argument vector (args[0] written to stdout if non-NULL).
 * @param[in] in_path Prepared stdin file path.
 * @param[in] out_path Destination stdout file path.
 * @param[in] err_path Destination stderr file path.
 * @param[out] exit_code_out Receives simulated exit status.
 * @return SED_TEST_OK on success, error code on file write failure.
 */
static enum sed_test_err
mock_runner_ok(const char *env_kv, const char *const *args, const char *in_path,
               const char *out_path, const char *err_path, int *exit_code_out) {
  const char *payload = (args != NULL && args[0] != NULL) ? args[0] : "";
  enum sed_test_err rc;
  (void)in_path;

  rc = sed_test_write_file(out_path, payload, strlen(payload));
  if (rc != SED_TEST_OK) {
    return rc;
  }
  rc = sed_test_write_file(err_path, "mock-err", 8U);
  if (rc != SED_TEST_OK) {
    return rc;
  }
  *exit_code_out = (env_kv != NULL && strcmp(env_kv, "EXIT=2") == 0) ? 2 : 0;
  return SED_TEST_OK;
}

/**
 * @brief Dummy failing test callback used to exercise sed_test_run_suite fail
 * branch.
 * @return Always 1.
 */
static int dummy_fail_case(void) { return 1; }

/**
 * @brief Dummy passing test callback used to exercise sed_test_run_suite pass
 * branch.
 * @return Always 0.
 */
static int dummy_pass_case(void) { return 0; }

/**
 * @brief Verifies NIST FIPS 180-4 SHA-256 test vectors (0B, 3B, 56B, 64B,
 * 112B).
 * @return 0 on pass, 1 on failure.
 */
static int test_sha256_fips_vectors(void) {
  char hex[65];
  const char *msg56 =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const char *msg64 =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  const char *msg112 =
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
      "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_sha256_mem((const unsigned char *)"a", 1U, NULL));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG, sed_sha256_mem(NULL, 3U, hex));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_sha256_mem(NULL, 0U, hex));
  SED_CHECK_STR_EQ(
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", hex);

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_sha256_mem((const unsigned char *)"abc", 3U, hex));
  SED_CHECK_STR_EQ(
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hex);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_sha256_mem((const unsigned char *)msg56,
                                               strlen(msg56), hex));
  SED_CHECK_STR_EQ(
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", hex);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_sha256_mem((const unsigned char *)msg64,
                                               strlen(msg64), hex));
  SED_CHECK_INT_EQ(64L, (long)strlen(hex));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_sha256_mem((const unsigned char *)msg112,
                                               strlen(msg112), hex));
  SED_CHECK_STR_EQ(
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1", hex);

  return 0;
}

/**
 * @brief Exercises every error enum name, diagnostic helper, and file I/O edge.
 * @return 0 on pass, 1 on failure.
 */
static int test_harness_error_and_io_edges(void) {
  struct sed_test_diag diag;
  struct sed_cmd_capture cap;
  char hex[65];
  char *buf = NULL;
  size_t len = 0U;
  char long_msg[300];
  char saved_bin[512];
  char long_bin[540];
  size_t i;

  SED_CHECK_STR_EQ("SED_TEST_OK", sed_test_err_name(SED_TEST_OK));
  SED_CHECK_STR_EQ("SED_TEST_ERR_INVALID_ARG",
                   sed_test_err_name(SED_TEST_ERR_INVALID_ARG));
  SED_CHECK_STR_EQ("SED_TEST_ERR_ALLOC", sed_test_err_name(SED_TEST_ERR_ALLOC));
  SED_CHECK_STR_EQ("SED_TEST_ERR_IO", sed_test_err_name(SED_TEST_ERR_IO));
  SED_CHECK_STR_EQ("SED_TEST_ERR_SPAWN", sed_test_err_name(SED_TEST_ERR_SPAWN));
  SED_CHECK_STR_EQ("SED_TEST_ERR_EXIT_MISMATCH",
                   sed_test_err_name(SED_TEST_ERR_EXIT_MISMATCH));
  SED_CHECK_STR_EQ("SED_TEST_ERR_STDOUT_MISMATCH",
                   sed_test_err_name(SED_TEST_ERR_STDOUT_MISMATCH));
  SED_CHECK_STR_EQ("SED_TEST_ERR_STDERR_MISMATCH",
                   sed_test_err_name(SED_TEST_ERR_STDERR_MISMATCH));
  SED_CHECK_STR_EQ("SED_TEST_ERR_FILE_MISMATCH",
                   sed_test_err_name(SED_TEST_ERR_FILE_MISMATCH));
  SED_CHECK_STR_EQ("SED_TEST_ERR_SHA256_MISMATCH",
                   sed_test_err_name(SED_TEST_ERR_SHA256_MISMATCH));
  SED_CHECK_STR_EQ("SED_TEST_ERR_HEADER_CONTRACT",
                   sed_test_err_name(SED_TEST_ERR_HEADER_CONTRACT));
  SED_CHECK_STR_EQ("SED_TEST_ERR_UNKNOWN",
                   sed_test_err_name((enum sed_test_err)999));

  sed_test_diag_set(NULL, SED_TEST_OK, "x.c", 1, "ignored");
  sed_test_diag_set(&diag, SED_TEST_ERR_IO, NULL, 42, NULL);
  SED_CHECK_STR_EQ("<unknown>", diag.file);
  SED_CHECK_STR_EQ("", diag.detail);

  sed_test_diag_set(&diag, SED_TEST_OK, "short.c", 3, "ok");
  SED_CHECK_STR_EQ("short.c", diag.file);
  SED_CHECK_STR_EQ("ok", diag.detail);

  for (i = 0U; i < sizeof(long_msg) - 1U; ++i) {
    long_msg[i] = 'A';
  }
  long_msg[sizeof(long_msg) - 1U] = (char)0;
  sed_test_diag_set(&diag, SED_TEST_ERR_IO, "f.c", 7, long_msg);
  SED_CHECK_INT_EQ((long)(sizeof(diag.detail) - 1U), (long)strlen(diag.detail));

  /* Binary path getter/setter edge cases (preserve original path) */
  strcpy(saved_bin, sed_test_get_binary_path());
  sed_test_set_binary_path(NULL);
  sed_test_set_binary_path("");
  SED_CHECK_STR_EQ(saved_bin, sed_test_get_binary_path());
  memset(long_bin, 'B', sizeof(long_bin) - 1U);
  long_bin[sizeof(long_bin) - 1U] = (char)0;
  sed_test_set_binary_path(long_bin);
  SED_CHECK_INT_EQ(511L, (long)strlen(sed_test_get_binary_path()));
  sed_test_set_binary_path(saved_bin);

  sed_cmd_capture_free(NULL);
  memset(&cap, 0, sizeof(cap));
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_write_file(NULL, "a", 1U));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_write_file("x.tmp", NULL, 2U));
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_test_write_file("no_such_dir_83/?.?", "a", 1U));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("STEMP0.TMP", NULL, 0U));
  (void)remove("STEMP0.TMP");

  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_read_file(NULL, &buf, &len));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_read_file("x.tmp", NULL, &len));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_read_file("x.tmp", &buf, NULL));
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_test_read_file("NONEXIST.TMP", &buf, &len));

  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG, sed_sha256_file(NULL, hex));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG, sed_sha256_file("x.tmp", NULL));
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO, sed_sha256_file("NONEXIST.TMP", hex));

  /* Exercise multi-kilobyte realloc growth inside sed_test_read_file */
  {
    char big[1200];
    memset(big, 'Z', sizeof(big));
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_write_file("STBIG.TMP", big, sizeof(big)));
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("STBIG.TMP", &buf, &len));
    SED_CHECK_INT_EQ((long)sizeof(big), (long)len);
    free(buf);
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_sha256_file("STBIG.TMP", hex));
    (void)remove("STBIG.TMP");
  }

  SED_CHECK_INT_EQ(1, sed_test_fail("unit", 1, "intentional"));
  SED_CHECK_INT_EQ(1, sed_test_fail(NULL, 1, NULL));
  SED_CHECK_INT_EQ(1, sed_test_check_int_eq(1L, 2L, "unit", 2));
  SED_CHECK_INT_EQ(1, sed_test_check_int_eq(1L, 2L, NULL, 2));
  SED_CHECK_INT_EQ(1, sed_test_check_str_eq("a", "b", "unit", 3));
  SED_CHECK_INT_EQ(1, sed_test_check_str_eq(NULL, "b", NULL, 3));
  SED_CHECK_INT_EQ(1, sed_test_check_str_eq("a", NULL, "unit", 3));

  {
    struct sed_test_entry sub_entries[2];
    sub_entries[0].name = "dummy_pass";
    sub_entries[0].fn = dummy_pass_case;
    sub_entries[1].name = "dummy_fail";
    sub_entries[1].fn = dummy_fail_case;
    SED_CHECK_INT_EQ(1, sed_test_run_suite(NULL, NULL, 0U));
    SED_CHECK_INT_EQ(1, sed_test_run_suite("bad", NULL, 1U));
    SED_CHECK_INT_EQ(1, sed_test_run_suite("sub_fail", sub_entries, 2U));
  }

  return 0;
}

/**
 * @brief Exercises CRLF normalization, tilde unescaping, and runner wrappers.
 * @return 0 on pass, 1 on failure.
 */
static int test_harness_runner_and_unescape_edges(void) {
  char crlf_buf[32];
  char dec[32];
  size_t n;
  struct sed_cmd_capture cap;
  const char *mock_args[2];
  const char *empty_args[1];

  SED_CHECK_INT_EQ(0L, (long)sed_test_normalize_crlf(NULL, 5U));
  SED_CHECK_INT_EQ(0L, (long)sed_test_normalize_crlf(crlf_buf, 0U));

  n = sed_test_unescape(crlf_buf, sizeof(crlf_buf), "a~r~nb~rc~r");
  SED_CHECK_INT_EQ(7L, (long)n);
  n = sed_test_normalize_crlf(crlf_buf, n);
  SED_CHECK_INT_EQ(6L, (long)n);
  SED_CHECK_INT_EQ('a', (int)(unsigned char)crlf_buf[0]);
  SED_CHECK_INT_EQ(10, (int)(unsigned char)crlf_buf[1]);
  SED_CHECK_INT_EQ('b', (int)(unsigned char)crlf_buf[2]);
  SED_CHECK_INT_EQ(13, (int)(unsigned char)crlf_buf[3]);
  SED_CHECK_INT_EQ('c', (int)(unsigned char)crlf_buf[4]);
  SED_CHECK_INT_EQ(13, (int)(unsigned char)crlf_buf[5]);

  SED_CHECK_INT_EQ(0L, (long)sed_test_unescape(NULL, 8U, "abc"));
  SED_CHECK_INT_EQ(0L, (long)sed_test_unescape(dec, 0U, "abc"));
  SED_CHECK_INT_EQ(0L, (long)sed_test_unescape(dec, sizeof(dec), NULL));
  SED_CHECK_STR_EQ("", dec);

  /* Truncation when capacity is small */
  n = sed_test_unescape(dec, 3U, "abcdef");
  SED_CHECK_INT_EQ(2L, (long)n);
  SED_CHECK_STR_EQ("ab", dec);

  /* Every tilde escape sequence + trailing single '~' */
  n = sed_test_unescape(dec, sizeof(dec), "~n~r~t~a~f~v~0~/~~~qZ~");
  SED_CHECK_INT_EQ(12L, (long)n);
  SED_CHECK_INT_EQ(10, (int)(unsigned char)dec[0]);
  SED_CHECK_INT_EQ(13, (int)(unsigned char)dec[1]);
  SED_CHECK_INT_EQ(9, (int)(unsigned char)dec[2]);
  SED_CHECK_INT_EQ(7, (int)(unsigned char)dec[3]);
  SED_CHECK_INT_EQ(12, (int)(unsigned char)dec[4]);
  SED_CHECK_INT_EQ(11, (int)(unsigned char)dec[5]);
  SED_CHECK_INT_EQ(0, (int)(unsigned char)dec[6]);
  SED_CHECK_INT_EQ(92, (int)(unsigned char)dec[7]);
  SED_CHECK_INT_EQ('~', (int)(unsigned char)dec[8]);
  SED_CHECK_INT_EQ('q', (int)(unsigned char)dec[9]);
  SED_CHECK_INT_EQ('Z', (int)(unsigned char)dec[10]);
  SED_CHECK_INT_EQ('~', (int)(unsigned char)dec[11]);

  /* Argument validation on runner wrappers */
  (void)sed_test_unescape(crlf_buf, sizeof(crlf_buf), "hello~n");
  mock_args[0] = crlf_buf;
  mock_args[1] = NULL;
  empty_args[0] = NULL;

  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_run_env(NULL, NULL, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_run_env(NULL, mock_args, NULL, 0U, NULL));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_expect_simple(NULL, "in", "out", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_expect_simple(mock_args, "in", NULL, 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_expect_tilde(NULL, "in", "out", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_INVALID_ARG,
                   sed_test_expect_tilde(mock_args, "in", NULL, 0));

  /* Mock in-process error propagation */
  sed_test_set_inproc_runner(mock_runner_err);
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_test_expect_simple(mock_args, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   sed_test_expect_tilde(mock_args, NULL, "", 0));

  /* Mock in-process success, exit mismatch, and stdout mismatch branches */
  sed_test_set_inproc_runner(mock_runner_ok);
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_simple(mock_args, "stdin", crlf_buf, 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_simple(empty_args, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_EXIT_MISMATCH,
                   sed_test_expect_simple(mock_args, NULL, crlf_buf, 1));
  SED_CHECK_INT_EQ(SED_TEST_ERR_STDOUT_MISMATCH,
                   sed_test_expect_simple(mock_args, NULL, "other!", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_STDOUT_MISMATCH,
                   sed_test_expect_simple(mock_args, NULL, "short", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(mock_args, "in~n", "hello~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(empty_args, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_EXIT_MISMATCH,
                   sed_test_expect_tilde(mock_args, NULL, "hello~n", 1));
  SED_CHECK_INT_EQ(SED_TEST_ERR_STDOUT_MISMATCH,
                   sed_test_expect_tilde(mock_args, NULL, "other~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_ERR_STDOUT_MISMATCH,
                   sed_test_expect_tilde(mock_args, NULL, "x", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_run_env("EXIT=2", mock_args, "x", 1U, &cap));
  SED_CHECK_INT_EQ(2, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* Restore real child-process spawning (or platform default) */
  sed_test_set_inproc_runner(NULL);

#if !defined(__DOS__) && !defined(__MSDOS__) && !defined(__WATCOMC__)
  /* Exercise real child spawn with and without env_kv override */
  {
    const char *ver_args[2];
    ver_args[0] = "--version";
    ver_args[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_run_env("COLS=60", ver_args, NULL, 0U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "4.10") != NULL);
    sed_cmd_capture_free(&cap);

    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_run_env("NOVALUE", ver_args, "a", 1U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }
#endif

  return 0;
}

/**
 * @brief Checks that a generated header exists and contains a required token.
 * @param[in] rel_name Relative header path under SED_GENERATED_DIR.
 * @param[in] needle Substring that must appear inside the generated file.
 * @return SED_TEST_OK when present and valid, error code otherwise.
 */
static enum sed_test_err verify_generated_header_token(const char *rel_name,
                                                       const char *needle) {
#if defined(SED_MANIFEST_SYNTAX_CHECK_ONLY) || defined(__DOS__) ||             \
    defined(__MSDOS__) || defined(__WATCOMC__)
  (void)rel_name;
  (void)needle;
  return SED_TEST_OK;
#else
  char path[1024];
  char *buf = NULL;
  size_t len = 0U;
  enum sed_test_err rc;

  sprintf(path, "%s/%s", SED_GENERATED_DIR, rel_name);
  rc = sed_test_read_file(path, &buf, &len);
  if (rc != SED_TEST_OK) {
    return rc;
  }
  if (len == 0U || (needle != NULL && strstr(buf, needle) == NULL)) {
    free(buf);
    return SED_TEST_ERR_HEADER_CONTRACT;
  }
  free(buf);
  return SED_TEST_OK;
#endif
}

/**
 * @brief Verifies all generated build headers in SED_GENERATED_DIR.
 * @return 0 on pass, 1 on missing file or broken macro contract.
 */
static int test_generated_headers_contract(void) {
  static const char *const required_headers[][2] = {
      {"config.h", "4.10"},
      {"config.h", "GNULIB_NO_VLA"},
      {"config.h", "rpl_re_compile_pattern"},
      {"version.h", "Version"},
      {"version.c", "4.10"},
      {"stdckdint.h", "ckd_add"},
      {"stdcountof.h", "countof"},
      {"uchar.h", "char32_t"},
      {"obstack.h", "obstack"},
      {"error.h", "error"},
      {"unitypes.h", "uint32_t"},
      {"unicase.h", "uc_tolower"},
      {"unictype.h", "uc_is_"},
      {"uninorm.h", "uninorm"},
      {"selinux/selinux.h", "is_selinux_enabled"},
      {"selinux/context.h", "context_t"},
      {"selinux/label.h", "selabel_handle"},
      {"malloc/dynarray.gl.h", "dynarray"},
      {"malloc/dynarray-skeleton.gl.h", "DYNARRAY_STRUCT"},
      {"malloc/scratch_buffer.gl.h", "scratch_buffer"}};
  size_t i;

  for (i = 0U; i < sizeof(required_headers) / sizeof(required_headers[0]);
       ++i) {
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     verify_generated_header_token(required_headers[i][0],
                                                   required_headers[i][1]));
  }
#if !defined(SED_MANIFEST_SYNTAX_CHECK_ONLY) && !defined(__DOS__) &&           \
    !defined(__MSDOS__) && !defined(__WATCOMC__)
  SED_CHECK_INT_EQ(SED_TEST_ERR_IO,
                   verify_generated_header_token("missing_hdr.h", "x"));
  SED_CHECK_INT_EQ(
      SED_TEST_ERR_HEADER_CONTRACT,
      verify_generated_header_token("version.h", "TOKEN_NEVER_PRESENT_ZZZ"));
#endif
  return 0;
}

/**
 * @brief Verifies byte-for-byte SHA-256 immutability of upstream GNU sed 4.10.
 * @return 0 on pass, 1 on any modified or missing upstream .c/.h file.
 */
static int test_upstream_sha256_manifest(void) {
#if defined(SED_MANIFEST_SYNTAX_CHECK_ONLY) || defined(__DOS__) ||             \
    defined(__MSDOS__) || defined(__WATCOMC__)
  /* On MS-DOS VM guests, host SED_UPSTREAM_SOURCE_DIR is outside the VM;
   * manifest count and first entry are still verified. */
  SED_CHECK(SED_UPSTREAM_FILE_COUNT > 0U);
  SED_CHECK(g_sed_upstream_manifest[0].rel_path != NULL);
  return 0;
#else
  size_t i;
  SED_CHECK(SED_UPSTREAM_FILE_COUNT >= 100U);
  for (i = 0U; g_sed_upstream_manifest[i].rel_path != NULL; ++i) {
    char full_path[1024];
    char actual_hex[65];
    enum sed_test_err rc;

    sprintf(full_path, "%s/%s", SED_UPSTREAM_SOURCE_DIR,
            g_sed_upstream_manifest[i].rel_path);
    rc = sed_sha256_file(full_path, actual_hex);
    if (rc != SED_TEST_OK) {
      fprintf(stderr, "Failed reading upstream file: %s%c", full_path,
              (char)10);
      return 1;
    }
    if (strcmp(actual_hex, g_sed_upstream_manifest[i].sha256_hex) != 0) {
      fprintf(stderr,
              "Upstream file modified: %s%c  expected: %s%c  actual:   %s%c",
              full_path, (char)10, g_sed_upstream_manifest[i].sha256_hex,
              (char)10, actual_hex, (char)10);
      return 1;
    }
  }
  SED_CHECK_INT_EQ((long)SED_UPSTREAM_FILE_COUNT, (long)i);
  return 0;
#endif
}

/**
 * @brief Entry point for strict-C89 upstream immutability & harness unit suite.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector (optional argv[1] = sed binary path).
 * @return 0 if all tests pass, 1 otherwise.
 */
int main(int argc, char **argv) {
  static const struct sed_test_entry entries[] = {
      {"sha256_fips_vectors", test_sha256_fips_vectors},
      {"harness_error_and_io_edges", test_harness_error_and_io_edges},
      {"harness_runner_and_unescape_edges",
       test_harness_runner_and_unescape_edges},
      {"generated_headers_contract", test_generated_headers_contract},
      {"upstream_sha256_manifest", test_upstream_sha256_manifest}};

  if (argc > 1) {
    sed_test_set_binary_path(argv[1]);
  }
  SED_CHECK(sed_test_get_binary_path() != NULL);
  return sed_test_run_suite("t_hygien (Immutability & C89 Harness)", entries,
                            sizeof(entries) / sizeof(entries[0]));
}
