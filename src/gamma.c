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
    uint32_t player;
    uint64_t area;
    uint8_t flags;
    field_t *parent; // for find-union algorithm
    uint64_t size;   // for find-union algorithm
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

typedef uint8_t field_flag_t;
const field_flag_t EMPTY_FIELD_FLAG = 1u << 0u;
const field_flag_t FIELD_VISITED_MASK = 1u << 1u;
const field_flag_t FIELD_AREA_VALID_MASK = 1u << 2u;

/**
 * Struktura przechowująca stan gry.
 */
struct gamma {
    uint32_t max_areas;
    uint32_t players_num;
    uint32_t height;
    uint32_t width;

    uint64_t occupied_fields;
    uint64_t next_area;

    field_flag_t field_visited_expected_flag;
    field_flag_t field_area_valid_expected_flag;

    player_t *players;
    field_t **board;
};

field_t **allocate_board(uint32_t width, uint32_t height) {
    field_t **board = malloc(height * sizeof(field_t *));
    if (board == NULL) {
        return NULL;
    }

    uint32_t allocated_rows = 0;
    for (; allocated_rows < height; allocated_rows++) {
        board[allocated_rows] = malloc((uint64_t)width * sizeof(field_t));
        if (board[allocated_rows] == NULL) {
            break;
        }
        for (uint32_t i = 0; i < width; i++) {
            board[allocated_rows][i].flags = EMPTY_FIELD_FLAG;
            board[allocated_rows][i].parent = &board[allocated_rows][i];
            board[allocated_rows][i].size = 1;
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
    game->next_area = 0;

    game->field_visited_expected_flag = FIELD_VISITED_MASK;
    game->field_area_valid_expected_flag = FIELD_AREA_VALID_MASK;

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
    return is_within_board(g, x, y) && g->board[y][x].player == player;
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

bool fu_reindex(gamma_t *g) {
    for (uint32_t p = 0; p < g->players_num; p++) {
        g->players[p].areas = 0;
    }

    for (int64_t row = 0; row < g->height; row++) {
        for (int64_t column = 0; column < g->width; column++) {
        }
    }

    return true;
}

static inline field_t *fu_find(field_t *field) {
    // path halving method

    while (field->parent != field) {
        field->parent = field->parent->parent;
        field = field->parent;
    }

    return field;
}

static inline field_t *fu_union(field_t *x, field_t *y) {
    // union by size method

    field_t *x_root = fu_find(x);
    field_t *y_root = fu_find(y);

    if (x_root == y_root) {
        // x and y are already in the same set
        return x_root;
    }

    if (x_root->size < y_root->size) {
        field_t *tmp = x_root;
        x_root = y_root;
        y_root = tmp;
    }

    // merge y_root into x_root
    y_root->parent = x_root;
    x_root->size = x_root->size + y_root->size;

    return x_root;
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
    if (g == NULL || player == 0 || player > g->players_num) {
        return false;
    }

    return false;
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
    if (g == NULL || player == 0 || player > g->players_num) {
        return false;
    }

    return false;
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
 * @param[in] buffer - bufor do którego zapisany ma zostać wynik.
 * @param[in] n - liczba do skonwertowania.
 * @return Liczba zapisanych bajtów.
 */
int uint_to_string(char* buffer, uint32_t n) {
    char digits[11];  // max_len = round(log10(2^32)) = 10
    uint8_t written = 0;

    do {
        // this writes the digits in reversed order
        digits[written++] = n % 10;
        n /= 10;
    } while (n != 0);

    for(int p = written - 1; p >= 0; p--) {
        *buffer++ = digits[p];
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
    if(str == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    uint64_t pos = 0;
    for(uint64_t field = 0; field < total_fields; field++){
        uint32_t x = field / g->height;
        uint32_t y = field % g->width;
        
        const uint64_t left_buffer_space = allocated_space - pos;
        if (left_buffer_space < 50) { // max needed for 1 field is 33 bytes
            allocated_space = allocated_space + (total_fields - field) + 50;
            str = realloc(str, allocated_space);
            if (str == NULL) {
                errno = ENOMEM;
                return NULL;
            }
        }

        if(y == 0 && x != 0) {
            str[pos++] = '\n';
        }

        if(g->board[x][y].flags & EMPTY_FIELD_FLAG) {
            str[pos++] = '.';
        }
        else {
            uint32_t player = g->board[x][y].player;
            if (player < 10) {
                str[pos++] = ASCII_ZERO + player;
            }
            else {
                str[pos++] = '[';
                pos += uint_to_string(str, player);
                str[pos++] = ']';
            }
        }
    }

    return NULL;
}
