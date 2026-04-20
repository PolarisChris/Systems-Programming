#include <stdio.h>

static int is_letter(char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z');
}

int main(void) {
    char input_line[1024];
    char *scan_cursor;

    printf("Enter a string: ");
    if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
        return 1;
    }

    scan_cursor = input_line;
    while (*scan_cursor != '\0') {
        char *word_start;
        char *word_end;
        char *prefix_end;

        while (*scan_cursor != '\0' && !is_letter(*scan_cursor)) {
            scan_cursor++;
        }
        if (*scan_cursor == '\0') {
            break;
        }

        word_start = scan_cursor;
        word_end = scan_cursor;
        while (*word_end != '\0' && is_letter(*word_end)) {
            word_end++;
        }

        prefix_end = word_start;
        while (prefix_end < word_end) {
            char *cursor = word_start;
            while (cursor <= prefix_end) {
                putchar(*cursor);
                cursor++;
            }
            putchar('\n');
            prefix_end++;
        }

        scan_cursor = word_end;
    }

    return 0;
}
