/** @file
 * Implementacja modułu rozgrywki gry gamma w trybie interaktywnym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

/** _GNU_SOURCE - wymagane, aby stdio.h definiowało funkcję fileno */
#define _GNU_SOURCE

#include "interactive_mode.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/** Kod ASCII 4 wysyłany przy naciśnięciu klawiszy ctrl+d */
#define END_OF_TRANSMISSION 4
/** Kod ASCII 27 - znak escape */
#define ESCAPE 27
/** Kod ASCII 91 - znak otwierający nawias kwadratowy - [ */
#define OPENING_SQUARE_BRACKET 91

typedef enum arrow_key_sequence_last_character {
    ARROW_UP = 65,
    ARROW_DOWN = 66,
    ARROW_RIGHT = 67,
    ARROW_LEFT = 68,
} arrow_key_sequence_last_character;

/** ANSI escape code - wyczyszczenie bufora terminala. */
#define CLEAR_SCREEN "\x1b[2J"
/** ANSI escape code - przesunięcie kursora na zadaną pozycję. */
#define MOVE_CURSOR "\x1b[%u;%uH"
/** ANSI escape code - ukrycie kursora. */
#define HIDE_CURSOR "\x1b[?25l"
/** ANSI escape code - przywrócenie widoczności kursora. */
#define SHOW_CURSOR "\x1b[?25h"
/** ANSI escape code - przełączenie na alternatywny bufor terminala. */
#define SET_ALTERNATIVE_BUFFER "\x1b[?1049h"
/** ANSI escape code - przełączenie na glówny bufor terminala. */
#define SET_NORMAL_BUFFER "\x1b[?1049l"
/** ANSI escape code - przywrócenie domyślnych kolorów */
#define RESET_COLORS "\x1b[m"
/** ANSI escape code - zmiana koloru tła na biały */
#define WHITE_BACKGROUND "\x1b[107m"
/** ANSI escape code - zmiana koloru tła na zielony */
#define GREEN_BACKGROUND "\x1b[42m"
/** ANSI escape code - zmiana koloru tekstu na żółty */
#define YELLOW_TEXT "\x1b[38;5;226m"
/** ANSI escape code - zmiana koloru tekstu na złoty */
#define GOLDEN_TEXT "\x1b[38;5;178m"
/** ANSI escape code - zmiana koloru tekstu na czerwony */
#define RED_TEXT "\x1b[31m"
/** ANSI escape code - zmiana koloru tekstu na czarny tekst */
#define BLACK_TEXT "\x1b[30m"

/** Ograniczenie górne długości znakowej reprezentacji jednego pola
 * 15 > ceil(log10(UINT32_MAX)) = 10 */
#define FIELD_WIDTH_UPPER_BOUND 15

/** @brief Wypisuje aktualny stan planszy.
 * @param[in,out] g           – wskaźnik na strukturę danych gry,
 * @param[in] field_x         – numer kolumny kursora,
 * @param[in] field_y         – numer wiersza kursora,
 * @param[in] player          – numer gracza dokonującego ruch.
 */
static void board_print(gamma_t *g, uint32_t field_x, uint32_t field_y,
                        uint32_t player) {
    const int base_field_width = snprintf(NULL, 0, "%" PRIu32, gamma_players_number(g));
    const uint32_t board_width = gamma_board_width(g);

    for (uint32_t y = gamma_board_height(g); y-- > 0;) {
        for (uint32_t x = 0; x < board_width; x++) {
            int field_width = base_field_width + (x == 0 ? 0 : 1);
            int written_chars;
            char buffer[FIELD_WIDTH_UPPER_BOUND];
            uint32_t player_number;
            gamma_render_field(g, buffer, x, y, field_width, &written_chars,
                               &player_number);
            if (y == field_y && x == field_x) {
                printf(WHITE_BACKGROUND BLACK_TEXT "%s" RESET_COLORS, buffer);
            } else if (player_number == player) {
                printf(GREEN_BACKGROUND BLACK_TEXT "%s" RESET_COLORS, buffer);
            } else if (player_number == 0) {
                printf(YELLOW_TEXT "%s" RESET_COLORS, buffer);
            } else {
                printf("%s", buffer);
            }
        }
        printf("\n");
    }
}

/** @brief Aktualizuje planszę i wyświetlane informacje na ekranie terminala.
 * Czyści aktualny bufor terminala i wypisuje aktualny stan planszy, wiersz
 * zachęcający gracza do dokonania ruchu, oraz ewentualny komunikat błędu.
 * @param[in,out] g           – wskaźnik na strukturę danych gry,
 * @param[in] field_x         – numer kolumny kursora,
 * @param[in] field_y         – numer wiersza kursora,
 * @param[in] player          – numer gracza dokonującego ruch,
 * @param[out] error_message  – wskaźnik na bufor znakowy zawierający komunikat błędu.
 */
static void rerender_screen(gamma_t *g, uint32_t field_x, uint32_t field_y,
                            uint32_t player, char *error_message) {
    printf(CLEAR_SCREEN);
    printf(MOVE_CURSOR, 0, 0);

    board_print(g, field_x, field_y, player);

    printf("\nPlayer %" PRIu32 "\n", player);
    printf("Busy fields %" PRIu64 "\tFree fields %" PRIu64 "\n",
           gamma_busy_fields(g, player), gamma_free_fields(g, player));
    if (gamma_golden_possible(g, player)) {
        printf(GOLDEN_TEXT "Golden move possible" RESET_COLORS);
    }
    if (error_message[0] != '\0') {
        printf(RED_TEXT "\n%s\n" RESET_COLORS, error_message);
    }
}

/** @brief Przesuwa kursor na podstawie wciśniętego klawisza strzałki.
 * Jeżeli niemożliwe jest wczytanie pełnej sekwencji strzałki, początkowe znaki zostaną
 * zignorowane.
 * @param[in] g               – wskaźnik na strukturę danych gry,
 * @param[in,out] field_x     – wskaźnik na numer kolumny kursora,
 * @param[in,out] field_y     – wskaźnik na numer wiersza kursora.
 */
static inline void respond_to_arrow_key(const gamma_t *g, uint32_t *field_x,
                                        uint32_t *field_y) {
    // Sekwencja strzałki to 3 znaki ESC [ ARROW_UP|ARROW_DOWN|ARROW_RIGHT|ARROW_LEFT
    int key;
    if ((key = getchar()) != ESCAPE) {
        ungetc(key, stdin);
        return;
    }
    if ((key = getchar()) != OPENING_SQUARE_BRACKET) {
        ungetc(key, stdin);
        return;
    }
    key = getchar();
    if (key == ARROW_UP) {
        *field_y = *field_y + 1 < gamma_board_height(g) ? *field_y + 1 : *field_y;
    } else if (key == ARROW_DOWN) {
        *field_y = *field_y > 0 ? *field_y - 1 : 0;
    } else if (key == ARROW_RIGHT) {
        *field_x = *field_x + 1 < gamma_board_width(g) ? *field_x + 1 : *field_x;
    } else if (key == ARROW_LEFT) {
        *field_x = *field_x > 0 ? *field_x - 1 : 0;
    } else {
        ungetc(key, stdin);
    }
}

/** @brief Reaguje na działanie użytkownika.
 * Wykonuje odpowiednią akcję na podstawie klawisza wciśniętego przez gracza.
 * @param[in] key             – kod znaku odpowiadającego wciśniętemu klawiszowi,
 * @param[in,out] game        – wskaźnik na strukturę danych gry,
 * @param[in,out] field_x     – wskaźnik na numer kolumny kursora,
 * @param[in,out] field_y     – wskaźnik na numer wiersza kursora,
 * @param[in] player          – numer gracza dokonującego ruch,
 * @param[out] advance_player – wskaźnik na wartość logiczną oznaczającą, czy należy
 *                              przesunąć kolejkę na następnego gracza,
 * @param[out] error_message  – wskaźnik na bufor znakowy, do którego zapisany może
 *                              zostać ewentualny kod błędu; bufor musi być odpowiednio
 *                              duży, aby pomieścić cały komunikat.
 */
static void respond_to_key(char key, gamma_t *game, uint32_t *field_x,
                           uint32_t *field_y, uint32_t player, bool *advance_player,
                           char *error_message) {
    *advance_player = false;
    error_message[0] = '\0';
    if (key == ' ') {
        bool success = gamma_move(game, player, *field_x, *field_y);
        if (success) {
            *advance_player = true;
        } else {
            sprintf(error_message, "Can't make this move.");
        }
    } else if (key == 'c' || key == 'C') {
        *advance_player = true;
    } else if (key == 'g' || key == 'G') {
        bool success = gamma_golden_move(game, player, *field_x, *field_y);
        if (success) {
            *advance_player = true;
        } else {
            sprintf(error_message, "Can't make this golden move.");
        }
    } else if (key == ESCAPE) {
        ungetc(key, stdin);
        respond_to_arrow_key(game, field_x, field_y);
    }
}

/** @brief Wyznacza następnego w kolejności gracza, który może dokonać ruchu.
 * @param[in] g             – wskaźnik na strukturę danych gry,
 * @param[in,out] player    – wskaźnik na numer aktualnego gracza, który zostanie
 *                            zaktualizowany.
 * @return Wartość logiczną @p true, jeżeli znaleziono następnego gracza, lub
 * @p false, jeżeli żaden gracz nie może dokonać ruchu i należy zakończyć grę.
 */
static inline bool advance_player_number(gamma_t *g, uint32_t *player) {
    const uint32_t players = gamma_players_number(g);
    uint32_t next_player = (*player % players) + 1;
    const uint32_t next_player_guard = next_player;

    if (gamma_free_fields(g, next_player) != 0 ||
        gamma_golden_possible(g, next_player)) {
        *player = next_player;
        return true;
    }
    do {
        next_player = (next_player % players) + 1;
        if (gamma_free_fields(g, next_player) != 0 ||
            gamma_golden_possible(g, next_player)) {
            *player = next_player;
            return true;
        }
    } while (next_player != next_player_guard);

    // Żaden gracz nie może wykonać już ruchu.
    return false;
}

/** @brief Wczytuje ruchy użytkownika, reaguje na nie i aktualizuje planszę.
 * @param[in,out] g           – wskaźnik na strukturę danych gry,
 * @param[out] error_message  – wskaźnik na bufor znakowy, do którego zapisywane będą
 *                              zostać ewentualne komunikaty błędów.
 * @return Kod @p NO_ERROR jeżeli gra została zakończona poprawnie, @p ENCOUNTERED_EOF
 * jeżeli wejście zostało zamknięte przed poprawnym zakończeniem gry.
 */
static io_error_t run_io_loop(gamma_t *g, char *error_message) {
    uint32_t field_x = 0, field_y = 0, current_player = 1;

    while (true) {
        rerender_screen(g, field_x, field_y, current_player, error_message);

        int c = getchar();
        if (c == END_OF_TRANSMISSION) {
            return NO_ERROR;
        } else if (c == EOF) {
            return ENCOUNTERED_EOF;
        }

        bool advance_player;
        respond_to_key((char)c, g, &field_x, &field_y, current_player, &advance_player,
                       error_message);
        if (advance_player) {
            bool game_continues = advance_player_number(g, &current_player);
            if (!game_continues) {
                return NO_ERROR;
            }
        }
    }
}

/** @brief Ustawia komunikat błędu jeżeli okno terminala jest zbyt małe.
 * Jeżeli rozmiar okna terminala jest zbyt mały, aby poprawnie wyświetlić cały
 * interfejs zapisuje do bufora komunikat ostrzeżenia.
 * @param[out] error_message  – wskaźnik na bufor znakowy, do którego zapisany zostanie
 *                              ewentualny komunikat błędu,
 * @param[in] game            – wskaźnik na strukturę przechowującą stan gry.
 * Kod @p NO_ERROR operacja została wykonana pomyślnie, @p TERMINAL_ERROR
 * jeżeli wywołanie systemowe zakończyło się błędem.
 */
static inline io_error_t check_if_terminal_window_is_big_enough(char *error_message,
                                                                const gamma_t *game) {
    struct winsize ws;
    int error = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    if (error != 0) {
        return TERMINAL_ERROR;
    }
    static unsigned const extra_rows_under_board = 4;
    const unsigned board_field_width =
        snprintf(NULL, 0, "%" PRIu32, gamma_players_number(game));
    if (ws.ws_row < gamma_board_height(game) + extra_rows_under_board ||
        ws.ws_col < gamma_board_width(game) * board_field_width) {
        sprintf(error_message, "Terminal size is too small to display the "
                               "whole board. Please resize the window.");
    }

    return NO_ERROR;
}

/** @brief Dostosowuje ustawienia terminala do wymagań trybu interaktywnego.
 * Jeżeli rozmiar okna terminala jest zbyt mały, aby poprawnie wyświetlić cały
 * interfejs zapisuje do bufora komunikat ostrzeżenia.
 * Jeżeli stdin nie jest podłączony do interaktywnego terminala, struktury old i new
 * nie zostaną uzupełnione danymi.
 * @param[out] old            – wskaźnik na strukturę, w której zapisane zostaną
 *                              oryginalne ustawienia terminala,
 * @param[out] new            – wskaźnik na strukturę, w której zapisane zostaną nowe
 *                              ustawienia terminala,
 * @param[out] error_message  – wskaźnik na bufor znakowy, do którego zapisany zostanie
 *                              ewentualny komunikat błędu,
 * @param[in] game            – wskaźnik na strukturę przechowującą stan gry.
 * Kod @p NO_ERROR operacja została wykonana pomyślnie, @p TERMINAL_ERROR
 * jeżeli wywołanie systemowe zakończyło się błędem.
 */
static io_error_t adjust_terminal_settings(struct termios *old, struct termios *new,
                                           char *error_message, const gamma_t *game) {
    bool is_interactive_terminal = isatty(fileno(stdin)) == 1;
    if (is_interactive_terminal) {
        int error = tcgetattr(STDIN_FILENO, old);
        if (error != 0) {
            return TERMINAL_ERROR;
        }
        *new = *old;

        new->c_lflag &= ~((uint32_t)ICANON | (uint32_t)ECHO);
        error = tcsetattr(STDIN_FILENO, TCSANOW, new);
        if (error != 0) {
            return TERMINAL_ERROR;
        }
    }

    printf(SET_ALTERNATIVE_BUFFER CLEAR_SCREEN HIDE_CURSOR);

    if (is_interactive_terminal) {
        return check_if_terminal_window_is_big_enough(error_message, game);
    }

    return NO_ERROR;
}

/** @brief Przywraca terminal do pierwotnych ustawień.
 * Jeżeli stdin nie jest podłączony do interaktywnego terminala, funkcja nie zmienia
 * ustawień terminala.
 * @param[in] old          – wskaźnik na strukturę przechowującą oryginalne
 *                           ustawienia terminala.
 * Kod @p NO_ERROR operacja została wykonana pomyślnie, @p TERMINAL_ERROR
 * jeżeli wywołanie systemowe zakończyło się błędem.
 */
static inline io_error_t restore_terminal_settings(struct termios *old) {
    bool is_interactive_terminal = isatty(fileno(stdin)) == 1;
    if (is_interactive_terminal) {
        int error = tcsetattr(STDIN_FILENO, TCSANOW, old);
        if (error != 0) {
            return TERMINAL_ERROR;
        }
    }

    printf(CLEAR_SCREEN SET_NORMAL_BUFFER SHOW_CURSOR);
    return NO_ERROR;
}

/** @brief Wyświetla informację o zwycięzcy gry lub o remisie.
 * @param[in] g          – wskaźnik na strukturę danych gry.
 */
static inline void print_game_winner(gamma_t *g) {
    uint32_t players_count = gamma_players_number(g);

    uint32_t winner = 1;
    uint64_t winner_fields = gamma_busy_fields(g, 1);
    bool sole_winner = true;
    for (uint32_t p = 1; p++ < players_count;) {
        uint32_t player_fields = gamma_busy_fields(g, p);
        if (player_fields > winner_fields) {
            winner = p;
            winner_fields = player_fields;
            sole_winner = true;
        } else if (player_fields == winner_fields) {
            sole_winner = false;
        }
    }

    if (!sole_winner) {
        printf("\nThe game ended in a tie.\n\n");
    } else {
        printf("\nPlayer %" PRIu32 " wins the game with %" PRIu64 " fields.\n\n",
               winner, winner_fields);
    }
}

/** @brief Wyświetla planszę, statystyki graczy oraz informację o zwycięzcy.
 * @param[in] g          – wskaźnik na strukturę danych gry.
 * @return Kod @p MEMORY_ERROR, jeżeli wystąpił błąd alokacji pamięci, @p NO_ERROR
 * w przeciwnym przypadku.
 */
static inline io_error_t print_game_summary(gamma_t *g) {
    char *rendered_board = gamma_board(g);
    if (rendered_board == NULL) {
        return MEMORY_ERROR;
    }
    printf("\n%s\n", rendered_board);
    free(rendered_board);

    uint32_t players_count = gamma_players_number(g);
    for (uint32_t p = 0; p++ < players_count;) {
        uint64_t player_points = gamma_busy_fields(g, p);
        printf("Player %" PRIu32 ",\tbusy fields %" PRIu64 "\n", p, player_points);
    }

    print_game_winner(g);
    return NO_ERROR;
}

io_error_t run_interactive_mode(gamma_t *g) {
    struct termios old_settings, new_settings;
    memset(&old_settings, 0, sizeof(struct termios));
    static char error_message[100];
    error_message[0] = '\0';

    io_error_t error =
        adjust_terminal_settings(&old_settings, &new_settings, error_message, g);
    if (error == NO_ERROR) {
        error = run_io_loop(g, error_message);
    }
    io_error_t cleanup_error = restore_terminal_settings(&old_settings);
    if (error == NO_ERROR && cleanup_error == NO_ERROR) {
        error = print_game_summary(g);
    } else {
        return TERMINAL_ERROR;
    }
    return error;
}
