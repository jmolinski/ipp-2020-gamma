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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Pomija znaki z stdin aż do znaku końca linii (włącznie).
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_CHARACTER, jeżeli przed znakiem nowej linii napotkano znaki inne niż
 * znaki białe.
 */
static inline error_t skip_until_next_line() {
    int ch;
    error_t errcode = NO_ERROR;

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
static inline error_t read_uint32_digits(char *buffer) {
    int ch, i = 0;
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
        if (i > 11) {
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
static inline error_t read_uint32(uint32_t *ptr) {
    char buffer[13]; // Maksymalna długość uint32_t to 10 znaków.
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
static inline error_t read_command_char(char *command, const char *allowed_commands) {
    int read_command = getchar();
    if (read_command == EOF) {
        return ENCOUNTERED_EOF;
    }
    if (read_command == '\n') {
        return LINE_IGNORED;
    }
    if (read_command == '#') {
        skip_until_next_line();
        return LINE_IGNORED;
    }
    if (strchr(allowed_commands, read_command) == NULL) {
        skip_until_next_line();
        return INVALID_VALUE;
    }
    *command = (char)read_command;
    return NO_ERROR;
}

/** @brief Zwraca liczbę parametrów przyjmowanych przez komendę.
 * @param[in] command   – znak identyfikujący komendę.
 * @return liczba parametrów przyjmowanych przez komendę.
 */
static inline int get_command_arguments_count(char command) {
    if (command == 'B' || command == 'I') {
        return 4;
    } else if (command == 'g' || command == 'm') {
        return 3;
    } else if (command == 'b' || command == 'f' || command == 'q') {
        return 1;
    }
    return 0;
}

error_t read_next_command(char *command, uint32_t args[4],
                          const char *allowed_commands) {
    int error;

    if ((error = read_command_char(command, allowed_commands)) != NO_ERROR) {
        return error;
    }

    int arguments_count = get_command_arguments_count(*command);
    for (int i = 0; i < arguments_count; i++) {
        if ((error = read_uint32(&args[i])) != NO_ERROR) {
            if (error == ENCOUNTERED_EOF) {
                return ENCOUNTERED_EOF;
            } else {
                skip_until_next_line();
                return error;
            }
        }
    }

    if ((error = skip_until_next_line()) != NO_ERROR) {
        return error;
    }

    return NO_ERROR;
}