/** @file
 * Implementacja modułu rozgrywki gry gamma w trybie interaktywnym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "errors.h"
#include "gamma.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>  //STDIN_FILENO

/** Kod ASCII 4, odpowiednik EOF przy wczytywaniu bez buforowania */
#define END_OF_TRANSMISSION 4

static inline void move_cursor(uint32_t column, uint32_t row) {
    printf("\x1b[%u;%uH", row, column);
}

static error_t print_board(gamma_t *g, uint32_t field_x, uint32_t field_y,
                           uint32_t player) {
    printf("\x1b[2J"); // clear screen & nuke scrollback
    move_cursor(0, 0);
    const uint32_t board_width = gamma_board_width(g);

    for (int64_t y = gamma_board_height(g) - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < board_width; x++) {
            // unsigned field_width = x == 0 ? min_first_column_width : min_width;
            // TODO szerokosc pola
            unsigned field_width = 1;
            int written_chars;
            char buffer[15];
            gamma_render_field(g, buffer, x, y, field_width, &written_chars);
            if (written_chars < 0) {
                return MEMORY_ERROR;
            }
            if (y == field_y && x == field_x) {
                printf("\x1b[31;105m%s\x1b[m", buffer); // TODO opisac
            } else {
                printf("%s", buffer);
            }
        }
        printf("\n");
    }

    printf("Player %u\nFree fields %lu\nOccupied fields %lu\n", player,
           gamma_free_fields(g, player), gamma_busy_fields(g, player));
    return NO_ERROR;
}

static void cleanup() {
    printf("\x1b[2J");     // clean up the alternate buffer
    printf("\x1b[?1049l"); // switch back to the normal buffer
    printf("\x1b[?25h");   // show the cursor again
}

static void restore_terminal_settings(struct termios *old) {
    tcsetattr(STDIN_FILENO, TCSANOW, old);
    cleanup();
}

static void adjust_terminal_settings(struct termios *old, struct termios *new) {
    /* tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt */
    tcgetattr(STDIN_FILENO, old);
    *new = *old;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    new->c_lflag &= ~((uint32_t)ICANON | (uint32_t)ECHO);

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr(STDIN_FILENO, TCSANOW, new);

    // TODO pytanie: co z cleanupami przy erorrach?

    printf("\x1b[?1049h"); // set alternative buffer
    printf("\x1b[2J");     // clear screen & nuke scrollback
    printf("\x1b[?25l");   // hide cursor
}

void respond_to_arrow_key(gamma_t *g, uint32_t *field_x, uint32_t *field_y) {
    // Sekwencja oznaczająca strzałkę to 27 91 (65|66|67|68)
    int key;
    if ((key = getchar()) != 27) {
        ungetc(key, stdin);
        return;
    }
    if ((key = getchar()) != 91) {
        ungetc(key, stdin);
        return;
    }
    key = getchar();
    if (key == 65) { // Strzałka w górę
        *field_y = *field_y + 1 < gamma_board_height(g) ? *field_y + 1 : *field_y;
    } else if (key == 66) { // Strzałka w dół.
        *field_y = *field_y > 0 ? *field_y - 1 : 0;
    } else if (key == 67) { // Strzałka w prawo.
        *field_x = *field_x + 1 < gamma_board_width(g) ? *field_x + 1 : *field_x;
    } else if (key == 68) { // Strzałka w lewo.
        *field_x = *field_x > 0 ? *field_x - 1 : 0;
    }
}

void respond_to_key(char key, gamma_t *game, uint32_t *field_x, uint32_t *field_y,
                    uint32_t player, bool *advance_player) {
    *advance_player = false;
    // TODO advance only if successful?
    // TODO printowac komunikat ze sie nie udalo?
    if (key == ' ') {
        gamma_move(game, player, *field_x, *field_y);
        *advance_player = true;
    } else if (key == 'c' || key == 'C') {
        *advance_player = true;
    } else if (key == 'g' || key == 'G') {
        gamma_golden_move(game, player, *field_x, *field_y);
        *advance_player = true;
    } else if (key == 27) { // Jeden z klawiszy strzałek.
        ungetc(key, stdin);
        respond_to_arrow_key(game, field_x, field_y);
    }
}

static inline void print_game_summary(gamma_t *g) {
    char *rendered_board = gamma_board(g);
    if (rendered_board != NULL) {
        printf("%s", rendered_board);
    }
    // TODO summary graczy
}

static inline bool advance_player_number(uint32_t *current_player,
                                         const uint32_t players) {
    *current_player = (*current_player % players) + 1;
    return false;
}

static error_t run_io_loop(gamma_t *g) {
    uint32_t field_x = 0;
    uint32_t field_y = 0;
    uint32_t current_player = 1;
    const uint32_t players = gamma_players_number(g);

    while (true) {
        error_t error = print_board(g, field_x, field_y, current_player);
        if (error == MEMORY_ERROR) {
            return MEMORY_ERROR;
        }

        int c = getchar();
        if (c == END_OF_TRANSMISSION) { // ctrl+d
            return NO_ERROR;
        }

        bool advance_player;
        respond_to_key((char)c, g, &field_x, &field_y, current_player, &advance_player);
        if (advance_player) {
            bool game_ended = advance_player_number(&current_player, players);
            if (game_ended) {
                return NO_ERROR;
            }
        }
    }
}

void run_interactive_mode(gamma_t *g) {
    struct termios old_settings, new_settings;
    adjust_terminal_settings(&old_settings, &new_settings);
    error_t error = run_io_loop(g);
    restore_terminal_settings(&old_settings);
    if (error == NO_ERROR) {
        print_game_summary(g);
    }
}
