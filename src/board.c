#include "board.h"
#include "gamma.h"
#include <stdlib.h>

const field_flag_t EMPTY_FIELD_FLAG = 1u << 0u;
const field_flag_t FIELD_VISITED_MASK = 1u << 1u;

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

field_t *fu_find(field_t *field) {
    // path halving method

    while (field->parent != field) {
        field->parent = field->parent->parent;
        field = field->parent;
    }

    return field;
}

bool fu_union(field_t *x, field_t *y) {
    // union by size method

    field_t *x_root = fu_find(x);
    field_t *y_root = fu_find(y);

    if (x_root == y_root) {
        // x and y are already in the same set
        return false;
    }

    if (x_root->size < y_root->size) {
        field_t *tmp = x_root;
        x_root = y_root;
        y_root = tmp;
    }

    // merge y_root into x_root
    y_root->parent = x_root;
    x_root->size = x_root->size + y_root->size;

    return true;
}