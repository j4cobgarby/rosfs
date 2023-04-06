#ifndef __INCLUDE_TYPE_H__
#define __INCLUDE_TYPE_H__

#define _GNU_SOURCE

#include <rosidl_runtime_c/message_type_support_struct.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pubsubfs/hashmap.h>

/* ROS supports message of many types, each of which are defined in
    special files. This struct provides a way to store some helper data
    and functions for each type we want to use.
*/
struct rosfs_msg_type {
    size_t size;
    bool (*msg_init)(void *); // Function to initialise a message of this type. Something like std_msgs__msg__Int32__init
    const rosidl_message_type_support_t *type_support;
    int (*string_to_msg)(void *, const char *);
    int (*msg_to_string)(void *, char *, size_t size);
};

void rosfs_type_system_init(hashmap *typemap);

#endif
