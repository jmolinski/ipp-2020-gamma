/** @file
 * Plik zawiera logikę koordynującą rozpoczęcie i zakończenie rozgrywki w grze gamma.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "batch_mode.h"
#include "interactive_mode.h"
#include "text_input_handler.h"
#include <inttypes.h>
#include <stdio.h>

/** Wszystkie identyfikatory dozwolonych trybów rozgrywki. */
#define GAME_MODE_IDENTIFIERS "BI"

/** @brief Wczytuje parametry gry z stdin.
 * Wczytuje wiersze z stdin tak długo aż nie uda się poprawnie utworzyć nowej gry.
 * Aktualizuje numer aktualnego wiersza.
 * @param[out] game        – wskaźnik na zmienną do której zapisany zostanie wskaźnik
 *                           na strukturę przechowującą dane gry.
 * @param[out] mode        – wskaźnik na znak oznaczający tryb gry (B lub I),
 * @param[in,out] line     – wskaźnik na aktualny numer wiersza wejścia.
 * @return Kod @p NO_ERROR jeżeli wczytane parametry są poprawne,
 * @p ENCOUNTERED_EOF, jeżeli dane na wejściu się skończyły (EOF), @p MEMORY_ERROR,
 * jeżeli wystąpił błąd alokacji pamięci.
 */
static io_error_t create_game_struct(gamma_t **game, char *mode, unsigned long *line) {
    io_error_t error;
    uint32_t args[COMMAND_ARGUMENTS_UPPER_BOUND];
    do {
        (*line)++;
        error = read_next_command(mode, args, GAME_MODE_IDENTIFIERS);
        if (error == NO_ERROR) {
            if (!gamma_game_new_arguments_valid(args[0], args[1], args[2], args[3])) {
                error = INVALID_VALUE;
            } else {
                *game = gamma_new(args[0], args[1], args[2], args[3]);
                if (*game == NULL) {
                    error = MEMORY_ERROR;
                }
            }
        }
        if (error != NO_ERROR) {
            if (error == ENCOUNTERED_EOF) {
                return ENCOUNTERED_EOF;
            }
            if (error != LINE_IGNORED) {
                fprintf(stderr, "ERROR %lu\n", *line);
            }
        }
    } while (error != NO_ERROR);

    return NO_ERROR;
}

/** @brief Koordynuje przebieg gry gamma.
 * Wczytuje dane gry, tworzy nową grę i uruchamia rozgrywkę w trybie wsadowym
 * lub w trybie interaktywnym. Zwalnia pamięć po zakończeniu rozgrywki.
 * @return Zero, gdy wszystko przebiegło poprawnie,
 * a w przeciwnym przypadku kod zakończenia programu jest kodem błędu.
 * Kod 1 oznacza krytyczny błąd - na przykład błąd alokacji pamięci, lub błąd
 * wczytywania danych w trybie interaktywnym.
 */
int main() {
    char mode;
    gamma_t *game = NULL;
    unsigned long line = 0;

    io_error_t error = create_game_struct(&game, &mode, &line);
    if (error == MEMORY_ERROR) {
        return 1;
    } else if (error == ENCOUNTERED_EOF) {
        return 0;
    }

    error = NO_ERROR;
    if (mode == 'B') {
        run_batch_mode(game, &line);
    } else {
        error = run_interactive_mode(game);
    }

    gamma_delete(game);

    if (error != NO_ERROR) {
        return 1;
    }

    return 0;
}
