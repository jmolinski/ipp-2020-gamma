/** @file
 * Plik zawierający typ reprezentujący błąd.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 26.04.2020
 */

#ifndef GAMMA_ERRORS_H
#define GAMMA_ERRORS_H

/**
 * Enum opisujący różne typy błędów, które mogą wystąpić w trakcie wykonania
 */
typedef enum errors {
    ENCOUNTERED_EOF,
    INVALID_VALUE,
    NO_ERROR,
    LINE_IGNORED,
    MEMORY_ERROR,
    TERMINAL_ERROR,
} io_error_t;

#endif // GAMMA_ERRORS_H
