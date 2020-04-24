/** @file
 * Plik zawiera główną pętlę koordynującą przebieg gry.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "gamma.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "text_input_handler.h"
#include "errno.h"
#include "interactive_mode.h"
#include "batch_mode.h"

int main() {
    char mode;
    uint32_t width, height, players, areas;
    bool parameters_correct = false;

    do {
        parameters_correct =
            read_game_parameters(&mode, &width, &height, &players, &areas);
    } while (!parameters_correct);

    gamma_t *game = gamma_new(width, height, players, areas);
    if (game == NULL) {
        return 1;
    }

    if (mode == 'B') {
        run_batch_mode(game);
    } else {
        run_interactive_mode(game);
    }

    gamma_delete(game);

    if(errno == ENOMEM) {
        return 1;
    }

    return 0;
}
