#ifndef __INCLUDE_ROSFS_H__
#define __INCLUDE_ROSFS_H__

#define _GNU_SOURCE

#include <rcl/publisher.h>
#include <rcl/subscription.h>

#include "circ_buff.h"
#include "type.h"

#define RCL_VERIFY(expr, msg)\
    {rcl_ret_t __ret = expr;\
    if (__ret != RCL_RET_OK) {\
    fprintf(stderr, "[ERROR: %d]\t" msg, __ret);return -1;} else {}}

#define MAX_TOPICS 100 // Max topics to subscribe and publish to. Max topics in total is this * 2

union rosfs_pubsub_context {
    struct {
        rcl_publisher_t *publisher;
        struct rosfs_msg_type *type;
    } as_pub;
    struct {
        rcl_subscription_t *subscription;
        void *sub_msg; // Temporary message used to store subscription messages in before they're copied to the queue
        struct circ_buff msg_queue; // A circular buffer queue of messages
        struct rosfs_msg_type *type;
    } as_sub;
};

#endif /*__INCLUDE_ROSFS_H__*/
