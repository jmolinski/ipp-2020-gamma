#include "gamma.h"
#include "board.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define ASCII_ZERO 48

/**
 * Struktura przechowująca stan gracza.
 */
struct player {
    uint64_t occupied_fields;
    uint64_t zero_cost_move_fields;
    uint32_t areas;
    bool golden_move_done;
};

/**
 * Struktura przechowująca stan gry.
 */
struct gamma {
    uint32_t max_areas;
    uint32_t players_num;
    uint32_t height;
    uint32_t width;

    uint64_t occupied_fields;

    player_t *players;
    field_t **board;
};

/** @brief Tworzy strukturę przechowującą stan gry.
 * Alokuje pamięć na nową strukturę przechowującą stan gry.
 * Inicjuje tę strukturę tak, aby reprezentowała początkowy stan gry.
 * @param[in] width   – szerokość planszy, liczba dodatnia,
 * @param[in] height  – wysokość planszy, liczba dodatnia,
 * @param[in] players – liczba graczy, liczba dodatnia,
 * @param[in] areas   – maksymalna liczba obszarów,
 *                      jakie może zająć jeden gracz.
 * @return Wskaźnik na utworzoną strukturę lub NULL, gdy nie udało się
 * zaalokować pamięci lub któryś z parametrów jest niepoprawny.
 */
gamma_t *gamma_new(uint32_t width, uint32_t height, uint32_t players, uint32_t areas) {
    if (width == 0 || height == 0 || players == 0 || areas == 0) {
        return NULL;
    }

    gamma_t *game = malloc(sizeof(gamma_t));
    if (game == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    game->width = width;
    game->height = height;
    game->max_areas = areas;
    game->players_num = players;

    game->occupied_fields = 0;

    game->players = calloc(players, sizeof(player_t));
    if (game->players != NULL) {
        game->board = allocate_board(width, height);
        if (game->board != NULL) {
            return game;
        }

        free(game->players);
    }

    free(game);
    errno = ENOMEM;
    return NULL;
}

/** @brief Usuwa strukturę przechowującą stan gry.
 * Usuwa z pamięci strukturę wskazywaną przez @p g.
 * Nic nie robi, jeśli wskaźnik ten ma wartość NULL.
 * @param[in] g       – wskaźnik na usuwaną strukturę.
 */
void gamma_delete(gamma_t *g) {
    if (g == NULL) {
        return;
    }

    for (uint32_t row = 0; row < g->height; row++) {
        free(g->board[row]);
    }
    free(g->board);
    free(g->players);
    free(g);
}

/** @brief Sprawdza czy pole nalezy do planszy.
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza.
 * @return Wartość @p true, jeśli pole nalezy do planszy, a @p false
 * w przeciwnym przypadku.
 */
static inline bool is_within_board(gamma_t *g, int64_t x, int64_t y) {
    return x >= 0 && y >= 0 && g->width > x && g->height > y;
}

static inline bool belongs_to_player(gamma_t *g, int64_t x, int64_t y,
                                     uint32_t player) {
    if (!is_within_board(g, x, y) || g->board[y][x].empty) {
        return false;
    }
    return g->board[y][x].player == player;
}

static inline bool has_neighbor(gamma_t *g, int64_t x, int64_t y, uint32_t player) {
    return belongs_to_player(g, x + 1, y, player) ||
           belongs_to_player(g, x - 1, y, player) ||
           belongs_to_player(g, x, y + 1, player) ||
           belongs_to_player(g, x, y - 1, player);
}

static inline field_t *get_field(gamma_t *g, int64_t x, int64_t y) {
    if (!is_within_board(g, x, y)) {
        return NULL;
    }
    return &g->board[y][x];
}

uint8_t union_neighbors(gamma_t *g, uint32_t column, uint32_t row) {
    field_t **board = g->board;
    field_t *this_field = &board[row][column];
    const uint32_t player = this_field->player;

    uint8_t merged_areas = 0;
    // need to check this for all field neighbors
    if (belongs_to_player(g, column + 1, row, player))
        merged_areas += fu_union(this_field, &board[row][column + 1]);
    if (belongs_to_player(g, column - 1, row, player))
        merged_areas += fu_union(this_field, &board[row][column - 1]);
    if (belongs_to_player(g, column, row + 1, player))
        merged_areas += fu_union(this_field, &board[row + 1][column]);
    if (belongs_to_player(g, column, row - 1, player))
        merged_areas += fu_union(this_field, &board[row - 1][column]);

    return merged_areas;
}

uint8_t new_empty_fields(gamma_t *g, int64_t x, int64_t y, uint32_t player) {
    uint8_t new_nearby_empty_fields = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if ((dx == 0 || dy == 0) && !(dx == 0 && dy == 0)) {
                field_t *f = get_field(g, x + dx, y + dy);
                if (f != NULL && f->empty) {
                    new_nearby_empty_fields += !has_neighbor(g, x + dx, y + dy, player);
                }
            }
        }
    }
    return new_nearby_empty_fields;
}

/** @brief Wykonuje ruch.
 * Ustawia pionek gracza @p player na polu (@p x, @p y).
 * @param[in,out] g   – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza, liczba dodatnia niewiększa od wartości
 *                      @p players z funkcji @ref gamma_new,
 * @param[in] x       – numer kolumny, liczba nieujemna mniejsza od wartości
 *                      @p width z funkcji @ref gamma_new,
 * @param[in] y       – numer wiersza, liczba nieujemna mniejsza od wartości
 *                      @p height z funkcji @ref gamma_new.
 * @return Wartość @p true, jeśli ruch został wykonany, a @p false,
 * gdy ruch jest nielegalny lub któryś z parametrów jest niepoprawny.
 */
bool gamma_move(gamma_t *g, uint32_t player, uint32_t x, uint32_t y) {
    if (g == NULL || player == 0 || player > g->players_num || x >= g->width ||
        y >= g->height) {
        return false;
    }
    if (!g->board[y][x].empty) {
        return false;
    }

    const uint32_t player_index = player % g->players_num;

    int64_t xs = x;
    int64_t ys = y;

    field_t *n = get_field(g, xs + 1, ys);
    field_t *s = get_field(g, xs - 1, ys);
    field_t *e = get_field(g, xs, ys + 1);
    field_t *w = get_field(g, xs, ys - 1);

    const bool is_zero_cost_move = has_neighbor(g, xs, ys, player);
    if (g->players[player_index].areas == g->max_areas) {
        if (!is_zero_cost_move) {
            return false;
        }
    }

    int new_nearby_empty_fields = new_empty_fields(g, xs, ys, player);

    g->board[y][x].player = player;
    g->board[y][x].empty = false;
    g->occupied_fields++;
    g->players[player_index].areas++;
    g->players[player_index].occupied_fields++;
    g->players[player_index].zero_cost_move_fields += new_nearby_empty_fields;

    g->players[player_index].areas -= union_neighbors(g, xs, ys);

    field_t *neighbors[4] = {n, s, e, w};
    for (int i = 0; i < 4; i++) {
        if (neighbors[i] == NULL || neighbors[i]->empty)
            continue;

        uint32_t neighbor = neighbors[i]->player;
        g->players[neighbor % g->players_num].zero_cost_move_fields--;

        for (int j = i; j < 4; j++)
            if (neighbors[j] != NULL && neighbors[j]->player == neighbor)
                neighbors[j] = NULL;
    }

    return true;
}

bool reindex_areas(gamma_t *g) {
    for (uint32_t p = 0; p < g->players_num; p++) {
        g->players[p].areas = 0;
    }

    // Reset fields fu fields.
    for (int64_t row = 0; row < g->height; row++) {
        for (int64_t column = 0; column < g->width; column++) {
            if (g->board[row][column].empty) {
                continue;
            }
            g->board[row][column].parent = &g->board[row][column];
            g->board[row][column].size = 1;
            uint32_t player_index = g->board[row][column].player % g->players_num;
            g->players[player_index].areas++;
        }
    }

    // Recreate find-union sets.
    for (int64_t row = 0; row < g->height; row++) {
        for (int64_t column = 0; column < g->width; column++) {
            if (g->board[row][column].empty) {
                continue;
            }
            uint32_t player_index = g->board[row][column].player % g->players_num;
            uint8_t merged_areas = union_neighbors(g, column, row);
            g->players[player_index].areas -= merged_areas;
        }
    }

    // Assert that all players have no more areas than g->max_areas.
    for (uint32_t p = 0; p < g->players_num; p++) {
        if (g->players[p].areas > g->max_areas) {
            return false;
        }
    }

    return true;
}

/** @brief Wykonuje złoty ruch.
 * Ustawia pionek gracza @p player na polu (@p x, @p y) zajętym przez innego
 * gracza, usuwając pionek innego gracza.
 * @param[in,out] g   – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza, liczba dodatnia niewiększa od wartości
 *                      @p players z funkcji @ref gamma_new,
 * @param[in] x       – numer kolumny, liczba nieujemna mniejsza od wartości
 *                      @p width z funkcji @ref gamma_new,
 * @param[in] y       – numer wiersza, liczba nieujemna mniejsza od wartości
 *                      @p height z funkcji @ref gamma_new.
 * @return Wartość @p true, jeśli ruch został wykonany, a @p false,
 * gdy gracz wykorzystał już swój złoty ruch, ruch jest nielegalny
 * lub któryś z parametrów jest niepoprawny.
 */
bool gamma_golden_move(gamma_t *g, uint32_t player, uint32_t x, uint32_t y) {
    if (g == NULL || player == 0 || player > g->players_num || x >= g->width ||
        y >= g->height) {
        return false;
    }
    if (g->board[y][x].empty || g->board[y][x].player == player) {
        return false;
    }

    const uint32_t player_index = player % g->players_num;
    if (g->players[player_index].golden_move_done) {
        return false;
    }

    const bool is_zero_cost_move = has_neighbor(g, x, y, player);
    if (g->players[player_index].areas == g->max_areas) {
        if (!is_zero_cost_move) {
            return false;
        }
    }

    int new_nearby_empty_fields = new_empty_fields(g, x, y, player);

    uint32_t previous_player = g->board[y][x].player;
    uint32_t previous_player_index = previous_player % g->players_num;
    g->board[y][x].player = player;

    bool valid_state = reindex_areas(g);
    if (!valid_state) {
        // exceeded areas limit
        g->board[y][x].player = previous_player;
        reindex_areas(g);
        return false;
    }

    g->players[player_index].occupied_fields++;
    g->players[previous_player_index].occupied_fields--;
    g->players[player_index].zero_cost_move_fields += new_nearby_empty_fields;
    g->players[previous_player_index].zero_cost_move_fields -=
        new_empty_fields(g, x, y, previous_player);

    g->players[player_index].golden_move_done = true;

    return true;
}

/** @brief Podaje liczbę pól zajętych przez gracza.
 * Podaje liczbę pól zajętych przez gracza @p player.
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza, liczba dodatnia niewiększa od wartości
 *                      @p players z funkcji @ref gamma_new.
 * @return Liczba pól zajętych przez gracza lub zero,
 * jeśli któryś z parametrów jest niepoprawny.
 */
uint64_t gamma_busy_fields(gamma_t *g, uint32_t player) {
    if (g == NULL || player == 0 || player > g->players_num) {
        return 0;
    }

    const uint32_t player_index = player % g->players_num;
    return g->players[player_index].occupied_fields;
}

/** @brief Podaje liczbę pól, jakie jeszcze gracz może zająć.
 * Podaje liczbę wolnych pól, na których w danym stanie gry gracz @p player może
 * postawić swój pionek w następnym ruchu.
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza, liczba dodatnia niewiększa od wartości
 *                      @p players z funkcji @ref gamma_new.
 * @return Liczba pól, jakie jeszcze może zająć gracz lub zero,
 * jeśli któryś z parametrów jest niepoprawny.
 */
uint64_t gamma_free_fields(gamma_t *g, uint32_t player) {
    if (g == NULL || player == 0 || player > g->players_num) {
        return 0;
    }

    const uint32_t player_index = player % g->players_num;

    if (g->players[player_index].areas < g->max_areas) {
        uint64_t total_fields = g->width * g->height;
        return total_fields - g->occupied_fields;
    }

    return g->players[player_index].zero_cost_move_fields;
}

/** @brief Sprawdza, czy gracz może wykonać złoty ruch.
 * Sprawdza, czy gracz @p player jeszcze nie wykonał w tej rozgrywce złotego
 * ruchu i jest przynajmniej jedno pole zajęte przez innego gracza.
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza, liczba dodatnia niewiększa od wartości
 *                      @p players z funkcji @ref gamma_new.
 * @return Wartość @p true, jeśli gracz jeszcze nie wykonał w tej rozgrywce
 * złotego ruchu i jest przynajmniej jedno pole zajęte przez innego gracza,
 * a @p false w przeciwnym przypadku.
 */
bool gamma_golden_possible(gamma_t *g, uint32_t player) {
    if (g == NULL || player == 0 || player > g->players_num) {
        return false;
    }

    const uint32_t player_index = player % g->players_num;

    if (g->players[player_index].golden_move_done) {
        return false;
    }

    for (uint32_t p = 0; p < g->players_num; p++) {
        if (g->players[p].occupied_fields > 0 && p != player_index) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Zmienia liczbę w napis.
 * Zapisuje cyfry zadanej liczby nieujemnej jako znaki w buforze.
 * Bufor musi być odpowiedio dlugi aby pomieścić wszystkie cyfry.
 * @param[in,out] buffer - bufor do którego zapisany ma zostać wynik.
 * @param[in] n - liczba do skonwertowania.
 * @return Liczba zapisanych bajtów.
 */
static inline int uint_to_string(char *buffer, uint32_t n) {
    char digits[11]; // max_len = round(log10(2^32)) = 10
    uint8_t written = 0;

    do {
        // this writes the digits in reversed order
        digits[written++] = n % 10;
        n /= 10;
    } while (n != 0);

    for (int p = written - 1; p >= 0; p--) {
        *buffer++ = ASCII_ZERO + digits[p];
    }

    return written;
}

/** @brief Daje napis opisujący stan planszy.
 * Alokuje w pamięci bufor, w którym umieszcza napis zawierający tekstowy
 * opis aktualnego stanu planszy. Przykład znajduje się w pliku gamma_test.c.
 * Funkcja wywołująca musi zwolnić ten bufor.
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry.
 * @return Wskaźnik na zaalokowany bufor zawierający napis opisujący stan
 * planszy lub NULL, jeśli nie udało się zaalokować pamięci.
 */
char *gamma_board(gamma_t *g) {
    if (g == NULL) {
        return NULL;
    }

    const uint64_t total_fields = (g->width) * g->height;
    // + g->height to account for newlines, +100 is just some extra space for \0
    // and player numbers longer than 1 char (will be reallocated if needed)
    uint64_t allocated_space = (total_fields + g->height + 100) * sizeof(char);
    char *str = malloc(allocated_space); // +1 for \0
    if (str == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    uint64_t pos = 0;
    uint64_t written_fields = 0;
    for(int64_t y = g->height - 1; y >= 0; y--, written_fields++) {
        for(uint32_t x = 0; x < g->width; x++) {
            const uint64_t left_buffer_space = allocated_space - pos;
            if (left_buffer_space < 50) { // max needed for 1 field is 33 bytes
                allocated_space = allocated_space + (total_fields - written_fields) + 50;
                str = realloc(str, allocated_space);
                if (str == NULL) {
                    errno = ENOMEM;
                    return NULL;
                }
            }

            if (x == 0 && y != g->height - 1) {
                str[pos++] = '\n';
            }

            if (g->board[y][x].empty) {
                str[pos++] = '.';
            } else {
                uint32_t player = g->board[y][x].player;
                if (player < 10) {
                    str[pos++] = ASCII_ZERO + player;
                } else {
                    str[pos++] = '[';
                    pos += uint_to_string(str, player);
                    str[pos++] = ']';
                }
            }
        }
    }

    str[pos++] = '\n';
    str[pos] = '\0';

    return str;
}
