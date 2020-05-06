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
#include <termios.h>
#include <unistd.h>

/** Kod ASCII 4 wysyłany przy naciśnięciu klawiszy ctrl+d */
#define END_OF_TRANSMISSION 4

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
/** ANSI escape code - zmiana kolorów na białe tło, czarny tekst */
#define INVERT_COLORS "\x1b[30;107m"
/** ANSI escape code - przywrócenie domyślnych kolorów */
#define RESET_COLORS "\x1b[m"

/** @brief Sprawdza czy otrzymany symbol oznacza koniec dostępnych danych.
 * @param[in] c           – kod oznaczający znak lub EOF.
 * @return @p true, jeżeli przekazany kod oznacza znak terminujący, @p false w
 * przeciwnym przypadku.
 */
static inline bool is_terminating_symbol(int c) {
    return c == END_OF_TRANSMISSION || c == EOF;
}

/** @brief Aktualizuje planszę na ekranie terminala.
 * Czyści aktualny bufor terminala i wypisuje aktualny stan planszy, wiersz
 * zachęcający gracza do dokonania ruchu, oraz ewentualny komunikat błędu.
 * @param[in,out] g           – wskaźnik na strukturę danych gry,
 * @param[in] field_x         – numer kolumny kursora,
 * @param[in] field_y         – numer wiersza kursora,
 * @param[in] player          – numer gracza dokonującego ruch,
 * @param[out] error_message  – wskaźnik na bufor znakowy zawierający komunikat błędu.
 */
static void print_board(gamma_t *g, uint32_t field_x, uint32_t field_y, uint32_t player,
                        char *error_message) {
    printf(CLEAR_SCREEN);
    printf(MOVE_CURSOR, 0, 0);
    const uint32_t board_width = gamma_board_width(g);
    unsigned first_column_width, other_columns_width;
    gamma_rendered_fields_width(g, &first_column_width, &other_columns_width);

    for (int64_t y = gamma_board_height(g) - 1; y >= 0; y--) {
        for (uint32_t x = 0; x < board_width; x++) {
            unsigned field_width = x == 0 ? first_column_width : other_columns_width;
            int written_chars;
            char buffer[15];
            gamma_render_field(g, buffer, x, y, field_width, &written_chars);
            if (y == field_y && x == field_x) {
                printf(INVERT_COLORS "%s" RESET_COLORS, buffer);
            } else {
                printf("%s", buffer);
            }
        }
        printf("\n");
    }

    printf("PLAYER %" PRIu32 " %" PRIu64 " %" PRIu64 " %c\n", player,
           gamma_busy_fields(g, player), gamma_free_fields(g, player),
           gamma_golden_possible(g, player) ? 'G' : ' ');
    if (error_message[0] != '\0') {
        printf("%s\n", error_message);
    }
}

/** @brief Przesuwa kursor na podstawie wciśniętego klawisza strzałki.
 * @param[in] g               – wskaźnik na strukturę danych gry,
 * @param[in,out] field_x     – wskaźnik na numer kolumny kursora,
 * @param[in,out] field_y     – wskaźnik na numer wiersza kursora.
 */
void respond_to_arrow_key(const gamma_t *g, uint32_t *field_x, uint32_t *field_y) {
    // Sekwencja oznaczająca strzałkę to 27 91 (65|66|67|68).
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
    } else if (key == 27) { // Jeden z klawiszy strzałek.
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
 * @param[in,out] g          – wskaźnik na strukturę danych gry.
 */
static void run_io_loop(gamma_t *g) {
    uint32_t field_x = 0, field_y = 0, current_player = 1;
    char error_message[100];
    error_message[0] = '\0';

    while (true) {
        print_board(g, field_x, field_y, current_player, error_message);

        int c = getchar();
        if (is_terminating_symbol(c)) {
            return;
        }

        bool advance_player;
        respond_to_key((char)c, g, &field_x, &field_y, current_player, &advance_player,
                       error_message);
        if (advance_player) {
            bool game_continues = advance_player_number(g, &current_player);
            if (!game_continues) {
                return;
            }
        }
    }
}

/** @brief Przeprowadza rozgrywkę w trybie wsadowym.
 * @param[out] new       – wskaźnik na strukturę, w której zapisane zostaną nowe
 *                         ustawienia termianala.
 * @param[out] old       – wskaźnik na strukturę, w której zapisane zostaną oryginalne
 *                         ustawienia termianala.
 */
static void adjust_terminal_settings(struct termios *old, struct termios *new) {
    tcgetattr(STDIN_FILENO, old);
    *new = *old;

    // ICANON - tryb obsługi wejścia (buforowanie, '\n', EOF).
    // ECHO - wypisywanie naciśniętego klawisza na stdout.
    new->c_lflag &= ~((uint32_t)ICANON | (uint32_t)ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, new);

    printf(SET_ALTERNATIVE_BUFFER CLEAR_SCREEN HIDE_CURSOR);
}

/** @brief Przeprowadza rozgrywkę w trybie wsadowym.
 * @param[in] old          – wskaźnik na strukturę przechowującą oryginalne
 *                           ustawienia termianala.
 */
static void restore_terminal_settings(struct termios *old) {
    tcsetattr(STDIN_FILENO, TCSANOW, old);
    printf(CLEAR_SCREEN SET_NORMAL_BUFFER SHOW_CURSOR);
}

/** @brief Wyświetla planszę i statystyki graczy.
 * @param[in] g          – wskaźnik na strukturę danych gry.
 */
static inline void print_game_summary(gamma_t *g) {
    char *rendered_board = gamma_board(g);
    if (rendered_board == NULL) {
        return;
    }
    printf("\n%s\n", rendered_board);
    free(rendered_board);

    uint32_t players_count = gamma_players_number(g);
    for (uint64_t p = 1; p <= players_count; p++) {
        uint64_t player_points = gamma_busy_fields(g, p);
        printf("PLAYER %" PRIu64 " %" PRIu64 "\n", p, player_points);
    }
}

void run_interactive_mode(gamma_t *g) {
    struct termios old_settings, new_settings;
    // "Pusta" inicjacja wymagana, ponieważ valgrind przekazanie niezainicjowanej
    // struktury do tcgetattr uważa za błąd (mimo, że jest to według dokumentacji
    // poprawne - tcgetattr ustawia wartości pól na prawidłowe)
    memset(&old_settings, 0, sizeof(struct termios));
    adjust_terminal_settings(&old_settings, &new_settings);
    run_io_loop(g);
    restore_terminal_settings(&old_settings);
    print_game_summary(g);
}
