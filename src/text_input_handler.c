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

const int NO_ERROR = 0;
const int ENCOUNTERED_EOF = 1;
const int INVALID_VALUE = 2;
const int UNEXPECTED_NEWLINE = 3;

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
int read_uint32(uint32_t *ptr) {
    char buffer[13]; // maksymalna długość uint32_t to 10
    skip_white_characters();
    int error = read_uint32_digits(buffer);
    if (error != NO_ERROR) {
        return error;
    }

    errno = 0;
    *ptr = strtoul(buffer, NULL, 10);
    if (errno == ERANGE) {
        return INVALID_VALUE;
    }
    return NO_ERROR;
}

int read_game_parameters(char *mode, uint32_t *width, uint32_t *height,
                         uint32_t *players, uint32_t *areas) {
    int read_mode = getchar();
    if (read_mode == EOF) {
        return ENCOUNTERED_EOF;
    }
    *mode = (char)read_mode;

    int error = NO_ERROR;
    if ((error = read_uint32(width)) != NO_ERROR ||
        (error = read_uint32(height)) != NO_ERROR ||
        (error = read_uint32(players)) != NO_ERROR ||
        (error = read_uint32(areas)) != NO_ERROR) {
        if (error == ENCOUNTERED_EOF) {
            return ENCOUNTERED_EOF;
        }
        if (error == UNEXPECTED_NEWLINE) {
            return INVALID_VALUE;
        }
    }

    if ((error = skip_until_next_line()) != NO_ERROR) {
        return error;
    }

    if ((*mode != 'B' && *mode != 'I') || *width == 0 || *height == 0 ||
        *players == 0 || *areas == 0) {
        return INVALID_VALUE;
    }

    return NO_ERROR;
}
