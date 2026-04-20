#include <stdio.h>

void print_fizzbuzz(int value) {
    if (value % 15 == 0) {
        printf("FizzBuzz\n");
    } else if (value % 3 == 0) {
        printf("Fizz\n");
    } else if (value % 5 == 0) {
        printf("Buzz\n");
    } else {
        printf("%d\n", value);
    }
}

int main(void) {
    int limit;

    printf("Please enter an integer: ");
    if (scanf("%d", &limit) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    if (limit >= 0) {
        for (int current_number = 0; current_number <= limit; current_number++) {
            print_fizzbuzz(current_number);
        }
    } else {
        for (int current_number = 0; current_number >= limit; current_number--) {
            print_fizzbuzz(current_number);
        }
    }

    return 0;
}
