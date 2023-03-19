#include <rcl/allocator.h>
#include <rcl/macros.h>
#include <rcl/publisher.h>
#include <rcl/subscription.h>
#include <rcl/validate_topic_name.h>
#include <rcl/types.h>

#include <rclc/executor_handle.h>
#include <rcutils/allocator.h>
#include <rosidl_runtime_c/message_type_support_struct.h>

#include <std_msgs/msg/detail/int32__functions.h>
#include <std_msgs/msg/detail/int32__struct.h>
#include <std_msgs/msg/int32.h>

#include <rclc/subscription.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rclc/types.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TOPICS 100

#define RCL_VERIFY(expr, msg)\
    {rcl_ret_t __ret = expr;\
    if (__ret != RCL_RET_OK) {\
    fprintf(stderr, "[ERROR: %d]\t" msg, __ret);return -1;} else {}}


rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

void _dummy_cbk(const void *msg) {
    printf("Dummy callback called(!?)\n");
}

void ctx_cbk(const void *msg, void *ctx) {
    printf("Callback!\n");
}

int main(int argc, const char **argv) {
    allocator = rcl_get_default_allocator();

    RCL_VERIFY(rclc_support_init(&support, argc, argv, &allocator), "Error while initialising RCLC support.\n");
    RCL_VERIFY(rclc_node_init_default(&node, "pubsubfs", "", &support), "Error while intialising node.\n");

    executor = rclc_executor_get_zero_initialized_executor();
    RCL_VERIFY(rclc_executor_init(&executor, &support.context, MAX_TOPICS, &allocator), "Failed to initialise executor\n");

    printf("Spinng a little bit\n");
    rclc_executor_spin_some(&executor, 1000000);
    printf("Now going to make subscription.\n");

    // Create dummy subscriber
    rcl_subscription_t _dummy_sub;
    const char *_dummy_topic = "testtopic";
    const rosidl_message_type_support_t *_dummy_support = ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32);
    std_msgs__msg__Int32 _dummy_msg;
    std_msgs__msg__Int32__init(&_dummy_msg);
    RCL_VERIFY(rclc_subscription_init_default(&_dummy_sub, &node, _dummy_support, _dummy_topic), "Failed to initialise dummy subscription.\n");
    //RCL_VERIFY(rclc_executor_add_subscription(&executor, &_dummy_sub, &_dummy_msg, _dummy_cbk, ON_NEW_DATA), "Failed to setup dummy callback.\n");
    RCL_VERIFY(rclc_executor_add_subscription_with_context(&executor, &_dummy_sub, &_dummy_msg, ctx_cbk, &_dummy_sub, ON_NEW_DATA), "Failed to setup callback.\n");

    while (1) {
        if (rclc_executor_spin_some(&executor, 1000) != RCL_RET_OK) {
            printf("RCLC executor failed.\n");
            break;
        }
    }

    return 0;
}
