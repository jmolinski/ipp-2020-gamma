/** @file
 * Implementacja interfejsu silnika gry gamma.
 *
 * @author Jakub Moliński <jm419502@students.mimuw.edu.pl>
 * @copyright Uniwersytet Warszawski
 * @date 06.04.2020
 */

#include "gamma.h"
#include "errors.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Struktura przechowująca stan pola.
 */
typedef struct field {
    bool empty;      /**< Informacja czy dane pole jest puste. */
    uint8_t rank;    /**< Ranga obszaru, którego rodzicem jest to pole (find-union). */
    uint32_t player; /**< Numer gracza zajmującego pole. */
    struct field *parent; /**< Rodzic pola w danym obszarze (find-union). */
} field_t;

/**
 * Struktura przechowująca stan gracza.
 */
typedef struct player {
    bool golden_move_done; /**< Informacja czy gracz wykonał już złoty ruch. */
    uint32_t areas; /**< Liczba rozłącznych obszarów zajmowanych przez gracza. */
    uint64_t occupied_fields;     /**< Liczba pól zajmowanych przez gracza. */
    uint64_t border_empty_fields; /**< Liczba pól, na których gracz może postawić
                                   * pionek bez zwiększania liczby rozłącznych
                                   * obszarów. */
} player_t;

/**
 * Struktura przechowująca stan gry.
 */
struct gamma {
    uint32_t max_areas;       /**< Maksymalna liczba rozłącznych obszarów zajmowanych
                               * przez gracza. */
    uint32_t players_num;     /**< Liczba graczy. */
    uint32_t height;          /**< Liczba rzędów planszy. */
    uint32_t width;           /**< Liczba kolumn planszy. */
    uint64_t occupied_fields; /**< Łączna liczba zajętych pól na planszy. */

    player_t *players; /**< Tablica danych graczy. */
    field_t **board;   /**< Tablica 2D przedstawiająca planszę. */
};

/** @brief Operacja find (find-union) na planszy gry.
 * Zwraca najstarszego rodzica (lidera) należacego do danego obszaru na planszy.
 * (Operacja na strukturze danych find-union).
 * Funkcja stosuje metodę path-halving skracania ścieżki do najstarszego rodzica.
 * Złożoność O(a(n)) gdzie a to odwrotna funcja Ackermanna [efektywnie O(1)].
 * @param[in,out] field    – wskaźnik na dowolne pole należące do planszy.
 * @return Wskaźnik na jednoznacznie wyznaczonego przedstawiciela danego obszaru
 * (find-union).
 */
static inline field_t *fu_find(field_t *field) {
    while (field->parent != field) {
        field->parent = field->parent->parent;
        field = field->parent;
    }

    return field;
}

/** @brief Operacja union (find-union) na obszarach z planszy gry.
 * Łączy dwa rozłączne obszary na planszy. (Operacja na strukturze danych find-union).
 * Funkcja stosuje metodę union by rank do wyznaczania nowego lidera po połączeniu
 * dwóch obszarów.
 * Złożoność O(a(n)) gdzie a to odwrotna funcja Ackermanna [efektywnie O(1)].
 * @param[in,out] x    – wskaźnik na dowolne pole należące do planszy,
 * @param[in,out] y    – wskaźnik na dowolne pole należące do planszy.
 * @return Wartość logiczna @p false jeżeli pola należały już do tego samego obszaru,
 * @p true jeżeli obszary zostały połączone.
 */
static inline bool fu_union(field_t *x, field_t *y) {
    field_t *x_root = fu_find(x);
    field_t *y_root = fu_find(y);

    if (x_root == y_root) {
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
 * Złożoność O(height*width).
 * @param[in] width       – szerokość planszy,
 * @param[in] height      – wysokoć planszy.
 * @return Wskaźnik na planszę lub @p NULL jesli nie udało się zaalokować pamięci.
 */
static field_t **allocate_board(uint32_t width, uint32_t height) {
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
    if (!gamma_game_new_arguments_valid(width, height, players, areas)) {
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

/** @brief Sprawdza, czy pole należy do planszy.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza.
 * @return Wartość @p true, jeśli pole nalezy do planszy, a @p false
 * w przeciwnym przypadku.
 */
static inline bool is_within_board(const gamma_t *g, int64_t x, int64_t y) {
    return x >= 0 && y >= 0 && g->width > x && g->height > y;
}

/** @brief Sprawdza, czy pole należy do zadanego gracza.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza,
 * @param[in] player  – numer gracza.
 * @return Wartość @p true, jeśli pole nalezy do gracza, a @p false
 * w przeciwnym przypadku lub jeśli pole nie należy do planszy.
 */
static inline bool belongs_to_player(const gamma_t *g, int64_t x, int64_t y,
                                     uint32_t player) {
    return is_within_board(g, x, y) && !g->board[y][x].empty &&
           g->board[y][x].player == player;
}

/** @brief Sprawdza, czy zadane pole sąsiaduje z polem zadanego gracza.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza,
 * @param[in] player  – numer gracza.
 * @return Wartość @p true, jeśli pole sąsiaduje z dowolnym polem zadanego gracza,
 * a @p false w przeciwnym przypadku.
 */
static inline bool has_neighbor(const gamma_t *g, int64_t x, int64_t y,
                                uint32_t player) {
    return belongs_to_player(g, x + 1, y, player) ||
           belongs_to_player(g, x - 1, y, player) ||
           belongs_to_player(g, x, y + 1, player) ||
           belongs_to_player(g, x, y - 1, player);
}

/** @brief Zwraca wskaźnik na pole o zadanych koordynatach.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza.
 * @return Wartość @p true, jeśli pole sąsiaduje z dowolnym polem zadanego gracza,
 * a @p false w przeciwnym przypadku.
 */
static inline field_t *get_field(const gamma_t *g, int64_t x, int64_t y) {
    return is_within_board(g, x, y) ? &g->board[y][x] : NULL;
}

/** @brief Łączy (union z find-union) pole z sąsiednimi obszarami tego samego gracza.
 * Wykonuje operację union na danym polu i na wszystkich sąsiadujacych z nim polami
 * należącymi do tego samego gracza co zadane pole.
 * Złożoność O(1).
 * @param[in,out] g     – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] column    – numer kolumny,
 * @param[in] row       – numer wiersza.
 * @return Liczbę pomyślnie przeprowadzonych operacji union (0, 1, 2, 3 lub 4).
 */
static inline unsigned union_neighbors(gamma_t *g, uint32_t column, uint32_t row) {
    field_t **board = g->board;
    field_t *this_field = &board[row][column];
    const uint32_t player = this_field->player;

    unsigned merged_areas = 0;

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

/** @brief Wyznacza ile nowych pustych pól sąsiaduje z danym polem.
 * Wyznacza liczbę pustych pól sąsiadujących z zadanym polem takich, że nie
 * sąsiadują one z żadnym polem zadanego gracza.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza,
 * @param[in] player  – numer gracza.
 * @return Liczbę pól spełniających zadany warunek (0, 1, 2, 3 lub 4).
 */
static inline unsigned new_border_empty_fields(const gamma_t *g, int64_t x, int64_t y,
                                               uint32_t player) {
    unsigned new_nearby_empty_fields = 0;
    for (int64_t dx = -1; dx <= 1; dx++) {
        for (int64_t dy = -1; dy <= 1; dy++) {
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

/** @brief Dekrementuje liczbę pustych pól graniczacych z obszarami każdego z sąsiadów.
 * Dla każdego gracza, który posiada pole graniczące z polem o zadanych koordynatach
 * dekrementuje liczbę pustych miejsc graniczących z obszarami tego gracza.
 * Złożoność O(1).
 * @param[in,out] g    – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] x        – numer kolumny,
 * @param[in] y        – numer wiersza.
 */
static inline void decrement_neighbors_border_empty_fields(gamma_t *g, int64_t x,
                                                           int64_t y) {
    const unsigned neighbors_count = 4;
    field_t *neighbors[] = {get_field(g, x + 1, y), get_field(g, x - 1, y),
                            get_field(g, x, y + 1), get_field(g, x, y - 1)};
    for (unsigned i = 0; i < neighbors_count; i++) {
        if (neighbors[i] == NULL || neighbors[i]->empty) {
            continue;
        }

        uint32_t neighbor = neighbors[i]->player;
        g->players[neighbor % g->players_num].border_empty_fields--;

        for (unsigned j = i; j < neighbors_count; j++) {
            if (neighbors[j] != NULL && neighbors[j]->player == neighbor) {
                neighbors[j] = NULL;
            }
        }
    }
}

/** @brief Sprawdza, czy zajęcie pola przez gracza sprawi, że przekroczy on limit
 * obszarów.
 * Złożoność O(1).
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza.
 * @return Wartość @p true jeżeli przekroczony zostałby limit obszarów, @p false
 * w przeciwnym przypadku.
 */
static inline bool would_exceed_areas_limit(const gamma_t *g, uint32_t player,
                                            uint32_t x, uint32_t y) {
    return g->players[player % g->players_num].areas == g->max_areas &&
           !has_neighbor(g, x, y, player);
}

bool gamma_move(gamma_t *g, uint32_t player, uint32_t x, uint32_t y) {
    if (!gamma_game_move_arguments_valid(g, player, x, y)) {
        return false;
    }

    const uint32_t player_index = player % g->players_num;
    unsigned border_empty_fields_to_add = new_border_empty_fields(g, x, y, player);

    g->board[y][x].player = player;
    g->board[y][x].empty = false;
    g->occupied_fields++;
    g->players[player_index].areas++;
    g->players[player_index].occupied_fields++;
    g->players[player_index].areas -= union_neighbors(g, x, y);
    g->players[player_index].border_empty_fields += border_empty_fields_to_add;

    decrement_neighbors_border_empty_fields(g, x, y);

    return true;
}

/**
 * @brief Ustawia metadane struktury danych find-union na wartości wyjściowe.
 * Ustawia wartosci field.parent oraz field.rank na wartości wyjściowe oraz resetuje
 * liczbę obszarów gracza.
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry.
 */
static inline void reset_find_union_metadata(gamma_t *g) {
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
}

/**
 * @brief Na nowo tworzy strukture find-union obszarów.
 * Na nowo tworzy strukture find-union obszarów i dla każdego gracza aktualizuje
 * liczbe posiadanych obszarów.
 * Złożoność O(height*width) + O(players_num)
 * @param[in,out] g       – wskaźnik na strukturę przechowującą stan gry.
 * @return Wartość @p true, jeżeli po zakończeniu każdy z graczy ma nie więcej niż
 * @p g->max_areas obszarów, @p false w przeciwnym przypadku.
 */
static bool reindex_areas(gamma_t *g) {
    for (uint32_t p = 0; p < g->players_num; p++) {
        g->players[p].areas = 0;
    }

    reset_find_union_metadata(g);

    // Utwórz na nowo sety find-union.
    for (int64_t row = 0; row < g->height; row++) {
        for (int64_t column = 0; column < g->width; column++) {
            if (g->board[row][column].empty) {
                continue;
            }
            uint32_t player_index = g->board[row][column].player % g->players_num;
            unsigned merged_areas = union_neighbors(g, column, row);
            g->players[player_index].areas -= merged_areas;
        }
    }

    for (uint32_t p = 0; p < g->players_num; p++) {
        if (g->players[p].areas > g->max_areas) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Sprawdza, czy możliwe jest podjęcie próby wykonania złotego ruchu.
 * Złożoność O(1)
 * @param[in] g       – wskaźnik na strukturę przechowującą stan gry,
 * @param[in] player  – numer gracza,
 * @param[in] x       – numer kolumny,
 * @param[in] y       – numer wiersza.
 * @return Wartość @p true, jeżeli argumenty są nieprawidłowe lub jeśli wykonanie
 * złotego ruchu na pewno nie jest możliwe, @p false jeśli można podjąć próbę
 * wykonania ruchu.
 */
static inline bool is_golden_move_impossible(const gamma_t *g, uint32_t player,
                                             uint32_t x, uint32_t y) {
    return (g == NULL || player == 0 || player > g->players_num || x >= g->width ||
            y >= g->height || g->board[y][x].empty || g->board[y][x].player == player ||
            g->players[player % g->players_num].golden_move_done ||
            would_exceed_areas_limit(g, player, x, y));
}

bool gamma_golden_move(gamma_t *g, uint32_t player, uint32_t x, uint32_t y) {
    if (is_golden_move_impossible(g, player, x, y)) {
        return false;
    }

    unsigned border_empty_fields_to_add = new_border_empty_fields(g, x, y, player);

    uint32_t previous_player = g->board[y][x].player;
    uint32_t previous_player_index = previous_player % g->players_num;

    g->board[y][x].player = player;
    bool areas_limit_exceeded = reindex_areas(g);
    if (!areas_limit_exceeded) {
        g->board[y][x].player = previous_player;
        reindex_areas(g);
        return false;
    }

    const uint32_t player_index = player % g->players_num;
    g->players[player_index].occupied_fields++;
    g->players[player_index].border_empty_fields += border_empty_fields_to_add;
    g->players[player_index].golden_move_done = true;

    unsigned lost_border_empty_fields =
        new_border_empty_fields(g, x, y, previous_player);
    g->players[previous_player_index].occupied_fields--;
    g->players[previous_player_index].border_empty_fields -= lost_border_empty_fields;

    return true;
}

uint64_t gamma_busy_fields(gamma_t *g, uint32_t player) {
    if (g == NULL || player == 0 || player > g->players_num) {
        return 0;
    }

    return g->players[player % g->players_num].occupied_fields;
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

    return g->players[player_index].border_empty_fields;
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
 * @brief Zwraca liczbę cyfr zadanej liczby nieujemnej.
 * @param[in] value   - liczba nieujemna.
 * @return Liczbę cyfr liczby @p value.
 */
static unsigned get_uint_length(uint64_t value) {
    unsigned l = 1;
    while (value > 9) {
        l++;
        value /= 10;
    }
    return l;
}

error_t gamma_render_field(const gamma_t *g, char *str, uint32_t x, uint32_t y,
                           uint32_t field_width, int *written_characters) {
    if (g->board[y][x].empty) {
        *written_characters = sprintf(str, "%*c", field_width, '.');
    } else {
        *written_characters = sprintf(str, "%*u", field_width, g->board[y][x].player);
    }
    if (*written_characters < 0) {
        return INVALID_VALUE;
    }
    return NO_ERROR;
}

void gamma_rendered_fields_width(const gamma_t *g, unsigned *first_column_width,
                                 unsigned *field_width) {
    uint64_t max_player = 1; // Najmniejszy numer gracza to 1.
    for (uint64_t p = 1; p <= g->players_num; p++) {
        if (g->players[p % g->players_num].occupied_fields > 0 && p > max_player) {
            max_player = p;
        }
    }
    unsigned min_width = get_uint_length(max_player);
    // Jeżeli min_width > 1, to dodajemy jeden dodatkowy znak paddingu, aby "najdłuższy"
    // gracz nie sklejał się z poprzednim.
    *field_width = min_width == 1 ? 1 : min_width + 1;

    uint64_t max_player_first_column = 1;
    for (uint64_t r = 0; r < g->height; r++) {
        if (!g->board[r][0].empty && g->board[r][0].player > max_player_first_column) {
            max_player_first_column = g->board[r][0].player;
        }
    }
    *first_column_width = get_uint_length(max_player_first_column);
}

char *gamma_board(gamma_t *g) {
    if (g == NULL) {
        return NULL;
    }

    unsigned min_width, min_first_column_width;
    gamma_rendered_fields_width(g, &min_first_column_width, &min_width);
    uint64_t written_fields = 0, allocated_space = 0, pos = 0;
    const uint64_t total_fields = g->width * g->height;
    const unsigned min_buffer_size = 50;
    char *str = NULL;

    for (int64_t y = g->height - 1; y >= 0; y--, written_fields++) {
        for (uint32_t x = 0; x < g->width; x++) {
            if ((allocated_space - pos) < min_buffer_size) {
                uint64_t left_fields = total_fields - written_fields;
                uint64_t extra_chars = left_fields * min_width + y + min_buffer_size;
                allocated_space += extra_chars * sizeof(char);
                if ((str = realloc(str, allocated_space)) == NULL) {
                    errno = ENOMEM;
                    free(str);
                    return NULL;
                }
            }

            unsigned field_width = x == 0 ? min_first_column_width : min_width;
            int written_chars;
            gamma_render_field(g, &str[pos], x, y, field_width, &written_chars);
            pos += (unsigned)written_chars;
        }
        str[pos++] = '\n';
    }

    str[pos] = '\0';
    return str;
}

bool gamma_is_valid_player(const gamma_t *g, uint32_t player) {
    return !(g == NULL || player == 0 || player > g->players_num);
}

bool gamma_game_new_arguments_valid(uint32_t width, uint32_t height, uint32_t players,
                                    uint32_t areas) {
    return !(width == 0 || height == 0 || players == 0 || areas == 0);
}

uint32_t gamma_players_number(gamma_t *g) {
    return g == NULL ? 0 : g->players_num;
}

uint32_t gamma_board_width(gamma_t *g) {
    return g == NULL ? 0 : g->width;
}

uint32_t gamma_board_height(gamma_t *g) {
    return g == NULL ? 0 : g->height;
}

bool gamma_game_move_arguments_valid(const gamma_t *g, uint32_t player, uint32_t x,
                                     uint32_t y) {
    return !(g == NULL || player == 0 || player > g->players_num || x >= g->width ||
             y >= g->height || !g->board[y][x].empty ||
             would_exceed_areas_limit(g, player, x, y));
}

bool gamma_game_golden_move_arguments_valid(const gamma_t *g, uint32_t player,
                                            uint32_t x, uint32_t y) {
    return !is_golden_move_impossible(g, player, x, y);
}
