/** @file
 * Implementacja modułu rozgrywki gry gamma w trybie interaktywnym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

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

/** @brief Wypisuje aktualny stan planszy.
 * @param[in,out] g           – wskaźnik na strukturę danych gry,
 * @param[in] field_x         – numer kolumny kursora,
 * @param[in] field_y         – numer wiersza kursora,
 * @param[in] player          – numer gracza dokonującego ruch.
 */
static void print_board(gamma_t *g, uint32_t field_x, uint32_t field_y,
                        uint32_t player) {
    // Maksymalna szerokość pola to ceil(log10(UINT32_MAX)) = 10
    const int base_field_width = snprintf(NULL, 0, "%" PRIu32, gamma_players_number(g));
    const uint32_t board_width = gamma_board_width(g);

    for (int64_t y = gamma_board_height(g) - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < board_width; x++) {
            int field_width = base_field_width + (x == 0 ? 0 : 1);
            int written_chars;
            char buffer[15];
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

    print_board(g, field_x, field_y, player);

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
 * @param[in] g               – wskaźnik na strukturę danych gry,
 * @param[in,out] field_x     – wskaźnik na numer kolumny kursora,
 * @param[in,out] field_y     – wskaźnik na numer wiersza kursora.
 */
void respond_to_arrow_key(const gamma_t *g, uint32_t *field_x, uint32_t *field_y) {
    // Sekwencja oznaczająca strzałkę to \escape [ 65|66|67|68.
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
    if (key == 65) { // Strzałka w górę
        *field_y = *field_y + 1 < gamma_board_height(g) ? *field_y + 1 : *field_y;
    } else if (key == 66) { // Strzałka w dół.
        *field_y = *field_y > 0 ? *field_y - 1 : 0;
    } else if (key == 67) { // Strzałka w prawo.
        *field_x = *field_x + 1 < gamma_board_width(g) ? *field_x + 1 : *field_x;
    } else if (key == 68) { // Strzałka w lewo.
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
 *                              zostać ewentualny kod błędu.
 */
void respond_to_key(char key, gamma_t *game, uint32_t *field_x, uint32_t *field_y,
                    uint32_t player, bool *advance_player, char *error_message) {
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

/** @brief Dostosowuje ustawienia terminala do wymagań trybu interaktywnego.
 * Jeżeli rozmiar okna terminala jest zbyt mały, aby poprawnie wyświetlić cały
 * interfejs zapisuje do bufora komunikat ostrzeżenia.
 * @param[out] old            – wskaźnik na strukturę, w której zapisane zostaną
 *                              oryginalne ustawienia termianala,
 * @param[out] new            – wskaźnik na strukturę, w której zapisane zostaną nowe
 *                              ustawienia termianala,
 * @param[out] error_message  – wskaźnik na bufor znakowy, do którego zapisany zostanie
 *                              ewentualny komunikat błędu,
 * @param[in] game            – wskaźnik na strukturę przechowującą stan gry.
 */
static void adjust_terminal_settings(struct termios *old, struct termios *new,
                                     char *error_message, const gamma_t *game) {
    tcgetattr(STDIN_FILENO, old);
    *new = *old;

    // ICANON - tryb obsługi wejścia (buforowanie),
    // ECHO - wypisywanie naciśniętego klawisza na stdout.
    new->c_lflag &= ~((uint32_t)ICANON | (uint32_t)ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, new);

    printf(SET_ALTERNATIVE_BUFFER CLEAR_SCREEN HIDE_CURSOR);

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    static unsigned const extra_rows_under_board = 4;
    const unsigned board_field_width =
        snprintf(NULL, 0, "%" PRIu32, gamma_players_number(game));
    if (ws.ws_row < gamma_board_height(game) + extra_rows_under_board ||
        ws.ws_col < gamma_board_width(game) * board_field_width) {
        sprintf(error_message, "Terminal size is too small to display the "
                               "whole board. Please resize the window.");
    }
}

/** @brief Przywraca terminal do pierwotnych ustawień.
 * @param[in] old          – wskaźnik na strukturę przechowującą oryginalne
 *                           ustawienia termianala.
 */
static void restore_terminal_settings(struct termios *old) {
    tcsetattr(STDIN_FILENO, TCSANOW, old);
    printf(CLEAR_SCREEN SET_NORMAL_BUFFER SHOW_CURSOR);
}

/** @brief Wyświetla informację o zwycięzcy gry lub o remisie.
 * @param[in] g          – wskaźnik na strukturę danych gry.
 */
static inline void print_game_winner(gamma_t *g) {
    uint32_t players_count = gamma_players_number(g);

    uint32_t winner = 1;
    uint64_t winner_fields = gamma_busy_fields(g, 1);
    bool sole_winner = true;
    for (uint64_t p = 2; p <= players_count; p++) {
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
    for (uint64_t p = 1; p <= players_count; p++) {
        uint64_t player_points = gamma_busy_fields(g, p);
        printf("Player %" PRIu64 ",\tbusy fields %" PRIu64 "\n", p, player_points);
    }

    print_game_winner(g);
    return NO_ERROR;
}

io_error_t run_interactive_mode(gamma_t *g) {
    struct termios old_settings, new_settings;
    // "Pusta" inicjacja wymagana, ponieważ valgrind przekazanie niezainicjowanej
    // struktury do tcgetattr uważa za błąd (mimo, że jest to według dokumentacji
    // poprawne - tcgetattr ustawia wartości pól na prawidłowe)
    memset(&old_settings, 0, sizeof(struct termios));
    static char error_message[100];
    error_message[0] = '\0';

    adjust_terminal_settings(&old_settings, &new_settings, error_message, g);
    io_error_t error = run_io_loop(g, error_message);
    restore_terminal_settings(&old_settings);
    if (error == NO_ERROR) {
        error = print_game_summary(g);
    }
    return error;
}
