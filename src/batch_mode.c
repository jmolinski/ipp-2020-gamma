/** @file
 * Implementacja modułu rozgrywki gry gamma w trybie wsadowym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "gamma.h"
#include "text_input_handler.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/** Wszystkie identyfikatory komend dozwolonych w trybie wsadowym */
#define BATCH_COMMAND_IDENTIFIERS "mgbfqp"

/** @brief Wykonuje gamma_move lub gamma_golden_move.
 * Weryfikuje poprawność przekazanych argumentów i wykonuje zadaną komendę.
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] command     – znak oznaczający typ komendy (m lub g),
 * @param[in] player      – numer gracza,
 * @param[in] x           – numer kolumny,
 * @param[in] y           – numer wiersza.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p INVALID_VALUE,
 * jeżeli wartość któregoś z parametrów jest nieprawidłowa.
 */
static io_error_t run_move_or_golden_move(gamma_t *g, char command, uint32_t player,
                                          uint32_t x, uint32_t y) {
    bool move_performed;
    if (command == 'm') {
        move_performed = gamma_move(g, player, x, y);
    } else {
        move_performed = gamma_golden_move(g, player, x, y);
    }
    printf("%u\n", (unsigned)move_performed);

    return NO_ERROR;
}

/** @brief Wykonuje gamma_free_fields, gamma_busy_fields lub gamma_golden_possible.
 * Weryfikuje poprawność przekazanych argumentów i wykonuje zadaną komendę.
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] command     – znak oznaczający typ komendy, (b, f lub q),
 * @param[in] player      – numer identyfikujący gracza.
 */
static void run_busy_free_fields_or_golden_possible(gamma_t *g, char command,
                                                    uint32_t player) {
    uint64_t result;
    if (command == 'b') {
        result = gamma_busy_fields(g, player);
    } else if (command == 'f') {
        result = gamma_free_fields(g, player);
    } else /* command == 'q' */ {
        result = (uint64_t)gamma_golden_possible(g, player);
    }

    printf("%" PRIu64 "\n", result);
}

/** @brief Wykonuje zadane polecenie.
 * Poza argumentem @p command identyfikującym typ komendy przyjmuje 3 argumenty.
 * Jeżeli dana komenda przyjmuje mniej niż 3 argumenty, dodatkowe argumenty nie są
 * używane.
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] command     – znak oznaczający typ komendy, (m, g, f, b, q lub p),
 * @param[in] args        – argumenty komendy.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_VALUE, jeżeli któryś z argumentów jest nieprawidłowy lub operacja się
 * nie powiedzie, jednak błąd jest niekrytyczny.
 */
static io_error_t run_command(gamma_t *g, char command, uint32_t args[3]) {
    if (command == 'm' || command == 'g') {
        return run_move_or_golden_move(g, command, args[0], args[1], args[2]);
    } else if (command == 'b' || command == 'f' || command == 'q') {
        run_busy_free_fields_or_golden_possible(g, command, args[0]);
        return NO_ERROR;
    } else {
        char *rendered_board = gamma_board(g);
        if (rendered_board == NULL) {
            return INVALID_VALUE;
        } else {
            printf("%s", rendered_board);
            free(rendered_board);
        }
    }

    return NO_ERROR;
}

void run_batch_mode(gamma_t *g, unsigned long *line) {
    printf("OK %lu\n", *line); // Gra rozpoczęta prawidłowo.

    char command;
    uint32_t args[COMMAND_ARGUMENTS_UPPER_BOUND];
    io_error_t error;

    do {
        (*line)++;
        error = read_next_command(&command, args, BATCH_COMMAND_IDENTIFIERS);
        if (error == NO_ERROR) {
            error = run_command(g, command, args);
            if (error != NO_ERROR) {
                fprintf(stderr, "ERROR %lu\n", *line);
            }
        } else if (error == INVALID_VALUE) {
            fprintf(stderr, "ERROR %lu\n", *line);
        }
    } while (error != ENCOUNTERED_EOF);
}
