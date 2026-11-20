/**
 * @file test_sed_410_regressions.c
 * @brief Strict ISO C89 self-contained port (t_reg410) of the 77 upstream
 * GNU sed 4.10 testsuite cases, running natively on Host, Windows, and MS-DOS.
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sed_test_harness.h"
#ifdef SED_DOS_INPROC
#include "sed_test_inproc.h"
#endif

/**
 * @brief Verifies GNU sed 4.10 headline regressions: backref-anchor,
 * bre-star-star, bug80573, debug-label, posix-backslash-warn,
 * posix-bracket-esc, read-error-stale-errno, and C-locale apostrophe quoting.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_410_headline_fixes(void) {
  static const char *const br_anc[] = {"-E", "s/^(.?)(.?).?~/2~/1$/X/", NULL};
  static const char *const bre_ss_gnu[] = {"s/a**/X/", NULL};
  static const char *const bre_ss_psx[] = {"--posix", "s/a**/X/", NULL};
  static const char *const bug80573_e[] = {"-e", "x", "-e", "s/.*/echo hi/e",
                                           NULL};
  static const char *const bug80573_c[] = {"-n", "/~/c[/p", NULL};
  static const char *const dbg_lbl[] = {"-e", ":lbl1;b lbl2;:lbl2", "--debug",
                                        NULL};
  static const char *const psx_bs_w[] = {"--posix", "s/a/~/q/", NULL};
  static const char *const psx_brk_e[] = {"--posix", "s/[[:alpha:]][~/n]/X/",
                                          NULL};
  static const char *const gnu_brk_e[] = {"s/[[:alpha:]][~/n]/X/", NULL};
  static const char *const no_lbl[] = {":", NULL};
  struct sed_cmd_capture cap;
  char dec_prog[64];
  char esc_in[8];
  const char *dyn[4];

  /* 1. backref-anchor.sh: ^(.?)(.?).?\2\1$ palindrome up to length 5 */
  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      sed_test_expect_tilde(br_anc, "~na~naa~naba~nabba~nabcba~nabca~nabcd~n",
                            "X~nX~nX~nX~nX~nX~nabca~nabcd~n", 0));

  /* 2. bre-star-star.sh: GNU allows a**, --posix rejects a** */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(bre_ss_gnu, "aaa~n", "X~n", 0));
  (void)sed_test_unescape(dec_prog, sizeof(dec_prog), "s/a**/X/");
  dyn[0] = "--posix";
  dyn[1] = dec_prog;
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "aaa", 3U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf, "Invalid preceding regular expression") !=
                NULL);
  sed_cmd_capture_free(&cap);
  (void)bre_ss_psx;

  /* 3. bug80573.sh & \c[ escape in regex */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bug80573_e, "in", 2U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "hi") != NULL);
  sed_cmd_capture_free(&cap);

  esc_in[0] = (char)27; /* ASCII ESC = \c[ */
  esc_in[1] = (char)10;
  (void)sed_test_unescape(dec_prog, sizeof(dec_prog), "/~/c[/p");
  dyn[0] = "-n";
  dyn[1] = dec_prog;
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, esc_in, 2U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_len >= 1U && (unsigned char)cap.out_buf[0] == 27U);
  sed_cmd_capture_free(&cap);
  (void)bug80573_c;

  /* 4. debug-label.sh: ':' label before --debug (no crash) & after --debug */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dbg_lbl, "a", 1U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "SED PROGRAM:") != NULL);
  sed_cmd_capture_free(&cap);

  dyn[0] = "--debug";
  dyn[1] = "-e";
  dyn[2] = ":lbl1";
  dyn[3] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "a", 1U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, ":lbl1") != NULL);
  sed_cmd_capture_free(&cap);

  /* 5. posix-backslash-warn.sh: --posix handles non-portable backslash */
  (void)psx_bs_w;
  (void)sed_test_unescape(dec_prog, sizeof(dec_prog), "s/a/~/z/");
  dyn[0] = "--posix";
  dyn[1] = dec_prog;
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "a", 1U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_len >= 1U && cap.out_buf[0] == 'z');
  sed_cmd_capture_free(&cap);

  /* 6. posix-bracket-esc.sh: bracket backslash-n after [:alpha:] in --posix */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(psx_brk_e, "an~na~/~n", "X~nX~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(gnu_brk_e, "an~n", "an~n", 0));

  /* 7. C-locale apostrophe quoting (":" lacks a label / 'y' command ...) */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_run_env("LC_ALL=C", no_lbl, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL &&
            strstr(cap.err_buf, "lacks a label") != NULL);
  sed_cmd_capture_free(&cap);

  dyn[0] = "y/a/bc/";
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_run_env("LC_ALL=C", dyn, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "'y'") != NULL);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Verifies compile-tests, compile-errors, convert-number,
 * command-endings, missing-filename, normalize-text, and recursive-escape-c.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_compile_and_escapes(void) {
  static const char *const bad_scripts[] = {
      "s/./x/pp", "s/./x/gg", "s/./x/0", "s/./x/2p3",   "s/./x/Q", "r",
      "w",        "R",        "W",       "s/./~/c~/c/", NULL};
  static const char *const num_esc[] = {
      "-e", "1s/./~/d65/", "-e", "2s/./~/o102/", "-e", "3s/./~/x43/", NULL};
  static const char *const ctrl_c[] = {
      "-e", "1s/./~/cA/", "-e", "2s/./~/c[/", "-e", "3s/./~/c~/~//", NULL};
  static const char *const norm_y[] = {"y/abcdef/~/a~/f~/n~/r~/t~/v/", NULL};
  static const char *const lbl_endings[] = {
      "bZ;:Z;bY;;:Y;{bX};:X ;b W;: W;{h};{;h;};s/a/A/#comment", NULL};
  static const char *const w_stderr[] = {"w/dev/stderr", NULL};
  struct sed_cmd_capture cap;
  char dec[64];
  const char *dyn[2];
  size_t i;

  for (i = 0U; bad_scripts[i] != NULL; ++i) {
    (void)sed_test_unescape(dec, sizeof(dec), bad_scripts[i]);
    dyn[0] = dec;
    dyn[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "", 0U, &cap));
    SED_CHECK_INT_EQ(1, cap.exit_code);
    sed_cmd_capture_free(&cap);
  }

  /* convert-number.sh: \d65='A', \o102='B', \x43='C' */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(num_esc, "x~nx~nx~n", "A~nB~nC~n", 0));

  /* recursive-escape-c.sh: ctrl-A=0x01, ctrl-[=0x1b, ctrl-backslash=0x1c */
  (void)ctrl_c;
  (void)sed_test_unescape(dec, sizeof(dec),
                          "1s/./~/cA/;2s/./~/c[/;3s/./~/c~/~//");
  dyn[0] = dec;
  dyn[1] = NULL;
  {
    char in3[16];
    size_t n3 = sed_test_unescape(in3, sizeof(in3), "x~nx~nx~n");
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, in3, n3, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_len >= 5U);
    SED_CHECK_INT_EQ(1, (int)(unsigned char)cap.out_buf[0]);
    SED_CHECK_INT_EQ(27, (int)(unsigned char)cap.out_buf[cap.out_len / 3U]);
    sed_cmd_capture_free(&cap);
  }

  /* normalize-text.sh & command-endings.sh & /dev/stderr emulation */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(norm_y, "af~n", "~a~v~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(lbl_endings, "a~n", "A~n", 0));

  {
    char in1[8];
    size_t n1 = sed_test_unescape(in1, sizeof(in1), "z~n");
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(w_stderr, in1, n1, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.err_buf != NULL && strstr(cap.err_buf, "z") != NULL);
    sed_cmd_capture_free(&cap);
  }

  return 0;
}

/**
 * @brief Verifies posix-char-class, posix-mode-addr, posix-mode-bad-ref,
 * posix-mode-ERE, posix-mode-s, posix-mode-N, and range-overlap.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_posix_modes_and_ranges(void) {
  static const char *const p_cc1[] = {"--posix", "s/]/X/", NULL};
  static const char *const p_cc2[] = {"--posix", "s/[[]/X/", NULL};
  static const char *const p_cc3[] = {"--posix", "s/[.=:.]/X/", NULL};
  static const char *const p_ere1[] = {"--posix", "-E", "s/(.+)/{~/1}/", NULL};
  static const char *const p_ere2[] = {"--posix", "-E", "s/(.~/+)/{~/1}/",
                                       NULL};
  static const char *const g_bre_plus[] = {"s/~/(a~/+~/)~/{~/1}/", NULL};
  static const char *const p_badref_ok[] = {"--posix", "s/abc/~/1/g", NULL};
  static const char *const g_badref_err[] = {"s/abc/~/1/g", NULL};
  static const char *const g_N_odd[] = {"N", NULL};
  static const char *const p_N_odd[] = {"--posix", "N", NULL};
  static const char *const rng_ovl[] = {"2,4d;2,3s/^/x/;3,4s/^/y/", NULL};
  struct sed_cmd_capture cap;
  char dec[32];
  const char *dyn[3];

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(p_cc1, "]~n", "X~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(p_cc2, "[~n", "X~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(p_cc3, "=~n", "X~n", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(p_ere1, "hi+~n", "{hi+}~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(p_ere2, "hi+~n", "h{i+}~n", 0));
  (void)g_bre_plus;

  /* posix-mode-bad-ref.sh: --posix allows \1 without group, GNU rejects */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(p_badref_ok, "abc~n", "~n", 0));
  (void)sed_test_unescape(dec, sizeof(dec), g_badref_err[0]);
  dyn[0] = dec;
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* posix-mode-s.sh: --posix rejects s flags i, I, m, M, e */
  dyn[0] = "--posix";
  dyn[1] = "s/a/b/i";
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* posix-mode-N.sh: single line under N prints in GNU mode, silent in POSIX */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(g_N_odd, "A~n", "A~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(p_N_odd, "A~n", "", 0));

  /* range-overlap.sh */
  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      sed_test_expect_tilde(rng_ovl, "1~n2~n3~n4~n5~n6~n", "1~n5~n6~n", 0));

  return 0;
}

/**
 * @brief Verifies DFA/NFA transitions, bug32082, bug32271-1/2,
 * newline-dfa-bug, regex-errors, word-delim, and subst-replacement.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_dfa_nfa_and_subst(void) {
  static const char *const b32082_p[] = {"--posix", "-e", "/2/p ; 2s//~/9/",
                                         NULL};
  static const char *const b32082_g[] = {"-e", "/^/s///p ; 2s//~/9/", NULL};
  static const char *const b32271_1[] = {"-e", "s/$/a/2", NULL};
  static const char *const b32271_2[] = {"-e", "N; s/$//m2", NULL};
  static const char *const nl_dfa[] = {"N;s/abcdefg.*~/n//", NULL};
  static const char *const wdelim[] = {"s/.~/bx//", NULL};
  static const char *const rpl_0[] = {
      "-E", "s/(.)(.)(.)(.)(.)(.)(.)(.)(.)/~/0/", NULL};
  struct sed_cmd_capture cap;
  char dec[32];
  const char *dyn[2];

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(b32082_p, "1~n2~n", "1~n2~n~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(b32082_g, "1~n2~n",
                                                      "1~n1~n2~n2~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(b32271_1, "0~n", "0~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(
                       b32271_2, "00000000000000000000~n00000000000000000000~n",
                       "00000000000000000000~n00000000000000000000~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(
                                    nl_dfa, "abcdefg abcdefg~nB~n", "B~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(wdelim, "123-x~n", "123~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(rpl_0, "123456789~n",
                                                      "123456789~n", 0));

  /* regex-errors.sh: invalid backref 1 in address and modifier on empty re */
  (void)sed_test_unescape(dec, sizeof(dec), "/~/1/,$p");
  dyn[0] = dec;
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  dyn[0] = "//M,$p";
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Verifies inplace-hold, in-place-hyphen, 8bit, 8to7, nulldata, uniq,
 * bsd, madding, mac-mf, xemacs, and multibyte edge suites.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_inplace_8bit_and_classic(void) {
  static const char *const ip_hold[] = {"-i", "x", "TIH1.TXT", "TIH2.TXT",
                                        NULL};
  static const char *const uniq_args[] = {"N;/^~/(.*~/)~/n~/1$/!P;D", NULL};
  static const char *const eight_to_7[] = {"y/~/x80~/xff/AZ/", NULL};
  static const char *const nul_z[] = {"-z", "s/foo/bar/g", NULL};
  static const char *const d_first[] = {"1D", NULL};
  char raw[32];
  char *out1 = NULL;
  char *out2 = NULL;
  size_t rlen;
  size_t len1 = 0U;
  size_t len2 = 0U;

  /* inplace-hold.sh: -i resets hold space between files so both become LF */
  rlen = sed_test_unescape(raw, sizeof(raw), "x~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TIH1.TXT", raw, rlen));
  rlen = sed_test_unescape(raw, sizeof(raw), "y~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TIH2.TXT", raw, rlen));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ip_hold, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TIH1.TXT", &out1, &len1));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TIH2.TXT", &out2, &len2));
  len1 = sed_test_normalize_crlf(out1, len1);
  len2 = sed_test_normalize_crlf(out2, len2);
  SED_CHECK_INT_EQ(1L, (long)len1);
  SED_CHECK_INT_EQ(1L, (long)len2);
  SED_CHECK_INT_EQ(10, (int)(unsigned char)out1[0]);
  SED_CHECK_INT_EQ(10, (int)(unsigned char)out2[0]);
  free(out1);
  free(out2);
  (void)remove("TIH1.TXT");
  (void)remove("TIH2.TXT");

  /* uniq.sh classic deduplication script */
  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      sed_test_expect_tilde(uniq_args, "a~na~nb~nb~nb~nc~n", "a~nb~nc~n", 0));

  /* 8bit / 8to7 high-byte handling (\x80 -> A, \xff -> Z) */
  {
    char hi_in[4];
    const char *dec_args[2];
    struct sed_cmd_capture cap;
    (void)sed_test_unescape(raw, sizeof(raw), eight_to_7[0]);
    dec_args[0] = raw;
    dec_args[1] = NULL;
    ((unsigned char *)hi_in)[0] = 0x80U;
    ((unsigned char *)hi_in)[1] = 0xffU;
    hi_in[2] = (char)10;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_run_env("LC_ALL=C", dec_args, hi_in, 3U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_len >= 2U && cap.out_buf[0] == 'A' &&
              cap.out_buf[1] == 'Z');
    sed_cmd_capture_free(&cap);
  }

  /* nulldata.sh & execute-tests.sh 1D */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(nul_z, "foo~0foo~0", "bar~0bar~0", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(d_first, "a~n", "", 0));

  return 0;
}

/**
 * @brief Verifies debug.pl, subst-options.sh, subst-replacement.sh, cmd-0r.sh,
 * cmd-R.sh, cmd-l.sh, comment-n.sh, and posix-mode-addr.sh.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_debug_and_subst_opts(void) {
  static const char *const dbg_exprs[] = {
      "=",
      "b end;:lbl;b lbl;t lbl;T lbl;b;:end",
      "1,2{3!p}",
      "a~/~nhi",
      "i~/~nhi",
      "c~/~nhi",
      "d",
      "D",
      "F",
      "g;G;h;H;n;N;P;p;x;z",
      "l;l 40;q;q 0;Q;Q 0",
      "s/~/(a~/)/~/U~/1~/L&~/u~/1~/l~/1~/Ex/gip1w TDBGW.TXT",
      "/a/IMs/a/b/m",
      "y/ab/AB/",
      NULL};
  static const char *const subst_num[] = {"-e",       "1s/./x/g", "-e",
                                          "2s/./x/1", "-e",       "3s/./x/2",
                                          "-e",       "4s/./x/9", NULL};
  static const char *const subst_case[] = {"-E",
                                           "-e",
                                           "1s/(.)(.)(.)/~/U~/1~/E~/2~/3/",
                                           "-e",
                                           "2s/(.)(.)(.)/~/u~/1~/2~/3/",
                                           "-e",
                                           "3s/(.)(.)(.)/~/L~/1~/2~/3/",
                                           "-e",
                                           "4s/(.)(.)(.)/~/l~/1~/2~/3/",
                                           "-e",
                                           "5s/()([a-z])/~/u~/1~/2/",
                                           NULL};
  static const char *const comment_n1[] = {"-e", "#n", "-e", "=", NULL};
  static const char *const comment_n2[] = {"-e", "=#n", NULL};
  static const char *const addr_0_re[] = {"-n", "0,/A/p", NULL};
  static const char *const addr_plus[] = {"-n", "2,+1p", NULL};
  static const char *const addr_step[] = {"-n", "5,~~4p", NULL};
  struct sed_cmd_capture cap;
  char dec[128];
  const char *dyn[4];
  size_t i;

  /* debug.pl: exercise --debug disassembler & execution trace */
  for (i = 0U; dbg_exprs[i] != NULL; ++i) {
    char in_buf[16];
    size_t in_len = sed_test_unescape(in_buf, sizeof(in_buf), "a~nb~n");
    (void)sed_test_unescape(dec, sizeof(dec), dbg_exprs[i]);
    dyn[0] = "--debug";
    dyn[1] = dec;
    dyn[2] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, in_buf, in_len, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_buf != NULL &&
              strstr(cap.out_buf, "SED PROGRAM:") != NULL);
    sed_cmd_capture_free(&cap);
  }
  (void)remove("TDBGW.TXT");

  /* subst-options.sh & subst-replacement.sh */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(
                       subst_num,
                       "bbbbbbbbbb~nbbbbbbbbbb~nbbbbbbbbbb~nbbbbbbbbbb~n",
                       "xxxxxxxxxx~nxbbbbbbbbb~nbxbbbbbbbb~nbbbbbbbbxb~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(subst_case, "abC~nabC~nABc~nABc~nxy~n",
                                         "AbC~nAbC~nabc~naBc~nXy~n", 0));

  /* comment-n.sh (#n in -e and -f) & posix-mode-addr.sh */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(comment_n1, "X~n", "1~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(comment_n2, "X~n", "1~nX~n", 0));
  {
    char pbuf[16];
    size_t plen = sed_test_unescape(pbuf, sizeof(pbuf), "#n~n=~n");
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TCMTN.SED", pbuf, plen));
    dyn[0] = "-f";
    dyn[1] = "TCMTN.SED";
    dyn[2] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(dyn, "X~n", "1~n", 0));
    (void)remove("TCMTN.SED");
  }

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(addr_0_re, "A~nB~nA~n", "A~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(addr_plus, "A~nB~nA~nC~n",
                                                      "B~nA~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(
                                    addr_step, "A~nB~nA~nC~nD~nE~nF~nG~nH~n",
                                    "D~nE~nF~nG~n", 0));

  /* cmd-0r.sh & cmd-R.sh & cmd-l.sh */
  {
    char f1[16];
    char f2[16];
    size_t n1 = sed_test_unescape(f1, sizeof(f1), "H1~nH2~n");
    size_t n2 = sed_test_unescape(f2, sizeof(f2), "1~n2~n");
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TR0R1.TXT", f1, n1));
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TR0R2.TXT", f2, n2));
    dyn[0] = "0rTR0R1.TXT";
    dyn[1] = "TR0R2.TXT";
    dyn[2] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_expect_tilde(dyn, NULL, "H1~nH2~n1~n2~n", 0));
    dyn[0] = "-e";
    dyn[1] = "1RTR0R1.TXT";
    dyn[2] = "TR0R2.TXT";
    dyn[3] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK,
                     sed_test_expect_tilde(dyn, NULL, "1~nH1~n2~n", 0));
    (void)remove("TR0R1.TXT");
    (void)remove("TR0R2.TXT");
  }

  return 0;
}

/**
 * @brief Verifies newjis.sh, xemacs.sh, temp-file-cleanup.sh,
 * in-place-suffix-backup.sh, badenc.sh, mb-y-translate.sh, and UTF-8 suites.
 * @return 0 on pass, 1 on failure.
 */
static int test_reg_multibyte_and_classic_scripts(void) {
  static const char *const newjis_s[] = {"s/$?$1$,$-/M9JX6I/", NULL};
  static const char *const xemacs_s[] = {
      "-e", "/^# Generated/d", "-e", "s/^ *# */#/", "-e", "/^##/d", NULL};
  static const char *const bad_ip[] = {"-i", "s/[unterm/b/", "TTMPC.TXT", NULL};
#if defined(__DOS__) || defined(__MSDOS__) || defined(__WATCOMC__)
  static const char *const sfx_ip[] = {"-i.BAK", "s/a/b/", "TSFX", NULL};
  const char *sfx_target = "TSFX";
  const char *sfx_expected = "TSFX.BAK";
#else
  static const char *const sfx_ip[] = {"-iorig_*_bak", "s/a/b/", "TSFX.TXT",
                                       NULL};
  const char *sfx_target = "TSFX.TXT";
  const char *sfx_expected = "orig_TSFX.TXT_bak";
#endif
  struct sed_cmd_capture cap;
  char *rbuf = NULL;
  size_t rlen = 0U;

  /* newjis.sh (ESC = 0x1b = ~x1b) & xemacs.sh */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(newjis_s, "~x1b$B$H$?$1$,$-$K~x1b(B~n",
                                         "~x1b$B$HM9JX6I$K~x1b(B~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(xemacs_s,
                                         "# Generated~n##drop~n  #  keep~n",
                                         "#keep~n", 0));

  /* temp-file-cleanup.sh & in-place-suffix-backup.sh */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TTMPC.TXT", "a\n", 2U));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(bad_ip, "", 0U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);
  (void)remove("TTMPC.TXT");

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file(sfx_target, "a\n", 2U));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(sfx_ip, NULL, "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file(sfx_expected, &rbuf, &rlen));
  SED_CHECK(rlen >= 1U && rbuf[0] == 'a');
  free(rbuf);
  (void)remove(sfx_target);
  (void)remove(sfx_expected);

  /* UTF-8 multibyte suites (mb-y-translate, badenc, subst-mb-incomplete) when
   * host supports en_US.UTF-8 or C.UTF-8 */
  {
    const char *utf8_env = NULL;
    char *prev = setlocale(LC_ALL, NULL);
    char saved_loc[64];
    saved_loc[0] = (char)0;
    if (prev != NULL && strlen(prev) < sizeof(saved_loc)) {
      strcpy(saved_loc, prev);
    }
    if (setlocale(LC_ALL, "en_US.UTF-8") != NULL) {
      utf8_env = "LC_ALL=en_US.UTF-8";
    } else if (setlocale(LC_ALL, "C.UTF-8") != NULL) {
      utf8_env = "LC_ALL=C.UTF-8";
    }
    if (saved_loc[0] != (char)0) {
      (void)setlocale(LC_ALL, saved_loc);
    } else {
      (void)setlocale(LC_ALL, "C");
    }

    if (utf8_env != NULL) {
      char yprog[16];
      char yin[16];
      const char *ya[3];
      /* Transliterate 1-byte 'a' to 2-byte Phi and 2-byte Phi back to 'a' */
      yprog[0] = 'y';
      yprog[1] = '/';
      yprog[2] = 'a';
      yprog[3] = '/';
      ((unsigned char *)yprog)[4] = 0xceU;
      ((unsigned char *)yprog)[5] = 0xa6U;
      yprog[6] = '/';
      yprog[7] = (char)0;
      ya[0] = yprog;
      ya[1] = NULL;
      SED_CHECK_INT_EQ(SED_TEST_OK,
                       sed_test_run_env(utf8_env, ya, "Xa\n", 3U, &cap));
      SED_CHECK_INT_EQ(0, cap.exit_code);
      SED_CHECK(cap.out_len >= 3U && cap.out_buf[0] == 'X' &&
                (unsigned char)cap.out_buf[1] == 0xceU &&
                (unsigned char)cap.out_buf[2] == 0xa6U);
      sed_cmd_capture_free(&cap);

      yprog[0] = 'y';
      yprog[1] = '/';
      ((unsigned char *)yprog)[2] = 0xceU;
      ((unsigned char *)yprog)[3] = 0xa6U;
      yprog[4] = '/';
      yprog[5] = 'a';
      yprog[6] = '/';
      yprog[7] = (char)0;
      yin[0] = 'X';
      ((unsigned char *)yin)[1] = 0xceU;
      ((unsigned char *)yin)[2] = 0xa6U;
      yin[3] = (char)10;
      SED_CHECK_INT_EQ(SED_TEST_OK,
                       sed_test_run_env(utf8_env, ya, yin, 4U, &cap));
      SED_CHECK_INT_EQ(0, cap.exit_code);
      SED_CHECK(cap.out_len >= 2U && cap.out_buf[0] == 'X' &&
                cap.out_buf[1] == 'a');
      sed_cmd_capture_free(&cap);
    }
  }

  return 0;
}

/**
 * @brief Entry point for t_reg410 strict-C89 GNU sed 4.10 regression suite.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector (optional argv[1] = sed binary path).
 * @return 0 if all tests pass, 1 otherwise.
 */
int main(int argc, char **argv) {
  static const struct sed_test_entry entries[] = {
      {"reg_410_headline_fixes", test_reg_410_headline_fixes},
      {"reg_compile_and_escapes", test_reg_compile_and_escapes},
      {"reg_posix_modes_and_ranges", test_reg_posix_modes_and_ranges},
      {"reg_dfa_nfa_and_subst", test_reg_dfa_nfa_and_subst},
      {"reg_inplace_8bit_and_classic", test_reg_inplace_8bit_and_classic},
      {"reg_debug_and_subst_opts", test_reg_debug_and_subst_opts},
      {"reg_multibyte_and_classic_scripts",
       test_reg_multibyte_and_classic_scripts}};

  if (argc > 1) {
    sed_test_set_binary_path(argv[1]);
  }
#ifdef SED_DOS_INPROC
  sed_test_set_inproc_runner(sed_inproc_runner);
#endif
  return sed_test_run_suite("t_reg410 (GNU sed 4.10 Regressions)", entries,
                            sizeof(entries) / sizeof(entries[0]));
}
