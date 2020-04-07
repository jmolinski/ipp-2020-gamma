#ifndef GAMMA_BOARD_H
#define GAMMA_BOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct field field_t;

/**
 * Struktura przechowująca stan pola.
 */
struct field {
    uint32_t player;
    bool empty;
    field_t *parent; // for find-union algorithm
    uint64_t size;   // for find-union algorithm
};

field_t **allocate_board(uint32_t width, uint32_t height);
field_t *fu_find(field_t *field);
bool fu_union(field_t *x, field_t *y);

#endif // GAMMA_BOARD_H
