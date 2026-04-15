#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define INITIAL_LENGTH 5
#define MAX_TROPHY_VALUE 9
#define MIN_TROPHY_VALUE 1
#define MIN_TROPHY_LIFETIME 1
#define MAX_TROPHY_LIFETIME 9

typedef struct {
    int y;
    int x;
} Point;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    Point *body;
    int length;
    int capacity;
    Direction dir;
} Snake;

typedef struct {
    Point pos;
    int value;
    time_t spawn_time;
    int lifetime;
} Trophy;

/* Primary Author: Giovanni
   Main Functionality: Return 1 if two points are equal, otherwise return 0. */
int points_equal(Point a, Point b) {
    return (a.y == b.y && a.x == b.x);
}

/* Primary Author: Giovanni
   Main Functionality: Return 1 if the given point is on the snake's body, otherwise return 0. */
int point_on_snake(const Snake *snake, Point p) {
    for (int i = 0; i < snake->length; i++) {
        if (points_equal(snake->body[i], p)) {
            return 1;
        }
    }
    return 0;
}

/* Primary Author: Giovanni
   Main Functionality: Return 1 if the two directions are direct opposites, otherwise return 0. */
int is_opposite_direction(Direction a, Direction b) {
    return ((a == DIR_UP && b == DIR_DOWN) ||
            (a == DIR_DOWN && b == DIR_UP) ||
            (a == DIR_LEFT && b == DIR_RIGHT) ||
            (a == DIR_RIGHT && b == DIR_LEFT));
}

/* Primary Author: Giovanni
   Main Functionality: Convert a direction into the next head position. */
Point get_next_head(Point head, Direction dir) {
    Point next = head;

    switch (dir) {
        case DIR_UP:
            next.y--;
            break;
        case DIR_DOWN:
            next.y++;
            break;
        case DIR_LEFT:
            next.x--;
            break;
        case DIR_RIGHT:
            next.x++;
            break;
    }

    return next;
}

/* Primary Author: Giovanni
   Main Functionality: Generate a random direction for the snake's initial movement. */
Direction random_direction(void) {
    return (Direction)(rand() % 4);
}

/* Primary Author: Giovanni
   Main Functionality: Initialize the snake in the center of the terminal with length 5 and random direction. */
void initialize_snake(Snake *snake, int max_y, int max_x) {
    snake->capacity = max_y * max_x;
    snake->body = (Point *)malloc(sizeof(Point) * snake->capacity);
    snake->length = INITIAL_LENGTH;
    snake->dir = random_direction();

    int center_y = max_y / 2;
    int center_x = max_x / 2;

    snake->body[0].y = center_y;
    snake->body[0].x = center_x;

    for (int i = 1; i < snake->length; i++) {
        switch (snake->dir) {
            case DIR_UP:
                snake->body[i].y = center_y + i;
                snake->body[i].x = center_x;
                break;
            case DIR_DOWN:
                snake->body[i].y = center_y - i;
                snake->body[i].x = center_x;
                break;
            case DIR_LEFT:
                snake->body[i].y = center_y;
                snake->body[i].x = center_x + i;
                break;
            case DIR_RIGHT:
                snake->body[i].y = center_y;
                snake->body[i].x = center_x - i;
                break;
        }
    }
}

/* Primary Author: Giovanni
   Main Functionality: Free all memory used by the snake. */
void destroy_snake(Snake *snake) {
    free(snake->body);
    snake->body = NULL;
}

/* Primary Author: Giovanni
   Main Functionality: Place a trophy at a random valid location that is not on the snake or border. */
void spawn_trophy(Trophy *trophy, const Snake *snake, int max_y, int max_x) {
    Point p;

    do {
        p.y = (rand() % (max_y - 2)) + 1;
        p.x = (rand() % (max_x - 2)) + 1;
    } while (point_on_snake(snake, p));

    trophy->pos = p;
    trophy->value = (rand() % MAX_TROPHY_VALUE) + MIN_TROPHY_VALUE;
    trophy->spawn_time = time(NULL);
    trophy->lifetime = (rand() % MAX_TROPHY_LIFETIME) + MIN_TROPHY_LIFETIME;
}

/* Primary Author: Giovanni
   Main Functionality: Return 1 if the current trophy has expired, otherwise return 0. */
int trophy_expired(const Trophy *trophy) {
    time_t now = time(NULL);
    return (int)(now - trophy->spawn_time) >= trophy->lifetime;
}

/* Primary Author: Giovanni
   Main Functionality: Draw the game border, snake, trophy, and status information. */
void draw_game(const Snake *snake, const Trophy *trophy, int max_y, int max_x, int win_length) {
    clear();

    for (int x = 0; x < max_x; x++) {
        mvaddch(0, x, '#');
        mvaddch(max_y - 1, x, '#');
    }

    for (int y = 0; y < max_y; y++) {
        mvaddch(y, 0, '#');
        mvaddch(y, max_x - 1, '#');
    }

    mvaddch(trophy->pos.y, trophy->pos.x, '0' + trophy->value);

    mvaddch(snake->body[0].y, snake->body[0].x, 'O');
    for (int i = 1; i < snake->length; i++) {
        mvaddch(snake->body[i].y, snake->body[i].x, 'o');
    }

    mvprintw(0, 2, " Length: %d ", snake->length);
    mvprintw(0, 18, " Goal: %d ", win_length);
    mvprintw(0, 32, " Trophy: %d ", trophy->value);

    refresh();
}

/* Primary Author: Giovanni
   Main Functionality: Return 1 if the snake's head collides with the border, otherwise return 0. */
int hits_border(Point head, int max_y, int max_x) {
    return (head.y <= 0 || head.y >= max_y - 1 || head.x <= 0 || head.x >= max_x - 1);
}

/* Primary Author: Giovanni
   Main Functionality: Return 1 if the snake's next head position collides with its body, otherwise return 0. */
int hits_self(const Snake *snake, Point next_head, int growing) {
    int check_length = snake->length;

    if (!growing) {
        check_length = snake->length - 1;
    }

    for (int i = 0; i < check_length; i++) {
        if (points_equal(snake->body[i], next_head)) {
            return 1;
        }
    }

    return 0;
}

/* Primary Author: Giovanni
   Main Functionality: Move the snake one step, optionally growing it if a trophy was eaten. */
void move_snake(Snake *snake, Point next_head, int grow_amount) {
    int new_length = snake->length + grow_amount;

    if (new_length > snake->capacity) {
        snake->capacity = new_length + 100;
        snake->body = (Point *)realloc(snake->body, sizeof(Point) * snake->capacity);
    }

    for (int i = snake->length - 1; i >= 0; i--) {
        snake->body[i + 1] = snake->body[i];
    }

    snake->body[0] = next_head;
    snake->length++;

    for (int g = 1; g < grow_amount; g++) {
        snake->body[snake->length] = snake->body[snake->length - 1];
        snake->length++;
    }

    if (grow_amount == 0) {
        snake->length--;
    }
}

/* Primary Author: Giovanni
   Main Functionality: Convert arrow-key input into a direction change if applicable. */
int handle_input(Direction *requested_dir) {
    int ch = getch();

    switch (ch) {
        case KEY_UP:
            *requested_dir = DIR_UP;
            return 1;
        case KEY_DOWN:
            *requested_dir = DIR_DOWN;
            return 1;
        case KEY_LEFT:
            *requested_dir = DIR_LEFT;
            return 1;
        case KEY_RIGHT:
            *requested_dir = DIR_RIGHT;
            return 1;
        default:
            return 0;
    }
}

/* Primary Author: Giovanni
   Main Functionality: Calculate the target length needed to win, defined as half the border perimeter. */
int calculate_win_length(int max_y, int max_x) {
    int perimeter = 2 * max_y + 2 * max_x - 4;
    return perimeter / 2;
}

/* Primary Author: Giovanni
   Main Functionality: Return the game delay in microseconds, making speed increase as snake length increases. */
int calculate_delay_us(int snake_length) {
    int delay = 180000 - (snake_length * 4000);

    if (delay < 50000) {
        delay = 50000;
    }

    return delay;
}

/* Primary Author: Giovanni
   Main Functionality: Show the final game result message and wait for a key press before exiting. */
void show_end_screen(const char *message, int max_y, int max_x) {
    clear();
    mvprintw(max_y / 2 - 1, (max_x - 20) / 2, "%s", message);
    mvprintw(max_y / 2 + 1, (max_x - 28) / 2, "Press any key to exit...");
    refresh();

    nodelay(stdscr, FALSE);
    getch();
}

/* Primary Author: Giovanni
   Main Functionality: Initialize ncurses settings for real-time snake gameplay. */
void initialize_ncurses(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);
}

/* Primary Author: Giovanni
   Main Functionality: Cleanly shut down ncurses mode before program exit. */
void shutdown_ncurses(void) {
    endwin();
}

/* Primary Author: Giovanni
   Main Functionality: Run the full snake game loop including movement, trophies, win/loss checks, and rendering. */
int main(void) {
    srand((unsigned int)time(NULL));

    initialize_ncurses();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    if (max_y < 10 || max_x < 20) {
        shutdown_ncurses();
        printf("Terminal window is too small. Please resize and try again.\n");
        return 1;
    }

    Snake snake;
    Trophy trophy;
    initialize_snake(&snake, max_y, max_x);
    spawn_trophy(&trophy, &snake, max_y, max_x);

    int win_length = calculate_win_length(max_y, max_x);
    int game_over = 0;
    int win = 0;

    while (!game_over) {
        Direction requested_dir = snake.dir;
        int input_received = handle_input(&requested_dir);

        if (input_received) {
            if (is_opposite_direction(snake.dir, requested_dir)) {
                game_over = 1;
                break;
            } else {
                snake.dir = requested_dir;
            }
        }

        if (trophy_expired(&trophy)) {
            spawn_trophy(&trophy, &snake, max_y, max_x);
        }

        Point next_head = get_next_head(snake.body[0], snake.dir);
        int growing = points_equal(next_head, trophy.pos);
        int grow_amount = growing ? trophy.value : 0;

        if (hits_border(next_head, max_y, max_x)) {
            game_over = 1;
            break;
        }

        if (hits_self(&snake, next_head, growing)) {
            game_over = 1;
            break;
        }

        move_snake(&snake, next_head, grow_amount);

        if (growing) {
            spawn_trophy(&trophy, &snake, max_y, max_x);
        }

        if (snake.length >= win_length) {
            win = 1;
            game_over = 1;
        }

        draw_game(&snake, &trophy, max_y, max_x, win_length);
        usleep(calculate_delay_us(snake.length));
    }

    if (win) {
        show_end_screen("You win!", max_y, max_x);
    } else {
        show_end_screen("Game over!", max_y, max_x);
    }

    destroy_snake(&snake);
    shutdown_ncurses();
    return 0;
}