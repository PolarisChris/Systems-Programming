#include <stdio.h>

int main(void) {
    int grade;

    printf("Please enter a numeric grade: ");

    if (scanf("%d", &grade) != 1) {
        printf("Your input could not be interpreted as an integer value.\n");
        return 0;
    }

    if (grade < 0 || grade > 100) {
        printf("You entered an invalid value.\n");
        return 0;
    }

    if (grade >= 90) {
        printf("Your grade is an A.\n");
    } else if (grade >= 80) {
        printf("Your grade is a B.\n");
    } else if (grade >= 70) {
        printf("Your grade is a C.\n");
    } else if (grade >= 60) {
        printf("Your grade is a D.\n");
    } else {
        printf("Your grade is an F.\n");
    }

    return 0;
}
