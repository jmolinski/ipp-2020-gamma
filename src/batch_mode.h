/** @file
 * Interfejs rozgrywki gry gamma w trybie wsadowym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#ifndef BATCH_MODE_H
#define BATCH_MODE_H

#include "gamma.h"

/** @brief Przeprowadza rozgrywkę w trybie wsadowym.
 * @param[in] g      – wskaźnik na strukturę danych gry.
 */
void run_batch_mode(gamma_t *g);

#endif /* BATCH_MODE_H */