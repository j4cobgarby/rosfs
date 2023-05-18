#include "circ_buff.h"
#include <stdio.h>
#include <stdlib.h>

void circ_buff_init(struct circ_buff *buff, int length) {
    buff->arr = calloc(length, sizeof(void*));
    buff->length = length;
}

void circ_buff_deinit(struct circ_buff *buff) {
    free(buff->arr);
}

void circ_buff_put(struct circ_buff *buff, void *x) {
    buff->arr[buff->end++] = x;
    buff->end %= buff->length;
}

void *circ_buff_get(struct circ_buff *buff) {
    void *ret = buff->arr[buff->start];
    buff->arr[buff->start] = NULL;

    buff->start += 1;
    buff->start %= buff->length;
    return ret;
}
