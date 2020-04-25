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

extern const int ENCOUNTERED_EOF;
extern const int INVALID_VALUE;
extern const int NO_ERROR;
extern const int UNEXPECTED_NEWLINE;

/** @brief Wczytuje parametry nowej rozgrywki.
 * @param[out] mode      – wskaźnik na znak oznaczający typ rozgrywki, I lub B,
 * @param[out] width     – wskaźnik na szerokość planszy, liczba dodatnia,
 * @param[out] height    – wskaźnik na wysokość planszy, liczba dodatnia,
 * @param[out] players   – wskaźnik na liczbę graczy, liczba dodatnia,
 * @param[out] areas     – wskaźnik na maksymalną liczbę obszarów,
 *                      jakie może zająć jeden gracz, liczba dodatnia.
 * @return Wartość logiczna @p NO_ERROR jeżeli wczytane parametry są poprawne,
 * @p ENCOUNTERED_EOF, jeżeli dane na wejściu się skończyły (EOF),
 * lub @p INVALID_VALUE, jeżeli wartości parametrów lub polecenie są niepoprawne.
 */
int read_game_parameters(char *mode, uint32_t *width, uint32_t *height,
                          uint32_t *players, uint32_t *areas);

#endif /* TEXT_INPUT_HANDLER_H */
