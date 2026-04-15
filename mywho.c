/*
 * mywho.c
 * Mimics the basic functionality of the who command by reading utmp entries.
 *
 * Compile:
 *     gcc -Wall -Wextra -o mywho mywho.c
 *
 * Run:
 *     ./mywho
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#ifndef UTMP_FILE_CANDIDATE_1
#define UTMP_FILE_CANDIDATE_1 "/var/run/utmp"
#endif

#ifndef UTMP_FILE_CANDIDATE_2
#define UTMP_FILE_CANDIDATE_2 "/run/utmp"
#endif

#ifndef UTMP_FILE_CANDIDATE_3
#define UTMP_FILE_CANDIDATE_3 "/var/adm/utmpx"
#endif

static int open_utmp_file(void) {
    const char *paths[] = {
        UTMP_FILE_CANDIDATE_1,
        UTMP_FILE_CANDIDATE_2,
        UTMP_FILE_CANDIDATE_3
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd != -1) {
            return fd;
        }
    }

    return -1;
}

static void trim_copy(char *dest, size_t dest_size, const char *src, size_t src_size) {
    size_t len = 0;

    while (len < src_size && src[len] != '\0') {
        len++;
    }

    if (len >= dest_size) {
        len = dest_size - 1;
    }

    memcpy(dest, src, len);
    dest[len] = '\0';
}

int main(void) {
    int fd = open_utmp_file();
    if (fd == -1) {
        perror("Error opening utmp file");
        return EXIT_FAILURE;
    }

    struct utmp entry;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, &entry, sizeof(entry))) == sizeof(entry)) {
        if (entry.ut_type == USER_PROCESS) {
            char username[UT_NAMESIZE + 1];
            char line[UT_LINESIZE + 1];
            char host[UT_HOSTSIZE + 1];
            char timebuf[64];

            trim_copy(username, sizeof(username), entry.ut_user, sizeof(entry.ut_user));
            trim_copy(line, sizeof(line), entry.ut_line, sizeof(entry.ut_line));
            trim_copy(host, sizeof(host), entry.ut_host, sizeof(entry.ut_host));

            time_t login_time = entry.ut_tv.tv_sec;
            struct tm *tm_info = localtime(&login_time);

            if (tm_info == NULL) {
                strncpy(timebuf, "unknown time", sizeof(timebuf) - 1);
                timebuf[sizeof(timebuf) - 1] = '\0';
            } else {
                strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
            }

            if (strlen(host) > 0) {
                printf("%-12s %-12s %-20s %s\n", username, line, timebuf, host);
            } else {
                printf("%-12s %-12s %-20s\n", username, line, timebuf);
            }
        }
    }

    if (bytes_read == -1) {
        perror("Error reading utmp file");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    return EXIT_SUCCESS;
}