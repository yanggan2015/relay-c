#include "audit.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

void relay_audit(const char *ip, const char *method, const char *path,
                 const char *qs, int status, const char *note) {
    time_t now;
    struct tm *tm;
    char ts[40];
    char fname[64];
    FILE *f;

#ifdef _WIN32
    _mkdir("logs");
#else
    mkdir("logs", 0755);
#endif

    time(&now);
    tm = localtime(&now);
    if (!tm) return;
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    snprintf(fname, sizeof(fname), "logs/access_%04d%02d%02d.log",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    f = fopen(fname, "a");
    if (!f) return;
    fprintf(f, "%s | ip=%s | method=%s | path=%s | qs=%s | status=%d | note=%s\n",
            ts,
            ip ? ip : "",
            method ? method : "",
            path ? path : "",
            qs ? qs : "",
            status,
            note ? note : "");
    fclose(f);
}
