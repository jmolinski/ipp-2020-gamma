/** @file
 * Implementacja interfejsu silnika gry gamma.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 06.04.2020
 */

#include "gamma.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define ASCII_ZERO 48

/**
 * Struktura przechowująca stan pola.
 */
struct field {
    uint32_t player; /**< flag for low speed */
    bool empty;
    field_t *parent; // for find-union algorithm
    uint8_t rank;    // for find-union algorithm
};

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

field_t *fu_find(field_t *field) {
    // path halving method

    while (field->parent != field) {
        field->parent = field->parent->parent;
        field = field->parent;
    }

    return field;
}

bool fu_union(field_t *x, field_t *y) {
    // Metoda union by rank.

    field_t *x_root = fu_find(x);
    field_t *y_root = fu_find(y);

    if (x_root == y_root) {
        // x and y are already in the same set
        return false;
    }

    if (x_root->rank < y_root->rank) {
        field_t *tmp = x_root;
        x_root = y_root;
        y_root = tmp;
    }

    y_root->parent = x_root;
    if (x_root->rank == y_root->rank) {
        x_root->rank += 1;
    }

    return true;
}

/** @brief Alokuje planszę do gry Gamma.
 * Alokuje planszę o zadanych wymiarach składającą się z pustych pól.
 * @param[in] width       – szerokość planszy,
 * @param[in] height      – wysokoć planszy.
 * @return Wskaźnik na planszę lub @p NULL jesli nie udało się zaalokować pamięci.
 */
field_t **allocate_board(uint32_t width, uint32_t height) {
    field_t **board = malloc(height * sizeof(field_t *));
    if (board == NULL) {
        return NULL;
    }

    uint32_t allocated_rows = 0;
    for (; allocated_rows < height; allocated_rows++) {
        board[allocated_rows] = malloc((uint64_t)width * sizeof(struct field));
        if (board[allocated_rows] == NULL) {
            break;
        }
        for (uint32_t i = 0; i < width; i++) {
            board[allocated_rows][i].empty = true;
            board[allocated_rows][i].parent = &board[allocated_rows][i];
            board[allocated_rows][i].rank = 1;
            board[allocated_rows][i].player = 0;
        }
    }

    if (allocated_rows == height) {
        return board;
    }

    for (uint32_t row = 0; row < allocated_rows; row++) {
        free(board[row]);
    }
    free(board);

    return NULL;
}

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

uint8_t new_zero_cost_fields(gamma_t *g, int64_t x, int64_t y, uint32_t player) {
    uint8_t new_nearby_empty_fields = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if ((dx == 0 && dy == 0) || (dx != 0 && dy != 0)) {
                continue;
            }
            field_t *f = get_field(g, x + dx, y + dy);
            if (f != NULL && f->empty && !has_neighbor(g, x + dx, y + dy, player)) {
                new_nearby_empty_fields++;
            }
        }
    }
    return new_nearby_empty_fields;
}

void decrement_neighbors_border_empty_fields(gamma_t *g, int64_t x, int64_t y) {
    field_t *neighbors[4] = {get_field(g, x + 1, y), get_field(g, x - 1, y),
                             get_field(g, x, y + 1), get_field(g, x, y - 1)};
    for (int i = 0; i < 4; i++) {
        if (neighbors[i] == NULL || neighbors[i]->empty)
            continue;

        uint32_t neighbor = neighbors[i]->player;
        g->players[neighbor % g->players_num].zero_cost_move_fields--;

        for (int j = i; j < 4; j++)
            if (neighbors[j] != NULL && neighbors[j]->player == neighbor)
                neighbors[j] = NULL;
    }
}

bool gamma_move(gamma_t *g, uint32_t player, uint32_t x, uint32_t y) {
    if (g == NULL || player == 0 || player > g->players_num || x >= g->width ||
        y >= g->height) {
        return false;
    }
    if (!g->board[y][x].empty) {
        return false;
    }

    const uint32_t player_index = player % g->players_num;

    if (g->players[player_index].areas == g->max_areas) {
        if (!has_neighbor(g, x, y, player)) {
            return false;
        }
    }

    int new_border_empty_fields = new_zero_cost_fields(g, x, y, player);

    g->board[y][x].player = player;
    g->board[y][x].empty = false;
    g->occupied_fields++;
    g->players[player_index].areas++;
    g->players[player_index].occupied_fields++;
    g->players[player_index].areas -= union_neighbors(g, x, y);
    g->players[player_index].zero_cost_move_fields += new_border_empty_fields;

    decrement_neighbors_border_empty_fields(g, x, y);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Na nowo tworzy strukture find-union obszarów.
 * Na nowo tworzy strukture find-union obszarów i dla każdego gracza aktualizuje
 * liczbe posiadanych obszarów.
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry.
 * @return Wartość @p true, jeżeli po zakończeniu każdy z graczy ma nie więcej niż
 * @p g->max_areas obszarów, @p false w przeciwnym przypadku.
 */
bool reindex_areas(gamma_t *g) {
    for (uint32_t p = 0; p < g->players_num; p++) {
        g->players[p].areas = 0;
    }

    // Zresetuj pola z danymi FU w całej planszy. [ O(mn) ]
    for (int64_t row = 0; row < g->height; row++) {
        for (int64_t column = 0; column < g->width; column++) {
            if (g->board[row][column].empty) {
                continue;
            }
            g->board[row][column].parent = &g->board[row][column];
            g->board[row][column].rank = 1;
            uint32_t player_index = g->board[row][column].player % g->players_num;
            g->players[player_index].areas++;
        }
    }

    // Utwórz na nowo sety find-union. [ O(mn) ]
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

    // Sprawdź czy każdy gracz ma nie wiecej niz g->max_areas obszarów.
    for (uint32_t p = 0; p < g->players_num; p++) {
        if (g->players[p].areas > g->max_areas) {
            return false;
        }
    }

    return true;
}

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
    if (g->players[player_index].areas == g->max_areas) {
        if (!has_neighbor(g, x, y, player)) {
            return false;
        }
    }

    // Ta wartość musi zostać obliczona przed podmienieniem gracza.
    int new_border_empty_fields = new_zero_cost_fields(g, x, y, player);

    uint32_t previous_player = g->board[y][x].player;
    uint32_t previous_player_index = previous_player % g->players_num;
    g->board[y][x].player = player;

    bool valid_state = reindex_areas(g);
    if (!valid_state) {
        // Przekroczono limit obszarów dla któregoś z graczy.
        g->board[y][x].player = previous_player;
        reindex_areas(g);
        return false;
    }

    g->players[player_index].occupied_fields++;
    g->players[player_index].zero_cost_move_fields += new_border_empty_fields;
    g->players[player_index].golden_move_done = true;

    const int lost_border_empty_fields = new_zero_cost_fields(g, x, y, previous_player);
    g->players[previous_player_index].occupied_fields--;
    g->players[previous_player_index].zero_cost_move_fields -= lost_border_empty_fields;

    return true;
}

uint64_t gamma_busy_fields(gamma_t *g, uint32_t player) {
    if (g == NULL || player == 0 || player > g->players_num) {
        return 0;
    }

    const uint32_t player_index = player % g->players_num;
    return g->players[player_index].occupied_fields;
}

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
 * Bufor musi być odpowiedio długi aby pomieścić wszystkie cyfry.
 * @param[in,out] buffer - bufor do którego zapisany ma zostać wynik.
 * @param[in] n - liczba do skonwertowania.
 * @return Liczba zapisanych bajtów.
 */
static inline int uint_to_string(char *buffer, uint32_t n) {
    char digits[11]; // max_len = round(log10(2^32)) = 10
    uint8_t written = 0;

    do {
        // zapisuje cyfry w odwróconej kolejności
        digits[written++] = n % 10;
        n /= 10;
    } while (n != 0);

    for (int p = written - 1; p >= 0; p--) {
        *buffer++ = ASCII_ZERO + digits[p];
    }

    return written;
}

/**
 * @brief Zapisuje tekstową reprezentację pola do bufora.
 * Zapisuje tekstową reprezentację pola do bufora. Bufor musi mieć odpowiednio
 * dużo wolnego miejsca aby pomieścić wszystkie znaki.
 * @param[in,out] buffer - bufor do którego zapisany ma zostać wynik,
 * @param[in] field - pole.
 * @return Liczba zapisanych bajtów.
 */
uint8_t render_field(char *buffer, field_t *field) {
    uint8_t written_chars = 0;
    if (field->empty) {
        buffer[written_chars++] = '.';
    } else {
        uint32_t player = field->player;
        if (player < 10) {
            buffer[written_chars++] = ASCII_ZERO + player;
        } else {
            buffer[written_chars++] = '[';
            written_chars += uint_to_string(&buffer[written_chars], player);
            buffer[written_chars++] = ']';
        }
    }

    return written_chars;
}

char *gamma_board(gamma_t *g) {
    if (g == NULL) {
        return NULL;
    }

    const uint64_t total_fields = (g->width) * g->height;
    // + g->height aby mieć miejsce na '\n', +100 to dodatkowe miejsce na \0 i
    // numery graczy powyżej 9; jeśli zabraknie miejsca wywołany zostanie realloc.
    uint64_t allocated_space = (total_fields + g->height + 100) * sizeof(char);
    char *str = malloc(allocated_space);
    if (str == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    uint64_t pos = 0;
    uint64_t written_fields = 0;
    for (int64_t y = g->height - 1; y >= 0; y--, written_fields++) {
        for (uint32_t x = 0; x < g->width; x++) {
            const uint64_t left_buffer_space = allocated_space - pos;

            if (left_buffer_space < 50) {
                // Maks wymagane na jedno pole to ~15 bajtów.
                uint64_t extra_space = (total_fields - written_fields) + 50;
                allocated_space += extra_space;
                str = realloc(str, allocated_space);
                if (str == NULL) {
                    errno = ENOMEM;
                    return NULL;
                }
            }

            pos += render_field(&str[pos], &g->board[y][x]);
        }
        str[pos++] = '\n';
    }

    str[pos] = '\0';

    return str;
}
