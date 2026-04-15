#define _POSIX_C_SOURCE 200809L

/*
 * ============================================================
 *  SNACK GAME PROJECT — GROUP ASSIGNMENT (3 STUDENTS)
 * ============================================================
 *
 *  Course Project: Classic Snake Game using ncurses (C)
 *
 *  Team Members & Responsibilities:
 *
 *  
 *   Brandon Price— INPUT + GAME CONTROL                    
 *   • Handles keyboard input                               
 *   • Controls game flow (quit, direction updates)         
 *  
 *
 *  
 *   Giovanni DiCenso — GAME LOGIC + MOVEMENT                      
 *   • Snake initialization                                 
 *   • Movement system and growth mechanics                 
 *   • Trophy spawning logic                                
 *  
 *
 *  
 *   Chris Deeb — RENDERING + COLLISIONS                     
 *   • Drawing snake, trophy, and borders                   
 *   • Collision detection (wall + self)                    
 *   • HUD / game display                                  
 *    
 */
#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#define MAX_LEN 500

/* A single coordinate on the screen */
typedef struct
{
    int y, x;
} Point;

/* Snake structure holding body and movement direction */
typedef struct
{
    Point body[MAX_LEN];
    int length;
    int dir;
} Snake;

/* Trophy structure (food object) */
typedef struct
{
    Point pos;
    int value;
    int active;
} Trophy;

/* Direction constants */
enum { UP, DOWN, LEFT, RIGHT };

/* ============================================================
 * Brandon Price — INPUT HANDLING
 * ============================================================ */

/*
 * Reads keyboard input from user.
 * Returns:
 *  - UP/DOWN/LEFT/RIGHT for movement
 *  - -1 for quit (q key)
 *  - -2 for no valid input
 */
int get_input()
{
    int ch = getch();

    switch (ch)
    {
        case KEY_UP: return UP;
        case KEY_DOWN: return DOWN;
        case KEY_LEFT: return LEFT;
        case KEY_RIGHT: return RIGHT;
        case 'q': return -1;
        default: return -2;
    }
}

/* ============================================================
 * Giovanni DiCenso — GAME INITIALIZATION & LOGIC
 * ============================================================ */

/*
 * Initializes snake at center of screen.
 * Snake starts with length 5 moving right.
 */
void init(Snake *s, int max_y, int max_x)
{
    s->length = 5;
    s->dir = RIGHT;

    int cy = max_y / 2;
    int cx = max_x / 2;

    /* Build initial horizontal snake body */
    for (int i = 0; i < s->length; i++)
    {
        s->body[i].y = cy;
        s->body[i].x = cx - i;
    }
}

/*
 * Spawns a trophy at a random location not on the snake.
 * Each trophy has a value from 1–9.
 */
void spawn_trophy(Trophy *t, Snake *s, int max_y, int max_x)
{
    int ok;

    do
    {
        ok = 1;

        t->pos.y = (rand() % (max_y - 2)) + 1;
        t->pos.x = (rand() % (max_x - 2)) + 1;
        t->value = (rand() % 9) + 1;

        /* Ensure trophy is not placed on snake body */
        for (int i = 0; i < s->length; i++)
        {
            if (s->body[i].y == t->pos.y &&
                s->body[i].x == t->pos.x)
            {
                ok = 0;
                break;
            }
        }

    } while (!ok);
}

/*
 * Calculates next head position based on direction.
 */
Point next_head(Point head, int dir)
{
    if (dir == UP) head.y--;
    if (dir == DOWN) head.y++;
    if (dir == LEFT) head.x--;
    if (dir == RIGHT) head.x++;

    return head;
}

/*
 * Moves snake forward.
 * If grow = 1, snake increases length (after eating trophy).
 */
void move_snake(Snake *s, Point new_head, int grow)
{
    /* Shift body forward */
    for (int i = s->length; i > 0; i--)
        s->body[i] = s->body[i - 1];

    /* Insert new head */
    s->body[0] = new_head;

    /* Increase length if snake ate trophy */
    if (grow)
        s->length++;
}

/* ============================================================
 * Chris Deeb — COLLISIONS + RENDERING
 * ============================================================ */

/*
 * Checks if snake hits wall boundaries.
 */
int hit_wall(Point h, int max_y, int max_x)
{
    return (h.y <= 0 || h.y >= max_y - 1 ||
            h.x <= 0 || h.x >= max_x - 1);
}

/*
 * Checks if snake collides with itself.
 */
int hit_self(Snake *s)
{
    Point h = s->body[0];

    for (int i = 1; i < s->length; i++)
        if (s->body[i].y == h.y && s->body[i].x == h.x)
            return 1;

    return 0;
}

/*
 * Draws the full game screen:
 * - border
 * - snake
 * - trophy
 * - HUD (score + goal)
 */
void draw(Snake *s, Trophy *t, int max_y, int max_x, int goal)
{
    clear();

    /* Draw top and bottom border */
    for (int x = 0; x < max_x; x++)
    {
        mvaddch(0, x, '#');
        mvaddch(max_y - 1, x, '#');
    }

    /* Draw left and right border */
    for (int y = 0; y < max_y; y++)
    {
        mvaddch(y, 0, '#');
        mvaddch(y, max_x - 1, '#');
    }

    /* Draw trophy */
    mvaddch(t->pos.y, t->pos.x, '0' + t->value);

    /* Draw snake head */
    mvaddch(s->body[0].y, s->body[0].x, 'O');

    /* Draw snake body */
    for (int i = 1; i < s->length; i++)
        mvaddch(s->body[i].y, s->body[i].x, 'o');

    /* HUD display */
    mvprintw(0, 2, "Len:%d Goal:%d", s->length, goal);

    refresh();
}

/* ============================================================
 * MAIN GAME LOOP
 * ============================================================ */

int main()
{
    srand(time(NULL));

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    Snake s;
    Trophy t;

    init(&s, max_y, max_x);
    spawn_trophy(&t, &s, max_y, max_x);

    int goal = (max_y + max_x) / 2;
    int grow = 0;

    while (1)
    {
        /* Handle input */
        int input = get_input();

        if (input == -1)
            break;

        if (input != -2)
            s.dir = input;

        /* Compute next move */
        Point next = next_head(s.body[0], s.dir);

        /* Collision check */
        if (hit_wall(next, max_y, max_x) || hit_self(&s))
            break;

        /* Trophy check */
        if (next.y == t.pos.y && next.x == t.pos.x)
        {
            grow = 1;
            spawn_trophy(&t, &s, max_y, max_x);
        }
        else
        {
            grow = 0;
        }

        /* Move snake */
        move_snake(&s, next, grow);

        /* Win condition */
        if (s.length >= goal)
            break;

        /* Render frame */
        draw(&s, &t, max_y, max_x, goal);

        /* Game speed */
        nanosleep(&(struct timespec){.tv_sec=0, .tv_nsec=90000000}, NULL);
    }

    /* Game over screen */
    clear();
    mvprintw(max_y / 2, max_x / 2 - 5, "Game Over");
    refresh();
    nodelay(stdscr, FALSE);
    getch();

    endwin();
    return 0;
}