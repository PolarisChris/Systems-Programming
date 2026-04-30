#define _POSIX_C_SOURCE 200809L

/*
 * Project split:
 * Chris did terminal setup, signals, cleanup, and status messaging.
 * Brandon did the shared game data and startup configuration.
 * Giovanni did input, rendering, movement, collision checks, and the main loop.
 *
 * Command to Build on Linux/WSL:
 * gcc -Wall -Wextra -std=c11 SnakeGameInitial.c -lncursesw -o snake
 */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__has_include)
#if __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncursesw/ncurses.h>)
#include <ncursesw/ncurses.h>
#elif __has_include(<ncursesw/curses.h>)
#include <ncursesw/curses.h>
#elif __has_include(<curses.h>)
#include <curses.h>
#else
#error "Install ncurses development headers, such as libncurses-dev."
#endif
#else
#include <ncurses.h>
#endif

#define INITIAL_LENGTH 5
#define FRAME_DELAY_MS 140
#define MIN_FRAME_DELAY_MS 45
#define MIN_ROWS 7
#define MIN_COLS 12
#define MIN_TROPHY_VALUE 1
#define MAX_TROPHY_VALUE 9
#define MIN_TROPHY_SECONDS 1
#define MAX_TROPHY_SECONDS 9

/* Brandon note: this struct stores one row/column coordinate for a snake segment. */
typedef struct {
    int row;
    int col;
} Point;

/* Brandon note: this enum keeps the snake's current travel direction readable. */
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

/* Brandon note: this struct groups the snake body buffer with its live movement state. */
typedef struct {
    Point *segments;
    int length;
    int capacity;
    Direction direction;
} Snake;

/* Brandon note: this struct keeps the single active trophy's value, position, and timeout. */
typedef struct {
    Point position;
    int value;
    time_t created_at;
    int lifetime_seconds;
} Trophy;

/* Brandon note: this struct bundles board dimensions, snake data, trophy data, and game status. */
typedef struct {
    int rows;
    int cols;
    Snake snake;
    Trophy trophy;
    int winning_length;
    bool running;
    char end_message[128];
} Game;

static bool curses_initialized = false;
static volatile sig_atomic_t shutdown_requested = 0;

static void cleanup_curses(void) {
    /* Chris | terminal cleanup: restore the shell once ncurses is done. */
    if (curses_initialized) {
        endwin();
        curses_initialized = false;
    }
}

static void handle_signal(int signum) {
    /* Chris | signal path: flip the shutdown flag so the loop can exit cleanly. */
    (void)signum;
    shutdown_requested = 1;
}

static void die(const char *message) {
    /* Chris | fail fast: reset terminal state first, then print the fatal error. */
    cleanup_curses();
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void install_signal_handlers(void) {
    /* Chris | setup step: register the signals that should stop the game safely. */
    struct sigaction signal_action;

    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_handler = handle_signal;

    if (sigaction(SIGINT, &signal_action, NULL) == -1 ||
        sigaction(SIGTERM, &signal_action, NULL) == -1 ||
        sigaction(SIGQUIT, &signal_action, NULL) == -1) {
        die("Unable to install signal handlers.");
    }
}

static void initialize_curses(void) {
    /* Chris | terminal setup: put ncurses in nonblocking mode for live keyboard input. */
    if (initscr() == NULL) {
        die("Unable to initialize ncurses.");
    }

    curses_initialized = true;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
}

static void set_end_message(Game *game, const char *message) {
    /* Chris | status text: keep the final message ready for output after shutdown. */
    snprintf(game->end_message, sizeof(game->end_message), "%s", message);
}

static void update_dimensions(Game *game, bool fail_if_small) {
    /* Brandon note: refresh the board size from the current terminal window. */
    getmaxyx(stdscr, game->rows, game->cols);

    if (game->rows < MIN_ROWS || game->cols < MIN_COLS) {
        if (fail_if_small) {
            die("Terminal window is too small for the game.");
        }

        set_end_message(game, "Game over: terminal window is too small.");
        game->running = false;
    }
}

static bool is_opposite(Direction current_direction, Direction proposed_direction) {
    /* Giovanni -> guardrail: reject instant 180-degree turns. */
    return (current_direction == DIR_UP && proposed_direction == DIR_DOWN) ||
           (current_direction == DIR_DOWN && proposed_direction == DIR_UP) ||
           (current_direction == DIR_LEFT && proposed_direction == DIR_RIGHT) ||
           (current_direction == DIR_RIGHT && proposed_direction == DIR_LEFT);
}

static int random_between(int minimum, int maximum) {
    /* Brandon note: return one random integer inside the inclusive project range. */
    return minimum + rand() % (maximum - minimum + 1);
}

static Direction random_direction(void) {
    /* Brandon note: choose the initial snake direction randomly from the four choices. */
    return (Direction)random_between(DIR_UP, DIR_RIGHT);
}

static int winning_length(const Game *game) {
    /* Brandon note: calculate the required win length from half of the border perimeter. */
    return (2 * game->rows + 2 * game->cols - 4) / 2;
}

static bool same_point(Point first_point, Point second_point) {
    /* Giovanni -> coordinate check: report whether two board positions match. */
    return first_point.row == second_point.row && first_point.col == second_point.col;
}

static bool point_on_snake(const Snake *snake, Point point) {
    /* Giovanni -> placement check: keep trophies from appearing on top of the snake. */
    int segment_index;

    for (segment_index = 0; segment_index < snake->length; ++segment_index) {
        if (same_point(snake->segments[segment_index], point)) {
            return true;
        }
    }

    return false;
}

static void initialize_snake(Game *game) {
    /* Brandon note: build the starting length-5 snake centered with a random direction. */
    int center_row;
    int starting_head_col;
    int starting_head_row;
    int segment_index;

    game->snake.capacity = (game->rows - 2) * (game->cols - 2);
    game->snake.length = INITIAL_LENGTH;
    game->snake.direction = random_direction();
    game->snake.segments = malloc((size_t)game->snake.capacity * sizeof(Point));

    if (game->snake.segments == NULL) {
        die("Unable to allocate memory for the snake.");
    }

    center_row = game->rows / 2;
    starting_head_row = center_row;
    starting_head_col = game->cols / 2;

    switch (game->snake.direction) {
        case DIR_UP:
            if (starting_head_row > game->rows - 1 - INITIAL_LENGTH) {
                starting_head_row = game->rows - 1 - INITIAL_LENGTH;
            }
            break;

        case DIR_DOWN:
            if (starting_head_row < INITIAL_LENGTH) {
                starting_head_row = INITIAL_LENGTH;
            }
            break;

        case DIR_LEFT:
            if (starting_head_col > game->cols - 1 - INITIAL_LENGTH) {
                starting_head_col = game->cols - 1 - INITIAL_LENGTH;
            }
            break;

        case DIR_RIGHT:
            if (starting_head_col < INITIAL_LENGTH) {
                starting_head_col = INITIAL_LENGTH;
            }
            break;
    }

    for (segment_index = 0; segment_index < game->snake.length; ++segment_index) {
        game->snake.segments[segment_index].row = starting_head_row;
        game->snake.segments[segment_index].col = starting_head_col;

        switch (game->snake.direction) {
            case DIR_UP:
                game->snake.segments[segment_index].row += segment_index;
                break;

            case DIR_DOWN:
                game->snake.segments[segment_index].row -= segment_index;
                break;

            case DIR_LEFT:
                game->snake.segments[segment_index].col += segment_index;
                break;

            case DIR_RIGHT:
                game->snake.segments[segment_index].col -= segment_index;
                break;
        }
    }
}

static void spawn_trophy(Game *game) {
    /* Brandon note: place exactly one random trophy inside the pit and away from the snake. */
    Point next_position;

    do {
        next_position.row = random_between(1, game->rows - 2);
        next_position.col = random_between(1, game->cols - 2);
    } while (point_on_snake(&game->snake, next_position));

    game->trophy.position = next_position;
    game->trophy.value = random_between(MIN_TROPHY_VALUE, MAX_TROPHY_VALUE);
    game->trophy.created_at = time(NULL);
    game->trophy.lifetime_seconds = random_between(MIN_TROPHY_SECONDS, MAX_TROPHY_SECONDS);
}

static void initialize_game(Game *game) {
    /* Brandon note: wire up the initial game state before the first frame is drawn. */
    memset(game, 0, sizeof(*game));
    srand((unsigned int)time(NULL));
    update_dimensions(game, true);
    initialize_snake(game);
    game->winning_length = winning_length(game);
    spawn_trophy(game);
    game->running = true;
    set_end_message(game, "Game ended.");
}

static void draw_game(const Game *game) {
    /* Giovanni -> render pass: draw the border, trophy, snake, and live game stats. */
    char status_message[128];
    int segment_index;
    int seconds_left;
    int status_width;

    erase();
    box(stdscr, 0, 0);

    seconds_left = game->trophy.lifetime_seconds -
                   (int)(time(NULL) - game->trophy.created_at);
    if (seconds_left < 0) {
        seconds_left = 0;
    }

    mvaddch(game->trophy.position.row,
            game->trophy.position.col,
            (chtype)('0' + game->trophy.value));

    for (segment_index = 0; segment_index < game->snake.length; ++segment_index) {
        mvaddch(game->snake.segments[segment_index].row,
                game->snake.segments[segment_index].col,
                (segment_index == 0) ? '@' : 'o');
    }

    snprintf(status_message,
             sizeof(status_message),
             " Length: %d Goal: %d Trophy: %d Time: %d ",
             game->snake.length,
             game->winning_length,
             game->trophy.value,
             seconds_left);
    status_width = game->cols - 4;
    if (status_width > 0) {
        mvaddnstr(0, 2, status_message, status_width);
    }

    refresh();
}

static void apply_direction(Game *game, Direction next_direction) {
    /* Giovanni -> input rule: apply the turn unless it is an illegal reversal. */
    if (is_opposite(game->snake.direction, next_direction)) {
        set_end_message(game, "Game over: reverse direction is not allowed.");
        game->running = false;
        return;
    }

    game->snake.direction = next_direction;
}

static void handle_keypress(Game *game, int pressed_key) {
    /* Giovanni -> key handling: arrows steer, q quits, and resize updates the board. */
    switch (pressed_key) {
        case KEY_UP:
            apply_direction(game, DIR_UP);
            break;

        case KEY_DOWN:
            apply_direction(game, DIR_DOWN);
            break;

        case KEY_LEFT:
            apply_direction(game, DIR_LEFT);
            break;

        case KEY_RIGHT:
            apply_direction(game, DIR_RIGHT);
            break;

        case KEY_RESIZE:
            update_dimensions(game, false);
            break;

        case 'q':
        case 'Q':
            set_end_message(game, "Game ended by user.");
            game->running = false;
            break;

        default:
            break;
    }
}

static void poll_input(Game *game) {
    /* Giovanni -> input sweep: consume every queued key before moving the snake. */
    int pressed_key;

    while (game->running && (pressed_key = getch()) != ERR) {
        handle_keypress(game, pressed_key);
    }
}

static bool hits_body(const Snake *snake, Point next_head, bool growing) {
    /* Giovanni -> collision check: test the next head position against the body. */
    int segment_index;
    int checked_length;

    checked_length = growing ? snake->length : snake->length - 1;

    for (segment_index = 0; segment_index < checked_length; ++segment_index) {
        if (snake->segments[segment_index].row == next_head.row &&
            snake->segments[segment_index].col == next_head.col) {
            return true;
        }
    }

    return false;
}

static Point next_head_position(const Snake *snake) {
    /* Giovanni -> movement math: compute the next head cell from the active direction. */
    Point next_head = snake->segments[0];

    switch (snake->direction) {
        case DIR_UP:
            --next_head.row;
            break;

        case DIR_DOWN:
            ++next_head.row;
            break;

        case DIR_LEFT:
            --next_head.col;
            break;

        case DIR_RIGHT:
            ++next_head.col;
            break;
    }

    return next_head;
}

static bool trophy_expired(const Game *game) {
    /* Brandon note: report when the current trophy's random timer has run out. */
    return (int)(time(NULL) - game->trophy.created_at) >= game->trophy.lifetime_seconds;
}

static bool snake_eats_trophy(const Game *game, Point next_head) {
    /* Giovanni -> trophy check: decide whether the next head cell earns growth. */
    return same_point(next_head, game->trophy.position);
}

static void advance_snake(Game *game) {
    /* Chris | gameplay step: move, grow on trophies, and stop on win/loss states. */
    Point next_head;
    Point tail_copy;
    int segment_index;
    int grow_amount;
    int new_length;
    bool growing;

    if (trophy_expired(game)) {
        spawn_trophy(game);
    }

    next_head = next_head_position(&game->snake);
    growing = snake_eats_trophy(game, next_head);
    grow_amount = growing ? game->trophy.value : 0;

    if (next_head.row <= 0 || next_head.row >= game->rows - 1 ||
        next_head.col <= 0 || next_head.col >= game->cols - 1) {
        set_end_message(game, "Game over: the snake hit the border.");
        game->running = false;
        return;
    }

    if (hits_body(&game->snake, next_head, growing)) {
        set_end_message(game, "Game over: the snake ran into itself.");
        game->running = false;
        return;
    }

    tail_copy = game->snake.segments[game->snake.length - 1];
    new_length = game->snake.length + grow_amount;
    if (new_length > game->snake.capacity) {
        new_length = game->snake.capacity;
    }

    for (segment_index = game->snake.length - 1; segment_index > 0; --segment_index) {
        game->snake.segments[segment_index] = game->snake.segments[segment_index - 1];
    }

    game->snake.segments[0] = next_head;

    for (segment_index = game->snake.length; segment_index < new_length; ++segment_index) {
        game->snake.segments[segment_index] = tail_copy;
    }

    game->snake.length = new_length;

    if (game->snake.length >= game->winning_length) {
        set_end_message(game, "You win: the snake reached the target length.");
        game->running = false;
        return;
    }

    if (growing) {
        spawn_trophy(game);
    }
}

static int frame_delay_ms(const Game *game) {
    /* Chris | pacing math: shorten the frame delay as the snake gets longer. */
    int delay;

    delay = FRAME_DELAY_MS - (game->snake.length - INITIAL_LENGTH) * 3;

    if (delay < MIN_FRAME_DELAY_MS) {
        delay = MIN_FRAME_DELAY_MS;
    }

    return delay;
}

static void pause_between_frames(const Game *game) {
    /* Chris | pacing: make the snake's speed increase with its current length. */
    napms(frame_delay_ms(game));
}

static void cleanup_game(Game *game) {
    /* Chris | resource cleanup: free the snake buffer before the program exits. */
    free(game->snake.segments);
    game->snake.segments = NULL;
}

int main(void) {
    /* Giovanni -> main flow: initialize, loop through frames, then shut everything down cleanly. */
    Game game;

    install_signal_handlers();
    initialize_curses();
    initialize_game(&game);
    draw_game(&game);

    while (game.running) {
        pause_between_frames(&game);

        if (shutdown_requested) {
            set_end_message(&game, "Game interrupted.");
            game.running = false;
            break;
        }

        poll_input(&game);
        if (!game.running) {
            break;
        }

        advance_snake(&game);
        if (!game.running) {
            break;
        }

        draw_game(&game);
    }

    cleanup_curses();
    cleanup_game(&game);
    printf("%s\n", game.end_message);

    return 0;
}
