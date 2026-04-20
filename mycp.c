/*
 * mycp.c
 * Copies one file to another using low-level system calls.
 *
 * Compile:
 *     gcc -Wall -Wextra -o mycp mycp.c
 *
 * Run:
 *     ./mycp source_file destination_file
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

static void write_str(int file_descriptor, const char *message) {
    while (*message != '\0') {
        ssize_t written = write(file_descriptor, message, strlen(message));
        if (written <= 0) {
            return;
        }
        message += written;
    }
}

static void write_error(const char *prefix, const char *path) {
    write_str(STDERR_FILENO, prefix);
    write_str(STDERR_FILENO, path);
    write_str(STDERR_FILENO, ": ");
    write_str(STDERR_FILENO, strerror(errno));
    write_str(STDERR_FILENO, "\n");
}

static int write_all(int file_descriptor, const char *buffer, ssize_t byte_count) {
    ssize_t total_written = 0;

    while (total_written < byte_count) {
        ssize_t written = write(file_descriptor,
                                buffer + total_written,
                                byte_count - total_written);
        if (written < 0) {
            return -1;
        }
        total_written += written;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write_str(STDERR_FILENO, "Usage: ./mycp <source_file> <destination_file>\n");
        return EXIT_FAILURE;
    }

    const char *source_file = argv[1];
    const char *destination_file = argv[2];

    int source_fd = open(source_file, O_RDONLY);
    if (source_fd < 0) {
        write_error("Error opening source file ", source_file);
        return EXIT_FAILURE;
    }

    struct stat source_stats;
    if (fstat(source_fd, &source_stats) < 0) {
        write_error("Error getting permissions for source file ", source_file);
        close(source_fd);
        return EXIT_FAILURE;
    }

    int destination_fd = open(destination_file,
                              O_WRONLY | O_CREAT | O_TRUNC,
                              source_stats.st_mode & 07777);
    if (destination_fd < 0) {
        write_error("Error opening destination file ", destination_file);
        close(source_fd);
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write_all(destination_fd, buffer, bytes_read) < 0) {
            write_error("Error writing to destination file ", destination_file);
            close(source_fd);
            close(destination_fd);
            return EXIT_FAILURE;
        }
    }

    if (bytes_read < 0) {
        write_error("Error reading source file ", source_file);
        close(source_fd);
        close(destination_fd);
        return EXIT_FAILURE;
    }

    if (close(source_fd) < 0) {
        write_error("Error closing source file ", source_file);
        close(destination_fd);
        return EXIT_FAILURE;
    }

    if (close(destination_fd) < 0) {
        write_error("Error closing destination file ", destination_file);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
