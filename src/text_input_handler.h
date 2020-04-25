/** @file
 * Interfejs modułu obsługującego wejście tekstowe z konsoli.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#ifndef TEXT_INPUT_HANDLER_H
#define TEXT_INPUT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Enum opisujący różne typy błędów, które mogą wystąpić w trakcie wykonania
 */
typedef enum errors {
    ENCOUNTERED_EOF,
    INVALID_VALUE,
    NO_ERROR,
    LINE_IGNORED,
    MEMORY_ERROR
} error_t;

/** @brief Wczytuje parametry następnej komendy.
 * @param[out] command         – wskaźnik na znak oznaczający typ komendy,
 * @param[out] args            – wskaźnik na tablicę argumentów (co najmniej 4 pola),
 * @param[in] allowed_commands – ciąg dozwolonych identyfikatorów komend.
 * @return Kod @p NO_ERROR jeżeli wczytane parametry są poprawne,
 * @p ENCOUNTERED_EOF, jeżeli dane na wejściu się skończyły (EOF), @p INVALID_VALUE,
 * jeżeli wartości parametrów lub polecenie są niepoprawne, @p LINE_IGNORED,
 * jeżeli wiersz jest pusty lub zaczyna się znakiem #.
 *
 */
error_t read_next_command(char *command, uint32_t args[4], const char *allowed_commands);

#endif /* TEXT_INPUT_HANDLER_H */
