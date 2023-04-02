#include "circ_buff.h"
#include <stdio.h>
#include <stdlib.h>

void circ_buff_init(struct circ_buff *buff, int length) {
    buff->arr = calloc(length, sizeof(void*));
    printf("buff->arr = %p\n", (void*)buff->arr);
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
    printf("circ_buff_get: buff->arr = %p, buff->length = %d\n", (void*)buff->arr, buff->length);
    void *ret = buff->arr[buff->start++];
    buff->start %= buff->length;
    return ret;
}
