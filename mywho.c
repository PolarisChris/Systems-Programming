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
    const char *candidate_paths[] = {
        UTMP_FILE_CANDIDATE_1,
        UTMP_FILE_CANDIDATE_2,
        UTMP_FILE_CANDIDATE_3
    };

    for (size_t path_index = 0;
         path_index < sizeof(candidate_paths) / sizeof(candidate_paths[0]);
         path_index++) {
        int utmp_fd = open(candidate_paths[path_index], O_RDONLY);
        if (utmp_fd != -1) {
            return utmp_fd;
        }
    }

    return -1;
}

static void trim_copy(char *destination,
                      size_t destination_size,
                      const char *source,
                      size_t source_size) {
    size_t copy_length = 0;

    while (copy_length < source_size && source[copy_length] != '\0') {
        copy_length++;
    }

    if (copy_length >= destination_size) {
        copy_length = destination_size - 1;
    }

    memcpy(destination, source, copy_length);
    destination[copy_length] = '\0';
}

int main(void) {
    int utmp_fd = open_utmp_file();
    if (utmp_fd == -1) {
        perror("Error opening utmp file");
        return EXIT_FAILURE;
    }

    struct utmp utmp_entry;
    ssize_t bytes_read;

    while ((bytes_read = read(utmp_fd, &utmp_entry, sizeof(utmp_entry))) ==
           sizeof(utmp_entry)) {
        if (utmp_entry.ut_type == USER_PROCESS) {
            char username[UT_NAMESIZE + 1];
            char terminal_line[UT_LINESIZE + 1];
            char remote_host[UT_HOSTSIZE + 1];
            char time_buffer[64];

            trim_copy(username,
                      sizeof(username),
                      utmp_entry.ut_user,
                      sizeof(utmp_entry.ut_user));
            trim_copy(terminal_line,
                      sizeof(terminal_line),
                      utmp_entry.ut_line,
                      sizeof(utmp_entry.ut_line));
            trim_copy(remote_host,
                      sizeof(remote_host),
                      utmp_entry.ut_host,
                      sizeof(utmp_entry.ut_host));

            time_t login_time = utmp_entry.ut_tv.tv_sec;
            struct tm *local_time_info = localtime(&login_time);

            if (local_time_info == NULL) {
                strncpy(time_buffer, "unknown time", sizeof(time_buffer) - 1);
                time_buffer[sizeof(time_buffer) - 1] = '\0';
            } else {
                strftime(time_buffer,
                         sizeof(time_buffer),
                         "%Y-%m-%d %H:%M:%S",
                         local_time_info);
            }

            if (strlen(remote_host) > 0) {
                printf("%-12s %-12s %-20s %s\n",
                       username,
                       terminal_line,
                       time_buffer,
                       remote_host);
            } else {
                printf("%-12s %-12s %-20s\n", username, terminal_line, time_buffer);
            }
        }
    }

    if (bytes_read == -1) {
        perror("Error reading utmp file");
        close(utmp_fd);
        return EXIT_FAILURE;
    }

    close(utmp_fd);
    return EXIT_SUCCESS;
}
