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
 * @return Kod @p NO_ERROR jeżeli wszystko przebiegło poprawnie, @p ENCOUNTERED_EOF
 * jeżeli wejście zostało zamknięte przed poprawnym zakończeniem gry, @p MEMORY_ERROR,
 * jeżeli wystąpił błąd alokacji pamięci, @p TERMINAL_ERROR jeżeli wystąpił błąd podczas
 * zmieniania parametrów terminala.
 */
io_error_t run_interactive_mode(gamma_t *g);

#endif /* INTERACTIVE_MODE_H */
