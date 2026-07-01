// glibc portability shim for the GPU backend DSOs.
//
// glibc 2.38+ headers redirect strtol/strtoul/strtoull/sscanf to versioned
// __isoc23_* symbols whenever __GLIBC_USE(ISOC23) is on — which _GNU_SOURCE
// enables, and g++ defines _GNU_SOURCE unconditionally on linux-gnu. A DSO
// built on such a host then fails to dlopen on older distros (e.g. Debian 12
// / glibc 2.36) with "GLIBC_2.38 not found". Same class of problem as the
// GLIBCXX one solved by statically linking libstdc++ in link_shared_library.
//
// Define the __isoc23_* names locally and forward to the classic symbols.
// The __asm__ aliases bind the *unversioned* names directly, sidestepping
// the header redirect (calling strtol here would just redirect again).
// None of our callers use C23-only conversions (%wN, binary ints), so the
// classic behavior is equivalent.

#include <stdarg.h>

extern long classic_strtol(const char *, char **, int) __asm__("strtol");
extern unsigned long classic_strtoul(const char *, char **, int) __asm__("strtoul");
extern unsigned long long classic_strtoull(const char *, char **, int) __asm__("strtoull");
extern int classic_vsscanf(const char *, const char *, va_list) __asm__("vsscanf");

long __isoc23_strtol(const char *s, char **end, int base) {
    return classic_strtol(s, end, base);
}

unsigned long __isoc23_strtoul(const char *s, char **end, int base) {
    return classic_strtoul(s, end, base);
}

unsigned long long __isoc23_strtoull(const char *s, char **end, int base) {
    return classic_strtoull(s, end, base);
}

int __isoc23_sscanf(const char *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = classic_vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}
