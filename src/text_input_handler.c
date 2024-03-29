/** @file
 * Implementacja modułu obsługującego wejście tekstowe z konsoli.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include "text_input_handler.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Ograniczenie górne długości znakowej reprezentacji liczby typu uint32_t
 * 13 > ceil(log10(UINT32_MAX)) = 10 */
#define UINT32_LENGTH_UPPER_BOUND 13

/** @brief Pomija znaki z stdin aż do znaku końca linii (włącznie).
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_CHARACTER, jeżeli przed znakiem nowej linii napotkano znaki inne niż
 * znaki białe.
 */
static inline io_error_t skip_until_next_line() {
    int ch;
    io_error_t errcode = NO_ERROR;

    do {
        ch = getchar();
        if (ch == EOF) {
            return ENCOUNTERED_EOF;
        }
        if (!isspace(ch)) {
            errcode = INVALID_VALUE;
        }
    } while (ch != '\n');

    return errcode;
}

/** @brief Pomija białe znaki z stdin aż do następnego niebiałego znaku lub EOF. */
static inline void skip_white_characters() {
    int ch;
    do {
        ch = getchar();
        if (ch == EOF) {
            return;
        }
        if (!isspace(ch) || ch == '\n') {
            ungetc(ch, stdin);
        }
    } while (isspace(ch) && ch != '\n');
}

/** @brief Wczytuje zera wiodące i do 11 cyfr liczby z stdin.
 * Bufor @p buffer musi mieć odpowiednio dużo miejsca na pomieszczenie wszystkich
 * znaków. Wynik zostaje automatycznie zakończony znakiem \0.
 * @param[out] buffer      – wskaźnik na bufor wyjściowy.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_VALUE, jeżeli napotkany zostanie niespodziewany znak.
 */
static io_error_t read_uint32_digits(char *buffer) {
    static const unsigned max_digits = 11;
    int ch;
    unsigned i = 0;
    bool first_char_zero = false;
    do {
        ch = getchar();
        if (ch == EOF) {
            return ENCOUNTERED_EOF;
        }
        if (i == 0 && ch == '0') {
            first_char_zero = true;
        } else if (!isdigit(ch)) {
            ungetc(ch, stdin);
            if (!isspace(ch)) {
                return INVALID_VALUE;
            }
        } else {
            buffer[i++] = (char)ch;
        }
        if (i > max_digits) {
            return INVALID_VALUE;
        }
    } while (isdigit(ch));

    if (i == 0) {
        if (!first_char_zero) {
            return INVALID_VALUE;
        }
        buffer[i++] = '0';
    }

    buffer[i] = '\0';
    return NO_ERROR;
}

/** @brief Wczytuje następny int bez znaku z stdin.
 * Pomija białe znaki przed liczbą oraz zera wiodące.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_VALUE, jeżeli napotkany zostanie niespodziewany znak.
 */
static inline io_error_t read_uint32(uint32_t *ptr) {
    char buffer[UINT32_LENGTH_UPPER_BOUND];
    skip_white_characters();
    int error = read_uint32_digits(buffer);
    if (error != NO_ERROR) {
        return error;
    }

    errno = 0;
    unsigned long value = strtoul(buffer, NULL, 10);
    if (errno == ERANGE || value > UINT32_MAX) {
        return INVALID_VALUE;
    }
    *ptr = (uint32_t)value;
    return NO_ERROR;
}

/** @brief Wczytuje znak identyfikujący komendę z stdin.
 * @param[out] command           – wskaźnik na komórkę, gdzie ma zostać zapisany
 *                                 wczytany znak,
 * @param[in] allowed_commands   – ciąg dozwolonych identyfikatorów komend.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli na stdin nie ma żadnego znaku, @p LINE_IGNORED, jeżeli linijka jest pusta,
 * @p LINE_IGNORED, jeżeli linijka zaczyna się od znaku #, lub @p INVALID_VALUE,
 * jeżeli wczytany znak nie jest poprawnym identyfikatorem komendy.
 */
static inline io_error_t read_command_char(char *command,
                                           const char *allowed_commands) {
    int read_command = getchar();
    switch (read_command) {
    case EOF:
        return ENCOUNTERED_EOF;
    case '\n':
        return LINE_IGNORED;
    case '#':
        skip_until_next_line();
        return LINE_IGNORED;
    default:
        if (strchr(allowed_commands, read_command) == NULL) {
            skip_until_next_line();
            return INVALID_VALUE;
        }
        *command = (char)read_command;
        return NO_ERROR;
    }
}

/** @brief Zwraca liczbę parametrów przyjmowanych przez komendę.
 * @param[in] command   – znak identyfikujący komendę.
 * @return liczba parametrów przyjmowanych przez komendę.
 */
static inline unsigned get_command_arguments_count(char command) {
    if (command == 'B' || command == 'I') {
        return 4;
    } else if (command == 'g' || command == 'm') {
        return 3;
    } else if (command == 'b' || command == 'f' || command == 'q') {
        return 1;
    }
    return 0;
}

/** @brief Wczytuje argumenty komendy.
 * Wczytuje argumenty komendy ze standardowego wejścia. W razie wystąpienia problemu
 * pomija wszystkie znaki do końca wiersza.
 * @param[in] command    – znak identyfikujący komendę,
 * @param[out] args      – wskaźnik na tablicę argumentów (co najmniej 4 pola).
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed wczytaniem wszystkich argumentów,
 * @p INVALID_VALUE, jeżeli napotkany zostanie niespodziewany znak.
 */
static io_error_t read_arguments(char command,
                                 uint32_t args[COMMAND_ARGUMENTS_UPPER_BOUND]) {
    unsigned arguments_count = get_command_arguments_count(command);
    if (arguments_count) {
        int ch = getchar();
        if (!isspace(ch) || ch == '\n') {
            ungetc(ch, stdin);
            skip_until_next_line();
            return INVALID_VALUE;
        }
    }

    io_error_t error;
    for (unsigned i = 0; i < arguments_count; i++) {
        if ((error = read_uint32(&args[i])) != NO_ERROR) {
            if (error == ENCOUNTERED_EOF) {
                return ENCOUNTERED_EOF;
            } else {
                skip_until_next_line();
                return error;
            }
        }
    }

    return NO_ERROR;
}

io_error_t text_input_read_next_command(char *command, uint32_t *args,
                                        const char *allowed_commands) {
    io_error_t error;
    if ((error = read_command_char(command, allowed_commands)) != NO_ERROR) {
        return error;
    }
    if (read_arguments(*command, args) != NO_ERROR) {
        return INVALID_VALUE;
    }
    if (skip_until_next_line() != NO_ERROR) {
        return INVALID_VALUE;
    }

    return NO_ERROR;
}