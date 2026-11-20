/**
 * @file test_sed_addresses_cmds.c
 * @brief Strict ISO C89 verification suite (t_cmds) covering every GNU sed 4.10
 * address form and editing command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sed_test_harness.h"
#ifdef SED_DOS_INPROC
#include "sed_test_inproc.h"
#endif

/**
 * @brief Verifies all address forms: N, $, /BRE/, custom delim, first~step,
 * ranges, 0,/re/, addr,+N, addr,~N, !, and nested {...} blocks.
 * @return 0 on pass, 1 on failure.
 */
static int test_all_address_forms(void) {
  static const char *const num_last[] = {"-n", "-e", "2p", "-e", "$p", NULL};
  static const char *const bre_empty[] = {"-n", "/two/p;//p", NULL};
  static const char *const step_mod[] = {"-n", "1~~2p", NULL};
  static const char *const zero_step[] = {"-n", "0~~3p", NULL};
  static const char *const range_num[] = {"-n", "2,4p", NULL};
  static const char *const range_rev[] = {"-n", "3,1p", NULL};
  static const char *const range_zero_re[] = {"-n", "0,/b/p", NULL};
  static const char *const range_plus[] = {"-n", "/b/,+2p", NULL};
  static const char *const range_tilde[] = {"-n", "2,~~3p", NULL};
  static const char *const neg_nested[] = {"-n", "1,4{2,3!{s/^/>/;p;};}", NULL};
  char custom_script[32];
  const char *custom_args[3];
  const char *six = "a~nb~nc~nd~ne~nf~n";

  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(num_last, "one~ntwo~nthree~nfour~n",
                                         "two~nfour~n", 0));

  /* BRE address followed by empty regex reusing previous pattern */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(bre_empty, "one~ntwo~ntwo2~n",
                                         "two~ntwo~ntwo2~ntwo2~n", 0));

  /* Custom regex delimiter via backslash-percent */
  custom_args[0] = "-n";
  custom_args[1] = "~/%a/b%p";
  custom_args[2] = NULL;
  (void)custom_script;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(
                                    custom_args, "x~na/b~ny~n", "a/b~n", 0));

  /* Stepping, ranges, 0,/re/, +N, ~N, !, and nested {...} */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(step_mod, six, "a~nc~ne~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(zero_step, six, "c~nf~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_num, six, "b~nc~nd~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_rev, six, "c~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_zero_re, "b~nc~n", "b~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_zero_re, six, "a~nb~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_plus, six, "b~nc~nd~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(range_tilde, six, "b~nc~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(neg_nested, six, ">a~n>d~n", 0));

  return 0;
}

/**
 * @brief Verifies text-insertion and cycle commands: a, i, c, d, D, n, N, p, P,
 * z, =, and #/#n comments.
 * @return 0 on pass, 1 on failure.
 */
static int test_text_and_cycle_commands(void) {
  static const char *const aic_args[] = {"-e", "1i before1", "-e", "2a after2",
                                         "-e", "3,4c mid34", NULL};
  static const char *const dD_args[] = {"N;s/1/ONE/;P;D", NULL};
  static const char *const n_eq_z[] = {"-e", "1{=;z;}", "-e", "2n", NULL};
  static const char *const hash_n[] = {"#n quiet header~n2p", NULL};
  char hprog[64];
  const char *h_args[2];

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(
                                    aic_args, "L1~nL2~nL3~nL4~nL5~n",
                                    "before1~nL1~nL2~nafter2~nmid34~nL5~n", 0));

  /* Multi-line N + P + D sliding window */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(dD_args, "1~n2~n3~n",
                                                      "ONE~n2~n3~n", 0));

  /* =, z (zap pattern space), and n */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(n_eq_z, "first~nsecond~nthird~n",
                                         "1~n~nsecond~nthird~n", 0));

  /* #n on line 1 enables silent mode; # comments ignored */
  (void)sed_test_unescape(hprog, sizeof(hprog), "#n magic~n# full comment~n2p");
  h_args[0] = hprog;
  h_args[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(h_args, "x~ny~nz~n", "y~n", 0));
  (void)hash_n;

  return 0;
}

/**
 * @brief Verifies hold-space commands (g, G, h, H, x) and control flow
 * (:label, b, t, T, q [code], Q [code], v [ver]).
 * @return 0 on pass, 1 on failure.
 */
static int test_hold_space_and_branching(void) {
  static const char *const rev_lines[] = {"1!G;h;$!d", NULL};
  static const char *const ghx_args[] = {"-n", "1{h;s/a/A/;x;p;g;p;};2{H;x;p;}",
                                         NULL};
  static const char *const btT_args[] = {
      "-e", "s/foo/bar/;t hit;T miss;:hit;s/$/!/;b end;:miss;s/$/?/;:end",
      NULL};
  static const char *const q_code[] = {"2q 7", NULL};
  static const char *const Q_code[] = {"2Q 9", NULL};
  static const char *const v_ok[] = {"v 4.10;s/a/A/", NULL};
  static const char *const v_empty[] = {"v;s/a/B/", NULL};
  static const char *const v_fail[] = {"v 99.0", NULL};
  static const char *const L_obsolete[] = {"L", NULL};
  struct sed_cmd_capture cap;

  /* Classic tac reversal using G, h, d */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(rev_lines, "1~n2~n3~n",
                                                      "3~n2~n1~n", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(ghx_args, "a~nb~n",
                                                      "a~nA~nA~nb~n", 0));

  /* b, t, T, and :label */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(btT_args, "foo~nbaz~n",
                                                      "bar!~nbaz?~n", 0));

  /* q [code] prints line 2 before exiting with 7 */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(q_code, "L1~nL2~nL3~n",
                                                      "L1~nL2~n", 7));

  /* Q [code] quits silently BEFORE printing line 2, exiting with 9 */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(Q_code, "L1~nL2~nL3~n", "L1~n", 9));

  /* v command version checks */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(v_ok, "a~n", "A~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(v_empty, "a~n", "B~n", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(v_fail, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  /* Obsolete L command is rejected with EXIT_BAD_USAGE (1) in sed 4.10 */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(L_obsolete, "a", 1U, &cap));
  SED_CHECK_INT_EQ(1, cap.exit_code);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Verifies file & diagnostic commands: F, l, r, R, w, W, and 0r.
 * @return 0 on pass, 1 on failure.
 */
static int test_file_io_and_list_commands(void) {
  static const char *const F_args[] = {"-n", "1F", "TFNAME.TXT", NULL};
  static const char *const l_args[] = {"-n", "l 40", NULL};
  static const char *const r_args[] = {"1r TREAD.TXT", "0r TREAD0.TXT",
                                       "TFNAME.TXT", NULL};
  static const char *const R_args[] = {"R TRLINE.TXT", NULL};
  static const char *const wW_args[] = {"-n", "N;w TWALL.TXT~nW TWONE.TXT",
                                        NULL};
  char raw[64];
  char wprog[64];
  const char *w_dyn[3];
  char *out = NULL;
  size_t rlen;
  size_t olen = 0U;

  rlen = sed_test_unescape(raw, sizeof(raw), "in1~nin2~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TFNAME.TXT", raw, rlen));

  /* F prints current input filename */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(F_args, NULL, "TFNAME.TXT~n", 0));

  /* l formats control bytes (BEL, TAB, backslash) plus trailing '$' */
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(l_args, "~a~t~/~n",
                                                      "~/a~/t~/~/$~n", 0));

  /* r and 0r (plus nonexistent r file silently ignored) */
  rlen = sed_test_unescape(raw, sizeof(raw), "HDR~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TREAD0.TXT", raw, rlen));
  rlen = sed_test_unescape(raw, sizeof(raw), "INS~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TREAD.TXT", raw, rlen));
  {
    static const char *const r2_args[] = {
        "-e", "0r TREAD0.TXT", "-e",         "1r TREAD.TXT",
        "-e", "2r NOMISS.TXT", "TFNAME.TXT", NULL};
    SED_CHECK_INT_EQ(
        SED_TEST_OK,
        sed_test_expect_tilde(r2_args, NULL, "HDR~nin1~nINS~nin2~n", 0));
  }
  (void)r_args;

  /* R reads one line per cycle */
  rlen = sed_test_unescape(raw, sizeof(raw), "R1~nR2~n");
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_write_file("TRLINE.TXT", raw, rlen));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(R_args, "A~nB~nC~n",
                                                      "A~nR1~nB~nR2~nC~n", 0));

  /* w writes full multi-line pattern space; W writes only first line */
  (void)wW_args;
  (void)sed_test_unescape(wprog, sizeof(wprog), "N;w TWALL.TXT~nW TWONE.TXT");
  w_dyn[0] = "-n";
  w_dyn[1] = wprog;
  w_dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(w_dyn, "P1~nP2~n", "", 0));

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TWALL.TXT", &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  rlen = sed_test_unescape(raw, sizeof(raw), "P1~nP2~n");
  SED_CHECK_STR_EQ(raw, out);
  free(out);
  out = NULL;

  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TWONE.TXT", &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  rlen = sed_test_unescape(raw, sizeof(raw), "P1~n");
  SED_CHECK_STR_EQ(raw, out);
  free(out);

  (void)remove("TFNAME.TXT");
  (void)remove("TREAD0.TXT");
  (void)remove("TREAD.TXT");
  (void)remove("TRLINE.TXT");
  (void)remove("TWALL.TXT");
  (void)remove("TWONE.TXT");
  return 0;
}

/**
 * @brief Verifies s/// flags (g, p, i/I, m/M, w, Nth, 1..9 backrefs, &,
 * \L\U\l\u\E), y/// transliteration, and e / s///e shell execution.
 * @return 0 on pass, 1 on failure.
 */
static int test_subst_translit_and_exec(void) {
  static const char *const s_nth[] = {"s/a/A/2;s/b/B/2g", NULL};
  static const char *const s_icase[] = {"-n", "s/foo/BAR/Ip", NULL};
  static const char *const s_mline[] = {"N;s/^b/B/m;s/a$/A/M", NULL};
  static const char *const s_wfile[] = {"-n", "s/hit/HIT/w TSW.TXT", NULL};
  static const char *const y_ascii[] = {"y/abc/XYZ/", NULL};
  static const char *const e_arg[] = {"-n", "1{e echo sed_e_ok~n}", NULL};
  char s9_prog[128];
  char case_prog[64];
  char e1_prog[64];
  char e2_prog[64];
  const char *dyn[3];
  struct sed_cmd_capture cap;
  char *out = NULL;
  size_t olen = 0U;

  /* Nth match and Nth+g match */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(s_nth, "aaa bbb~n", "aAa bBB~n", 0));

  /* Case-insensitive i/I + p flag */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(s_icase, "FoO~nno~n", "BAR~n", 0));

  /* Multi-line m/M anchors inside N pattern space */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(s_mline, "a~nb~n", "A~nB~n", 0));

  /* s command with w file flag */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(s_wfile, "miss~nhit~n", "", 0));
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_read_file("TSW.TXT", &out, &olen));
  olen = sed_test_normalize_crlf(out, olen);
  out[olen] = (char)0;
  SED_CHECK(strstr(out, "HIT") != NULL);
  free(out);
  (void)remove("TSW.TXT");

  /* All 9 backreferences \1..\9 + & under -E */
  (void)sed_test_unescape(
      s9_prog, sizeof(s9_prog),
      "s/(a)(b)(c)(d)(e)(f)(g)(h)(i)/~/9~/8~/7~/6~/5~/4~/3~/2~/1:&/");
  dyn[0] = "-E";
  dyn[1] = s9_prog;
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(
      SED_TEST_OK,
      sed_test_expect_tilde(dyn, "abcdefghi~n", "ihgfedcba:abcdefghi~n", 0));

  /* Case conversions \U \L \u \l \E */
  (void)sed_test_unescape(
      case_prog, sizeof(case_prog),
      "s/(ab) (CD) (ef) (GH)/~/U~/1~/E ~/L~/2~/E ~/u~/3 ~/l~/4/");
  dyn[0] = "-E";
  dyn[1] = case_prog;
  dyn[2] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_expect_tilde(dyn, "ab CD ef GH~n",
                                                      "AB cd Ef gH~n", 0));

  /* Transliteration y single-byte and UTF-8 multibyte */
  SED_CHECK_INT_EQ(SED_TEST_OK,
                   sed_test_expect_tilde(y_ascii, "cab~n", "ZXY~n", 0));
  if (y_ascii[0] != NULL) {
    char mb_prog[16];
    char mb_in[16];
    const char *mb_args[2];
    /* Transliterate e-acute (0xc3 0xa9) to E-acute (0xc3 0x89) */
    mb_prog[0] = 'y';
    mb_prog[1] = '/';
    ((unsigned char *)mb_prog)[2] = 0xc3U;
    ((unsigned char *)mb_prog)[3] = 0xa9U;
    mb_prog[4] = '/';
    ((unsigned char *)mb_prog)[5] = 0xc3U;
    ((unsigned char *)mb_prog)[6] = 0x89U;
    mb_prog[7] = '/';
    mb_prog[8] = (char)0;
    ((unsigned char *)mb_in)[0] = 0xc3U;
    ((unsigned char *)mb_in)[1] = 0xa9U;
    mb_in[2] = (char)10;
    mb_args[0] = mb_prog;
    mb_args[1] = NULL;
    SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run_env("LC_ALL=en_US.UTF-8",
                                                   mb_args, mb_in, 3U, &cap));
    SED_CHECK_INT_EQ(0, cap.exit_code);
    SED_CHECK(cap.out_len >= 2U && (unsigned char)cap.out_buf[0] == 0xc3U &&
              (unsigned char)cap.out_buf[1] == 0x89U);
    sed_cmd_capture_free(&cap);
  }

  /* Standalone 'e <cmd>', pattern-space 'e', and subst 'e' flag */
  (void)e_arg;
  (void)sed_test_unescape(e1_prog, sizeof(e1_prog), "e echo sed_e1");
  dyn[0] = e1_prog;
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "in", 2U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "sed_e1") != NULL);
  sed_cmd_capture_free(&cap);

  dyn[0] = "e";
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "echo sed_ep", 11U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "sed_ep") != NULL);
  sed_cmd_capture_free(&cap);

  (void)sed_test_unescape(e2_prog, sizeof(e2_prog), "s/.*/echo sed_e2/e");
  dyn[0] = e2_prog;
  dyn[1] = NULL;
  SED_CHECK_INT_EQ(SED_TEST_OK, sed_test_run(dyn, "in", 2U, &cap));
  SED_CHECK_INT_EQ(0, cap.exit_code);
  SED_CHECK(cap.out_buf != NULL && strstr(cap.out_buf, "sed_e2") != NULL);
  sed_cmd_capture_free(&cap);

  return 0;
}

/**
 * @brief Entry point for t_cmds strict-C89 addresses & commands suite.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector (optional argv[1] = sed binary path).
 * @return 0 if all tests pass, 1 otherwise.
 */
int main(int argc, char **argv) {
  static const struct sed_test_entry entries[] = {
      {"all_address_forms", test_all_address_forms},
      {"text_and_cycle_commands", test_text_and_cycle_commands},
      {"hold_space_and_branching", test_hold_space_and_branching},
      {"file_io_and_list_commands", test_file_io_and_list_commands},
      {"subst_translit_and_exec", test_subst_translit_and_exec}};

  if (argc > 1) {
    sed_test_set_binary_path(argv[1]);
  }
#ifdef SED_DOS_INPROC
  sed_test_set_inproc_runner(sed_inproc_runner);
#endif
  return sed_test_run_suite("t_cmds (Addresses & Commands)", entries,
                            sizeof(entries) / sizeof(entries[0]));
}
