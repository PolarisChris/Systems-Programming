#include <stdio.h>

static int is_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int main(void) {
    char input[1024];
    char *p;

    printf("Enter a string: ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 1;
    }

    p = input;
    while (*p != '\0') {
        char *word_start;
        char *word_end;
        char *prefix_end;

        while (*p != '\0' && !is_letter(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        word_start = p;
        word_end = p;
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

        p = word_end;
    }

    return 0;
}
