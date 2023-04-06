#include "type.h"

#include <geometry_msgs/msg/detail/vector3__struct.h>
#include <rosidl_runtime_c/message_type_support_struct.h>
#include <std_msgs/msg/detail/float32__struct.h>
#include <std_msgs/msg/detail/int32__struct.h>

#include <stdio.h>

#include <pubsubfs/hashmap.h>

#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/float32.h>

#include <geometry_msgs/msg/vector3.h>

#define MAX_TYPES 64

// Hashing function for the type hashmap.
static size_t hsh(int n) {
    return n % MAX_TYPES;
}

static int _str_to_Int32(void *i_void, const char *str) {
    std_msgs__msg__Int32 *i = i_void;
    return 1 == sscanf(str, "%d", &i->data) ? 0 : -1;
}

static int _Int32_to_str(void *i_void, char *str, size_t size) {
    std_msgs__msg__Int32 *i = i_void;
    return 1 == snprintf(str, size, "%d", i->data) ? 0 : -1;
}

static int _str_to_Float32(void *f_void, const char *str) {
    std_msgs__msg__Float32 *f = f_void;
    return 1 == sscanf(str, "%f", &f->data) ? 0 : -1;
}

static int _Float32_to_str(void *f_void, char *str, size_t size) {
    std_msgs__msg__Float32 *f = f_void;
    return 1 == snprintf(str, size, "%f", f->data) ? 0 : -1;
}

static int _str_to_Vector3(void *v_void, const char *str) {
    geometry_msgs__msg__Vector3 *v = v_void;
    return 3 == sscanf(str, "%lf\n%lf\n%lf", &v->x, &v->y, &v->z) ? 0 : 1;
}

static int _Vector3_to_str(void *v_void, char *str, size_t size) {
    geometry_msgs__msg__Vector3 *v = v_void;
    return 3 == snprintf(str, size, "%lf\n%lf\n%lf\n", v->x, v->y, v->z) ? 0 : 1;
}

static struct rosfs_msg_type type_array[MAX_TYPES];
static int types_count = 0; // Index incrememented when filling type_array

void rosfs_type_system_init(hashmap *typemap) {
#define SETUP_TYPE(PKG, FOLDER, TYPE) \
type_array[types_count].size = sizeof(PKG ## __ ## FOLDER ## __ ## TYPE);\
type_array[types_count].type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(PKG, FOLDER, TYPE);\
type_array[types_count].msg_init = (bool (*)(void*)) PKG ## __ ## FOLDER ## __ ## TYPE ## __ ## init;\
type_array[types_count].msg_to_string = _ ## TYPE ## _to_str;\
type_array[types_count].string_to_msg = _str_to_ ## TYPE;\
printf("Adding type @ %p to hashmap\n", (void*)(type_array + types_count));\
hashmap_add(typemap, serialise_string(#PKG "/" #FOLDER "/" #TYPE), &(type_array[types_count++]));

    *typemap = hashmap_new(MAX_TYPES, hsh);

    SETUP_TYPE(std_msgs, msg, Int32);
    SETUP_TYPE(std_msgs, msg, Float32);
    SETUP_TYPE(geometry_msgs, msg, Vector3);

    // Undefine this macro here since it is only valid within rosfs_type_system_init.
    // This way we make sure it's only called when it's valid
    #undef SETUP_TYPE

    printf("Setup %d types.\n", types_count);
}
