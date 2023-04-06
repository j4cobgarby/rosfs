#ifndef __INCLUDE_CIRC_BUFF_H__
#define __INCLUDE_CIRC_BUFF_H__

#define _GNU_SOURCE

struct circ_buff {
    void **arr;
    int end;
    int start;
    int length;
};

void circ_buff_init(struct circ_buff *buff, int length);
void circ_buff_deinit(struct circ_buff *buff);
void circ_buff_put(struct circ_buff *buff, void *x);
void *circ_buff_get(struct circ_buff *buff);

#endif
