/** @file
 * Plik zawiera główną pętlę koordynującą przebieg gry.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "batch_mode.h"
#include "errno.h"
#include "gamma.h"
#include "interactive_mode.h"
#include "text_input_handler.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

error_t create_game_struct(gamma_t **game, char *mode, uint64_t *line) {
    error_t error;
    uint32_t args[4];
    do {
        (*line)++;
        error = read_next_command(mode, args, "BI");
        if (error == NO_ERROR) {
            if (!gamma_game_new_arguments_valid(args[0], args[1], args[2], args[3])) {
                error = INVALID_VALUE;
            }
        }
        if (error != NO_ERROR) {
            if (error != LINE_IGNORED) {
                fprintf(stderr, "ERROR %lu\n", *line);
            }
        }
    } while (error != NO_ERROR);

    (*game) = gamma_new(args[0], args[1], args[2], args[3]);
    if (game == NULL) {
        return MEMORY_ERROR;
    }
    return NO_ERROR;
}

int main() {
    // TODO przetestowanie jak dziala kiedy malloc sie gdzies wywali
    char mode;
    gamma_t *game;
    uint64_t line = 0;

    int error = create_game_struct(&game, &mode, &line);
    if (error == MEMORY_ERROR) {
        return 1;
    }

    if (mode == 'B') {
        run_batch_mode(game, &line);
    } else {
        run_interactive_mode(game);
    }

    gamma_delete(game);

    if (errno == ENOMEM) {
        return 1;
    }

    return 0;
}
