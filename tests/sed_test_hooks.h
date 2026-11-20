/**
 * @file sed_test_hooks.h
 * @brief Strict C89 fault-injection and re-entrancy hooks compiled only into
 * libsed_testable.
 */
#ifndef SED_TEST_HOOKS_H
#define SED_TEST_HOOKS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
/** @brief Marks exit hook as non-returning under GCC and Clang. */
#define SED_HOOK_NORETURN __attribute__((__noreturn__))
#else
/** @brief Expands to empty when compiler lacks GNU noreturn attribute. */
#define SED_HOOK_NORETURN
#endif

/**
 * @brief Intercepts exit() inside libsed_testable for in-process white-box
 * tests.
 * @param[in] status Process exit status passed to exit().
 */
void sed_hook_exit(int status) SED_HOOK_NORETURN;

/**
 * @brief Intercepts fwrite() inside libsed_testable for write-error injection.
 * @param[in] ptr Source buffer.
 * @param[in] size Element byte size.
 * @param[in] nmemb Element count.
 * @param[in,out] stream Output FILE stream.
 * @return Number of elements written.
 */
size_t sed_hook_fwrite(const void *ptr, size_t size, size_t nmemb,
                       FILE *stream);

/**
 * @brief Intercepts fread() inside libsed_testable for read-error injection.
 * @param[out] ptr Destination buffer.
 * @param[in] size Element byte size.
 * @param[in] nmemb Element count.
 * @param[in,out] stream Input FILE stream.
 * @return Number of elements read.
 */
size_t sed_hook_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief Intercepts fflush() inside libsed_testable for flush-error injection.
 * @param[in,out] stream Output FILE stream.
 * @return 0 on success, EOF on injected or underlying failure.
 */
int sed_hook_fflush(FILE *stream);

/**
 * @brief Intercepts ferror() inside libsed_testable for stream-error injection.
 * @param[in] stream Inspected FILE stream.
 * @return Non-zero if error indicator is active.
 */
int sed_hook_ferror(FILE *stream);

/**
 * @brief Intercepts fclose() inside libsed_testable for close-error injection.
 * @param[in,out] stream Stream to close.
 * @return 0 on success, EOF on injected or underlying failure.
 */
int sed_hook_fclose(FILE *stream);

/**
 * @brief Intercepts mkostemp() inside libsed_testable for temp-file fault
 * injection.
 * @param[in,out] tmpl Mutable template path ending in XXXXXX.
 * @param[in] flags Additional open flags.
 * @return Non-negative file descriptor on success, -1 on failure.
 */
int sed_hook_mkostemp(char *tmpl, int flags);

/**
 * @brief Intercepts rename() inside libsed_testable for atomic-rename fault
 * injection.
 * @param[in] oldpath Source path.
 * @param[in] newpath Destination path.
 * @return 0 on success, -1 on failure.
 */
int sed_hook_rename(const char *oldpath, const char *newpath);

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
size_t sed_hook_mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);

/**
 * @brief Intercepts isatty() inside libsed_testable for terminal panic testing.
 * @param[in] fd File descriptor to check.
 * @return Non-zero when fd is (or is simulated as) a terminal.
 */
int sed_hook_isatty(int fd);

/**
 * @brief Intercepts popen() inside libsed_testable for subprocess fault
 * injection.
 * @param[in] command Shell command string.
 * @param[in] type Stream mode ("r" or "w").
 * @return Pipe stream pointer, or NULL on injected failure.
 */
FILE *sed_hook_popen(const char *command, const char *type);

#ifdef SED_TESTABLE_BUILD
/** @brief Redirects exit() to sed_hook_exit() in testable sed units. */
#undef exit
#define exit(status) sed_hook_exit(status)

/** @brief Redirects fwrite() to sed_hook_fwrite() in testable sed units. */
#undef fwrite
#define fwrite(ptr, sz, n, fp) sed_hook_fwrite((ptr), (sz), (n), (fp))
/** @brief Redirects fwrite_unlocked() to sed_hook_fwrite() in testable sed. */
#undef fwrite_unlocked
#define fwrite_unlocked(ptr, sz, n, fp) sed_hook_fwrite((ptr), (sz), (n), (fp))

/** @brief Redirects fread() to sed_hook_fread() in testable sed units. */
#undef fread
#define fread(ptr, sz, n, fp) sed_hook_fread((ptr), (sz), (n), (fp))
/** @brief Redirects fread_unlocked() to sed_hook_fread() in testable sed. */
#undef fread_unlocked
#define fread_unlocked(ptr, sz, n, fp) sed_hook_fread((ptr), (sz), (n), (fp))

/** @brief Redirects fflush() to sed_hook_fflush() in testable sed units. */
#undef fflush
#define fflush(fp) sed_hook_fflush(fp)
/** @brief Redirects fflush_unlocked() to sed_hook_fflush() in testable sed. */
#undef fflush_unlocked
#define fflush_unlocked(fp) sed_hook_fflush(fp)

/** @brief Redirects ferror() to sed_hook_ferror() in testable sed units. */
#undef ferror
#define ferror(fp) sed_hook_ferror(fp)
/** @brief Redirects ferror_unlocked() to sed_hook_ferror() in testable sed. */
#undef ferror_unlocked
#define ferror_unlocked(fp) sed_hook_ferror(fp)

/** @brief Redirects fclose() to sed_hook_fclose() in testable sed units. */
#undef fclose
#define fclose(fp) sed_hook_fclose(fp)

/** @brief Redirects mkostemp() to sed_hook_mkostemp() in testable sed units. */
#undef mkostemp
#define mkostemp(tmpl, flags) sed_hook_mkostemp((tmpl), (flags))

/** @brief Redirects rename() to sed_hook_rename() in testable sed units. */
#undef rename
#define rename(from, to) sed_hook_rename((from), (to))

/** @brief Redirects mbrtowc() to sed_hook_mbrtowc() in testable sed units. */
#undef mbrtowc
#define mbrtowc(pwc, s, n, ps) sed_hook_mbrtowc((pwc), (s), (n), (ps))

/** @brief Redirects isatty() to sed_hook_isatty() in testable sed units. */
#undef isatty
#define isatty(fd) sed_hook_isatty(fd)

/** @brief Redirects popen() to sed_hook_popen() in testable sed units. */
#undef popen
#define popen(cmd, mode) sed_hook_popen((cmd), (mode))

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
extern int optreset;
/** @brief Resets BSD/macOS getopt_long state before sed_testable_main. */
#define SED_RST_OPT() (optreset = 1, optind = 1)
#else
/** @brief Resets GNU/linux-getopt state before sed_testable_main. */
#define SED_RST_OPT() (optind = 0)
#endif

#if O_BINARY
/** @brief Resets O_BINARY mode flag when present on target. */
#define SED_RST_BIN() (binary_mode = 0)
#else
/** @brief No-op binary_mode reset on POSIX targets where O_BINARY == 0. */
#define SED_RST_BIN() ((void)0)
#endif

/** @brief Resets program and regex CLI flags in sed.c. */
#define SED_RST_1() (the_program = NULL, extended_regexp_flags = 0, debug = 0)
/** @brief Resets buffering, output, and sandbox flags in sed.c. */
#define SED_RST_2() (unbuffered = 0, no_default_output = 0, sandbox = 0)
/** @brief Resets file-separation, symlink, and inplace flags in sed.c. */
#define SED_RST_3() (separate_files = 0, follow_symlinks = 0, SED_RST_OPT())
/** @brief Resets delimiter, line-length, and file-mode globals in sed.c. */
#define SED_RST_4() (in_place_extension = NULL, lcmd_out_line_len = 70)
/** @brief Resets stream mode strings and binary flag in sed.c. */
#define SED_RST_5() (buffer_delimiter = (char)10, read_mode = "r")
/** @brief Resets write_mode and binary_mode in sed.c. */
#define SED_RST_6() (write_mode = "w", SED_RST_BIN())
/** @brief Combined first half of sed_testable_main global reset. */
#define SED_RST_A() (SED_RST_1(), SED_RST_2(), SED_RST_3())
/** @brief Combined second half of sed_testable_main global reset. */
#define SED_RST_B() (SED_RST_4(), SED_RST_5(), SED_RST_6())

/** @brief Resets static and global CLI state at the start of sed_testable_main.
 */
#undef initialize_main
#define initialize_main(ac, av) (SED_RST_A(), SED_RST_B())

#include <obstack.h>
/** @brief Resets compile.c block, jump, label, and buffer static pointers. */
#define SED_RST_C1() (blocks = NULL, jumps = NULL, labels = NULL)
/** @brief Resets compile.c script-buffer and stdio stream static state. */
#define SED_RST_C2() (pending_text = NULL, old_text_buf = NULL)
/** @brief Resets compile.c first_script and stream tracking pointers. */
#define SED_RST_C3() (first_script = 1, my_stdin = NULL, my_stdout = NULL)
/** @brief Resets all compile.c static state prior to obstack_begin. */
#define SED_RST_C() (SED_RST_C1(), SED_RST_C2(), SED_RST_C3(), my_stderr = NULL)
/** @brief Resets compile.c static state when initializing a new program. */
#undef obstack_init
#define obstack_init(h) (SED_RST_C(), obstack_begin((h), 0))
#endif

#ifdef __cplusplus
}
#endif

#endif /* SED_TEST_HOOKS_H */
