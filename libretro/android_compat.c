#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifndef SYS_gettid
#if defined(__aarch64__)
#define SYS_gettid 178
#elif defined(__x86_64__)
#define SYS_gettid 186
#else
#define SYS_gettid 224
#endif
#endif

int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    va_list ap;
    (void)prio;
    fprintf(stderr, "[%s] ", tag ? tag : "seedless");
    va_start(ap, fmt);
    vfprintf(stderr, fmt ? fmt : "", ap);
    va_end(ap);
    fputc('\n', stderr);
    return 0;
}

pid_t gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

#if defined(__GLIBC__)
#include <malloc.h>
#else
void *memalign(size_t alignment, size_t size)
{
    void *p = NULL;
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);
    if (posix_memalign(&p, alignment, size) != 0)
        return NULL;
    return p;
}
#endif

void seedless_compat_init(void)
{
}
