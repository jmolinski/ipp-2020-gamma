/** @file
 * Interfejs modułu obsługującego wejście tekstowe z konsoli.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 24.04.2020
 */

#include <stdbool.h>

/** @brief Wczytuje parametry nowej rozgrywki.
 * @param[out] mode      – wskaźnik na znak oznaczający typ rozgrywki, I lub B,
 * @param[out] width     – wskaźnik na szerokość planszy, liczba dodatnia,
 * @param[out] height    – wskaźnik na wysokość planszy, liczba dodatnia,
 * @param[out] players   – wskaźnik na liczbę graczy, liczba dodatnia,
 * @param[out] areas     – wskaźnik na maksymalną liczbę obszarów,
 *                      jakie może zająć jeden gracz, liczba dodatnia.
 * @return Wartość logiczna @p true jeżeli wczytane parametry są poprawne,
 * @p false, jeżeli nie udało się wczytać, lub parametry są niepoprawne.
 */
bool read_game_parameters(char *mode, uint32_t *width, uint32_t *height,
                          uint32_t *players, uint32_t *areas);
