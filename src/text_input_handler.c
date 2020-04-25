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

const int NO_ERROR = 0;
const int ENCOUNTERED_EOF = 1;
const int INVALID_VALUE = 2;
const int LINE_IGNORED = 3;
const int MEMORY_ERROR = 4;

/** @brief Pomija znaki z stdin aż do znaku końca linii (włącznie).
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_CHARACTER, jeżeli przed znakiem nowej linii napotkano znaki inne niż
 * znaki białe.
 */
int skip_until_next_line() {
    int ch;
    int errcode = NO_ERROR;

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

/** @brief Pomija białe znaki z stdin aż do następnego niebiałego znaku lub EOF.
 */
void skip_white_characters() {
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

/** @brief Wczytuje do 11 cyfr z stdin.
 * Bufor @p buffer musi mieć odpowiednio dużo miejsca na pomieszczenie wszystkich
 * znaków. Wynik zostaje automatycznie zakończony znakiem \0.
 * @param[out] buffer      – wskaźnik na bufor wyjściowy.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_VALUE, jeżeli napotkany zostanie niespodziewany znak.
 */
int read_uint32_digits(char *buffer) {
    int ch;
    int i = 0;
    do {
        ch = getchar();
        if (ch == EOF) {
            return ENCOUNTERED_EOF;
        }
        if (!isdigit(ch)) {
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

    buffer[i] = '\0';
    if (i == 0) {
        return INVALID_VALUE;
    }
    return NO_ERROR;
}

/** @brief Wczytuje następny int bez znaku z stdin.
 * Pomija białe znaki przed liczbą.
 * @return Kod @p NO_ERROR jeżeli operacja przebiegła poprawnie, @p ENCOUNTERED_EOF
 * jeżeli dane wejściowe kończą się przed napotkaniem znaku nowej linii,
 * @p INVALID_VALUE, jeżeli napotkany zostanie niespodziewany znak.
 */
// TODO -0, leading zeros
int read_uint32(uint32_t *ptr) {
    char buffer[13]; // maksymalna długość uint32_t to 10
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
int read_command_char(char *command, const char *allowed_commands) {
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
int get_command_arguments_count(char command) {
    if (command == 'B' || command == 'I') {
        return 4;
    } else if (command == 'g' || command == 'm') {
        return 3;
    } else if (command == 'b' || command == 'f' || command == 'q') {
        return 1;
    }
    return 0;
}

int read_next_command(char *command, uint32_t args[3], const char* allowed_commands) {
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