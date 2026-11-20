/**
 * @file test_sed_cli_options.c
 * @brief Strict ISO C89 end-to-end and in-process CLI option verification suite
 * (t_cli) for GNU sed 4.10.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(__DOS__) && !defined(__MSDOS__) &&            \
    !defined(__WATCOMC__)
#include <unistd.h>
#endif

#include "sed_test_harness.h"
#include "sed_test_inproc.h"

/**
 * @brief Verifies --version and --help output banners and exit status 0.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_version_and_help(void) {
  static const char *const ver_args[] = {"--version", NULL};
  static const char *const help_args[] = {"--help", NULL};
  struct sed_cmd_capture cap;

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ver_args, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL);
  SED_CHECK(strstr(cap.out_buf, "GNU sed) 4.10") != NULL);
  SED_CHECK(strstr(cap.out_buf, "SELinux") != NULL);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(help_args, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL);
  SED_CHECK(strstr(cap.out_buf, "Usage:") != NULL);
  SED_CHECK(strstr(cap.out_buf, "--sandbox") != NULL);
  SED_CHECK(strstr(cap.out_buf, "bug-sed@gnu.org") != NULL);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Verifies -n, --quiet, --silent, -e, --expression, and -f / --file.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_quiet_expr_and_file(void) {
  static const char *const q1[] = {"-n", "2p", NULL};
  static const char *const q2[] = {"--quiet", "2p", NULL};
  static const char *const q3[] = {"--silent", "2p", NULL};
  static const char *const multi_e[] = {"-e", "s/a/A/", "--expression=s/b/B/",
                                        NULL};
  static const char *const f_args[] = {"-f", "TCLI.SED", NULL};
  static const char *const flong_args[] = {"--file=TCLI.SED", NULL};
  static const char *const fstdin_args[] = {"-f", "-", "TCLIIN.TXT", NULL};
  char prog_buf[64];
  char in_buf[64];
  size_t plen;
  size_t ilen;

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(q1, "one~ntwo~nthree~n", "two~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(q2, "one~ntwo~nthree~n", "two~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(q3, "one~ntwo~nthree~n", "two~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(multi_e, "ab~n", "AB~n", 0));

  plen = sed_test_unescape(prog_buf, sizeof(prog_buf), "s/hello/world/g~n");
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_write_file("TCLI.SED", prog_buf, plen));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(f_args, "hello!~n", "world!~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(flong_args, "hello!~n",
                                                      "world!~n", 0));

  ilen = sed_test_unescape(in_buf, sizeof(in_buf), "hello stdin-prog~n");
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_write_file("TCLIIN.TXT", in_buf, ilen));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(fstdin_args, "s/hello/HI/~n",
                                         "HI stdin-prog~n", 0));

  (void)remove("TCLI.SED");
  (void)remove("TCLIIN.TXT");
  return 0;
}

/**
 * @brief Verifies -i (no backup), -i.bak, and -i"orig_*" in-place editing.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_in_place_modes(void) {
  static const char *const ip_nobak[] = {"-i", "s/old/new/g", "TIP1.TXT", NULL};
#if defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
  static const char *const ip_bak[] = {"-i.BAK", "s/new/fin/g", "TIP1", NULL};
  const char *bak_target = "TIP1";
  const char *bak_file = "TIP1.BAK";
#else
  static const char *const ip_bak[] = {"-i.bak", "s/new/fin/g", "TIP1.TXT",
                                       NULL};
  const char *bak_target = "TIP1.TXT";
  const char *bak_file = "TIP1.TXT.bak";
#endif
  static const char *const ip_star[] = {"-iB_*", "s/alpha/beta/", "TIP2.TXT",
                                        NULL};
  char raw[64];
  char *out = NULL;
  size_t rlen;
  size_t olen = 0U;

  rlen = sed_test_unescape(raw, sizeof(raw), "old line~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TIP1.TXT", raw, rlen));

  /* 1. -i without backup suffix */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ip_nobak, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TIP1.TXT", &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  rlen = sed_test_unescape(raw, sizeof(raw), "new line~n");
  SED_CHECK_STR_EQ(raw, out);
  if (strcmp(bak_target, "TIP1.TXT") != 0) {
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file(bak_target, out, olen));
  }
  free(out);
  out = NULL;

  /* 2. -i.bak overwrite existing backup check (run twice to test overwrite!) */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ip_bak, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ip_bak, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file(bak_target, &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  rlen = sed_test_unescape(raw, sizeof(raw), "fin line~n");
  SED_CHECK_STR_EQ(raw, out);
  free(out);
  out = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file(bak_file, &out, &olen));
  free(out);
  out = NULL;

  /* 3. -i with explicit '*' wildcard pattern (8.3-compatible B_TIP2.TXT) */
  rlen = sed_test_unescape(raw, sizeof(raw), "alpha~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TIP2.TXT", raw, rlen));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ip_star, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("B_TIP2.TXT", &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  SED_CHECK_STR_EQ(raw, out);
  free(out);

  (void)remove("TIP1.TXT");
  (void)remove(bak_target);
  (void)remove(bak_file);
  (void)remove("TIP2.TXT");
  (void)remove("B_TIP2.TXT");

#if !defined(_WIN32) && !defined(__DOS__) && !defined(__MSDOS__) &&            \
    !defined(__WATCOMC__)
  if (raw[0] != (char)0) {
    static const char *const fs_args[] = {"--follow-symlinks", "-i",
                                          "s/sym/SYM/", "TSYMLNK.TXT", NULL};
    rlen = sed_test_unescape(raw, sizeof(raw), "sym~n");
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_write_file("TSYMTGT.TXT", raw, rlen));
    (void)remove("TSYMLNK.TXT");
    if (symlink("TSYMTGT.TXT", "TSYMLNK.TXT") == 0) {
      SED_CHECK_INT_EQ(SED_TEST_OK,
                       sed_test_expect_tilde(fs_args, NULL, "", 0));
      SED_CHECK_INT_EQ(SED_TEST_OK,
                       sed_test_read_file("TSYMTGT.TXT", &out, &olen));
      out[olen] = (char)0;
      rlen = sed_test_unescape(raw, sizeof(raw), "SYM~n");
      SED_CHECK_STR_EQ(raw, out);
      free(out);
    }
    (void)remove("TSYMLNK.TXT");
    (void)remove("TSYMTGT.TXT");
  }
#endif
  return 0;
}

/**
 * @brief Verifies -b / --binary exact CRLF byte preservation on all platforms.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_binary_crlf_preservation(void) {
  static const char *const bin_args[] = {"-b", "s/foo$/bar/", "TBIN.TXT", NULL};
  static const char *const bin_long[] = {"--binary", "s/foo/baz/", "TBIN.TXT",
                                         NULL};
  struct sed_cmd_capture cap;
  char crlf_in[64];
  char crlf_exp_no_match[64];
  char crlf_exp_match[64];
  size_t ilen;
  size_t elen1;
  size_t elen2;

  ilen = sed_test_unescape(crlf_in, sizeof(crlf_in), "foo~r~n");
  elen1 = sed_test_unescape(crlf_exp_no_match, sizeof(crlf_exp_no_match),
                            "foo~r~n");
  elen2 = sed_test_unescape(crlf_exp_match, sizeof(crlf_exp_match), "baz~r~n");

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TBIN.TXT", crlf_in, ilen));

  /* Under -b, CR is preserved before LF so /foo$/ does NOT match foo+CRLF */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bin_args, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK_INT_EQ((long)elen1, (long)cap.out_len);
  SED_CHECK(memcmp(cap.out_buf, crlf_exp_no_match, elen1) == 0);
  sed_cmd_capture_free(&cap);

  /* And s/foo/baz/ preserves the exact trailing CRLF bytes */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bin_long, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK_INT_EQ((long)elen2, (long)cap.out_len);
  SED_CHECK(memcmp(cap.out_buf, crlf_exp_match, elen2) == 0);
  sed_cmd_capture_free(&cap);

  (void)remove("TBIN.TXT");
  return 0;
}

/**
 * @brief Verifies -E, -r, --regexp-extended, -l, COLS, -s, -u, and -z.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_ere_linelen_separate_nulldata(void) {
  static const char *const ere_E[] = {"-E", "s/(ab)+/X/", NULL};
  static const char *const ere_r[] = {"-r", "s/(ab)+/Y/", NULL};
  static const char *const ere_long[] = {"--regexp-extended", "s/(ab)+/Z/",
                                         NULL};
  static const char *const l_arg[] = {"-n", "-l", "5", "l", NULL};
  static const char *const l_long[] = {"-n", "--line-length=5", "l", NULL};
  static const char *const cols_arg[] = {"-n", "l", NULL};
  static const char *const sep_args[] = {"-n",      "-s",      "$p",
                                         "TS1.TXT", "TS2.TXT", NULL};
  static const char *const nosep_args[] = {"-n", "$p", "TS1.TXT", "TS2.TXT",
                                           NULL};
  static const char *const unbuf_args[] = {"-u", "--unbuffered", "s/x/y/",
                                           NULL};
  static const char *const z_args[] = {"-z", "s/a/A/", NULL};
  static const char *const z_long[] = {"--null-data", "--zero-terminated",
                                       "s/b/B/", NULL};
  struct sed_cmd_capture cap;
  char fbuf[32];
  size_t flen;

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(ere_E, "abab!~n", "X!~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(ere_r, "abab!~n", "Y!~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(ere_long, "abab!~n", "Z!~n", 0));

  /* -l 5 wraps 'l' output at 4 chars + backslash */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(l_arg, "abcdefgh~n",
                                                      "abcd~/~nefgh$~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(l_long, "abcdefgh~n",
                                                      "abcd~/~nefgh$~n", 0));

  /* COLS=6 sets lcmd_out_line_len = 5, COLS=1 is ignored */
  flen = sed_test_unescape(fbuf, sizeof(fbuf), "abcdefgh~n");
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_run_env("COLS=6", cols_arg, fbuf, flen, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "abcd") != NULL);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_run_env("COLS=1", cols_arg, fbuf, flen, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "abcdefgh$") != NULL);
  sed_cmd_capture_free(&cap);

  /* -s / --separate prints last line of EACH file vs overall last line */
  flen = sed_test_unescape(fbuf, sizeof(fbuf), "f1a~nf1b~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TS1.TXT", fbuf, flen));
  flen = sed_test_unescape(fbuf, sizeof(fbuf), "f2a~nf2b~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TS2.TXT", fbuf, flen));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(sep_args, NULL, "f1b~nf2b~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(nosep_args, NULL, "f2b~n", 0));
  (void)remove("TS1.TXT");
  (void)remove("TS2.TXT");

  /* -u / --unbuffered & -z / --null-data */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(unbuf_args, "x~n", "y~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(z_args, "a~0a~0", "A~0A~0", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(z_long, "b~0b~0", "B~0B~0", 0));

  return 0;
}

/**
 * @brief Verifies --posix, POSIXLY_CORRECT, --sandbox, --debug, and error exit
 * codes (EXIT_BAD_USAGE=1, EXIT_BAD_INPUT=2, EXIT_PANIC=4).
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_posix_sandbox_debug_and_errors(void) {
  static const char *const posix_reject_Q[] = {"--posix", "Q", NULL};
  static const char *const env_posix_args[] = {"1q", NULL};
  static const char *const sb_e[] = {"--sandbox", "e", NULL};
  static const char *const sb_se[] = {"--sandbox", "s/a/b/e", NULL};
  static const char *const sb_r[] = {"--sandbox", "r x.txt", NULL};
  static const char *const sb_w[] = {"--sandbox", "w x.txt", NULL};
  static const char *const sb_R[] = {"--sandbox", "R x.txt", NULL};
  static const char *const sb_W[] = {"--sandbox", "W x.txt", NULL};
  static const char *const dbg_args[] = {"--debug", "s/(a)/[~/1]/", NULL};
  static const char *const bad_opt[] = {"--definitely-invalid-opt", NULL};
  static const char *const no_script[] = {"-n", NULL};
  static const char *const bad_input[] = {"p", "NO_SUCH_INPUT_99.TXT", NULL};
  static const char *const bad_f[] = {"-f", "NO_SUCH_PROG_99.SED", NULL};
  static const char *const ip_nofile[] = {"-i", "s/a/b/", NULL};
  struct sed_cmd_capture cap;
  char sbuf[32];
  const char *dbg_dyn[3];
  size_t slen;

  /* --posix disables GNU extension command Q -> EXIT_BAD_USAGE (1) */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(posix_reject_Q, "x", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* POSIXLY_CORRECT environment variable activates POSIXLY_CORRECT mode */
  slen = sed_test_unescape(sbuf, sizeof(sbuf), "hi~n");
  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      sed_test_run_env("POSIXLY_CORRECT=1", env_posix_args, sbuf, slen, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* --sandbox rejects e, subst-e, r, w, R, W with EXIT_BAD_USAGE (1) */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_e, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_se, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_r, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_w, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_R, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(sb_W, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* --debug emits SED PROGRAM, INPUT, PATTERN, MATCHED REGEX REGISTERS */
  (void)dbg_args;
  (void)sed_test_unescape(sbuf, sizeof(sbuf), "s/~/(a~/)/[~/1]/");
  dbg_dyn[0] = "--debug";
  dbg_dyn[1] = sbuf;
  dbg_dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dbg_dyn, "a", 1U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL);
  SED_CHECK(strstr(cap.out_buf, "SED PROGRAM:") != NULL);
  SED_CHECK(strstr(cap.out_buf, "MATCHED REGEX REGISTERS") != NULL);
  sed_cmd_capture_free(&cap);

  /* EXIT_BAD_USAGE (1): unknown option and missing script */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bad_opt, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(no_script, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* EXIT_BAD_INPUT (2): missing input file */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bad_input, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(2, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* EXIT_PANIC (4): missing -f script file and -i without input files */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bad_f, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(ip_nofile, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(4, cap.exit_code);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Verifies in-process CLI execution via libsed_testable re-entrancy.
 * @return 0 on pass, 1 on failure.
 */
static int test_cli_inproc_reentrancy(void) {
  static const char *const a1[] = {"-n", "-e", "s/x/X/p", NULL};
  static const char *const a2[] = {"--sandbox", "w bad.txt", NULL};
  static const char *const a3[] = {"--version", NULL};
  struct sed_cmd_capture cap;

  sed_test_set_inproc_runner(sed_inproc_runner);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(a1, "x~ny~n", "X~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(a2, "x", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(a3, NULL, 0U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "4.10") != NULL);
  sed_cmd_capture_free(&cap);

#if !defined(__DOS__) && !defined(__MSDOS__) && !defined(__WATCOMC__)
  sed_test_set_inproc_runner(NULL);
#endif
  return 0;
}

/**
 * @brief Entry point for t_cli strict-C89 CLI options test suite.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector (optional argv[1] = sed binary path).
 * @return 0 if all tests pass, 1 otherwise.
 */
int main(int argc, char **argv) {
  static const struct sed_test_entry entries[] = {
      {"cli_version_and_help", test_cli_version_and_help},
      {"cli_quiet_expr_and_file", test_cli_quiet_expr_and_file},
      {"cli_in_place_modes", test_cli_in_place_modes},
      {"cli_binary_crlf_preservation", test_cli_binary_crlf_preservation},
      {"cli_ere_linelen_separate_nulldata",
       test_cli_ere_linelen_separate_nulldata},
      {"cli_posix_sandbox_debug_and_errors",
       test_cli_posix_sandbox_debug_and_errors},
      {"cli_inproc_reentrancy", test_cli_inproc_reentrancy}};

  if (argc > 1) {
    sed_test_set_binary_path(argv[1]);
  }
#if defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
  sed_test_set_inproc_runner(sed_inproc_runner);
#endif
  return sed_test_run_suite("t_cli (CLI Options & Exit Codes)", entries,
                            sizeof(entries) / sizeof(entries[0]));
}
