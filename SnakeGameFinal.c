#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define INITIAL_LENGTH 5
#define FRAME_DELAY_MS 140L

typedef struct {
    int row;
    int col;
} Point;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef enum {
    ESC_IDLE,
    ESC_SEEN,
    ESC_BRACKET
} EscapeState;

typedef struct {
    Point *segments;
    int length;
    int capacity;
    Direction direction;
} Snake;

typedef struct {
    int rows;
    int cols;
    Snake snake;
    EscapeState escape_state;
    bool running;
    char end_message[128];
} Game;

static struct termios original_termios;
static bool terminal_configured = false;
static volatile sig_atomic_t interrupted = 0;

static void restore_terminal(void) {
    if (terminal_configured) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        terminal_configured = false;
    }

    fputs("\033[0m\033[?25h", stdout);
    fflush(stdout);
}

static void handle_signal(int signum) {
    (void)signum;
    interrupted = 1;
}

static void die(const char *message) {
    restore_terminal();
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void install_signal_handlers(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGQUIT, &sa, NULL) == -1) {
        die("Unable to install signal handlers.");
    }
}

static void enable_raw_mode(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        die("snake must be run in an interactive terminal.");
    }

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        die("Unable to read terminal settings.");
    }

    raw = original_termios;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("Unable to switch terminal to raw mode.");
    }

    terminal_configured = true;
    atexit(restore_terminal);
}

static void get_terminal_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
        ws.ws_row == 0 || ws.ws_col == 0) {
        die("Unable to determine terminal size.");
    }

    *rows = (int) ws.ws_row;
    *cols = (int) ws.ws_col;
}

static bool is_opposite(Direction current, Direction next) {
    return (current == DIR_UP && next == DIR_DOWN) ||
           (current == DIR_DOWN && next == DIR_UP) ||
           (current == DIR_LEFT && next == DIR_RIGHT) ||
           (current == DIR_RIGHT && next == DIR_LEFT);
}

static void set_end_message(Game *game, const char *message) {
    snprintf(game->end_message, sizeof(game->end_message), "%s", message);
}

static void initialize_snake(Game *game) {
    int center_row;
    int head_col;
    int i;

    game->snake.capacity = (game->rows - 2) * (game->cols - 2);
    game->snake.length = INITIAL_LENGTH;
    game->snake.direction = DIR_RIGHT;
    game->snake.segments = malloc((size_t) game->snake.capacity * sizeof(Point));

    if (game->snake.segments == NULL) {
        die("Unable to allocate memory for the snake.");
    }

    center_row = game->rows / 2;
    head_col = game->cols / 2;
    if (head_col < INITIAL_LENGTH) {
        head_col = INITIAL_LENGTH;
    }

    for (i = 0; i < game->snake.length; ++i) {
        game->snake.segments[i].row = center_row;
        game->snake.segments[i].col = head_col - i;
    }
}

static void initialize_game(Game *game) {
    memset(game, 0, sizeof(*game));
    get_terminal_size(&game->rows, &game->cols);

    if (game->rows < 7 || game->cols < 12) {
        die("Terminal window is too small for the game.");
    }

    initialize_snake(game);
    game->escape_state = ESC_IDLE;
    game->running = true;
    set_end_message(game, "Game ended.");
}

static char snake_char_at(const Game *game, int row, int col) {
    int i;

    for (i = 0; i < game->snake.length; ++i) {
        if (game->snake.segments[i].row == row &&
            game->snake.segments[i].col == col) {
            return (i == 0) ? '@' : 'o';
        }
    }

    return ' ';
}

static void draw_game(const Game *game) {
    int row;
    int col;

    fputs("\033[2J\033[H\033[?25l", stdout);

    for (row = 0; row < game->rows; ++row) {
        for (col = 0; col < game->cols; ++col) {
            if (row == 0 || row == game->rows - 1 ||
                col == 0 || col == game->cols - 1) {
                putchar('#');
            } else {
                putchar(snake_char_at(game, row, col));
            }
        }
        if (row < game->rows - 1) {
            putchar('\n');
        }
    }

    fflush(stdout);
}

static void apply_direction(Game *game, Direction next_direction) {
    if (is_opposite(game->snake.direction, next_direction)) {
        set_end_message(game, "Game over: reverse direction is not allowed.");
        game->running = false;
        return;
    }

    game->snake.direction = next_direction;
}

static void handle_keypress(Game *game, char ch) {
    switch (game->escape_state) {
        case ESC_IDLE:
            if (ch == '\033') {
                game->escape_state = ESC_SEEN;
            } else if (ch == 'q' || ch == 'Q') {
                set_end_message(game, "Game ended by user.");
                game->running = false;
            }
            break;

        case ESC_SEEN:
            if (ch == '[') {
                game->escape_state = ESC_BRACKET;
            } else {
                game->escape_state = ESC_IDLE;
            }
            break;

        case ESC_BRACKET:
            if (ch == 'A') {
                apply_direction(game, DIR_UP);
            } else if (ch == 'B') {
                apply_direction(game, DIR_DOWN);
            } else if (ch == 'C') {
                apply_direction(game, DIR_RIGHT);
            } else if (ch == 'D') {
                apply_direction(game, DIR_LEFT);
            }
            game->escape_state = ESC_IDLE;
            break;
    }
}

static void poll_input(Game *game) {
    char ch;
    ssize_t bytes_read;

    do {
        bytes_read = read(STDIN_FILENO, &ch, 1);
        if (bytes_read == 1) {
            handle_keypress(game, ch);
        }
    } while (bytes_read == 1 && game->running);

    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        set_end_message(game, "Game ended: input error.");
        game->running = false;
    }
}

static bool hits_body(const Snake *snake, Point next_head) {
    int i;

    for (i = 0; i < snake->length - 1; ++i) {
        if (snake->segments[i].row == next_head.row &&
            snake->segments[i].col == next_head.col) {
            return true;
        }
    }

    return false;
}

static Point next_head_position(const Snake *snake) {
    Point next = snake->segments[0];

    switch (snake->direction) {
        case DIR_UP:
            --next.row;
            break;
        case DIR_DOWN:
            ++next.row;
            break;
        case DIR_LEFT:
            --next.col;
            break;
        case DIR_RIGHT:
            ++next.col;
            break;
    }

    return next;
}

static void advance_snake(Game *game) {
    Point next_head;
    int i;

    next_head = next_head_position(&game->snake);

    if (next_head.row <= 0 || next_head.row >= game->rows - 1 ||
        next_head.col <= 0 || next_head.col >= game->cols - 1) {
        set_end_message(game, "Game over: the snake hit the border.");
        game->running = false;
        return;
    }

    if (hits_body(&game->snake, next_head)) {
        set_end_message(game, "Game over: the snake ran into itself.");
        game->running = false;
        return;
    }

    for (i = game->snake.length - 1; i > 0; --i) {
        game->snake.segments[i] = game->snake.segments[i - 1];
    }

    game->snake.segments[0] = next_head;
}

static void sleep_for_frame(void) {
    struct timespec delay;

    delay.tv_sec = FRAME_DELAY_MS / 1000L;
    delay.tv_nsec = (FRAME_DELAY_MS % 1000L) * 1000000L;

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
        continue;
    }
}

int main(void) {
    Game game;

    install_signal_handlers();
    enable_raw_mode();
    initialize_game(&game);

    draw_game(&game);

    while (game.running) {
        sleep_for_frame();

        if (interrupted) {
            set_end_message(&game, "Game interrupted.");
            game.running = false;
            break;
        }

        poll_input(&game);
        if (!game.running) {
            break;
        }

        advance_snake(&game);
        if (game.running) {
            draw_game(&game);
        }
    }

    restore_terminal();
    free(game.snake.segments);
    printf("%s\n", game.end_message);

    return 0;
}
