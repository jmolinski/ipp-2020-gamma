/** @file
 * Interfejs modułu rozgrywki gry gamma w trybie interaktywnym.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#ifndef INTERACTIVE_MODE_H
#define INTERACTIVE_MODE_H

#include "gamma.h"

/** @brief Przeprowadza rozgrywkę w trybie interaktywnym.
 * @param[in] g      – wskaźnik na strukturę danych gry.
 */
void run_interactive_mode(gamma_t *g);

#endif /* INTERACTIVE_MODE_H */