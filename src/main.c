#include <rcl/macros.h>
#include <rcl/publisher.h>
#include <rcl/subscription.h>
#include <rclc/subscription.h>
#include <rclc/types.h>
#include <rcutils/allocator.h>
#include <rosidl_runtime_c/message_type_support_struct.h>
#include <std_msgs/msg/detail/int32__struct.h>
#include <std_msgs/msg/detail/string__struct.h>

#include <rcl/types.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rcl/validate_topic_name.h>

#include <pubsubfs/pubsubfs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RCL_VERIFY(expr, msg)\
    {if ((expr) != RCL_RET_OK) {\
        fprintf(stderr, msg);return -1;} else {}}

#define MAX_TOPICS 100 // Max topics to subscribe and publish to. Max topics in total is this * 2

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

void subscription_cbk(const void *msg, void *ctx) {
    rcl_subscription_t *sub = (rcl_subscription_t*)ctx;
    const char *msg_s = (const char *)msg;

    printf("Subscription callback from %s\n", msg_s);
}

int fs_init(void) {
    printf("Initialising ROSFS filesystem.\n");
    return 0;
}

int fs_make_subscriber(const char *topic, int flags, void *typedata) {
    RCLC_UNUSED(flags);
    
    int result;

    RCL_VERIFY(rcl_validate_topic_name(topic, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a subscriber to '%s' because the name is invalid. (%d)\n", topic, result);
        return -1;
    }

    printf("Creating a subscriber to '%s'.", topic);

    RCL_VERIFY(rclc_subscription_init_default(
        (rcl_subscription_t*)typedata,
        &node, 
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), 
        topic), 
        "Error while initialising subscriber.\n");

    RCL_VERIFY(rclc_executor_add_subscription_with_context(&executor,
        (rcl_subscription_t*)typedata, malloc(sizeof(std_msgs__msg__Int32)), 
        &subscription_cbk, typedata, ON_NEW_DATA),
        "Error while adding subscription to executor.\n");

    printf("Subscriber created successfully.\n");

    return 0;
}

int fs_make_publisher(const char *topic, int flags, void *typedata) {
    RCLC_UNUSED(flags);

    int result;

    RCL_VERIFY(rcl_validate_topic_name(topic, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a publisher to '%s' because the name is invalid. (%d)\n", topic, result);
        return -1;
    }

    printf("Creating a publisher to '%s'.", topic);

    RCL_VERIFY(rclc_publisher_init_default(
        (rcl_publisher_t*)typedata,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        topic), 
        "Error while initialising publisher.\n");

    printf("Publisher created successfully.\n");

    return 0;
}

int fs_publish(const char *topic, const char *buff, size_t size) {
    RCL_UNUSED(size);
    
    std_msgs__msg__Int32 msg;
    struct topic_info *top_inf = find_topic(&publishers, topic);
    rcl_publisher_t *pub;

    if (top_inf == NULL) {
        printf("Couldn't find topic '%s'\n", topic);
        return -1;
    }

    pub = (rcl_publisher_t*)top_inf->typedata;

    msg.data = atoi(buff);
    
    RCL_VERIFY(rcl_publish(pub, &msg, NULL), "Error while publishing a message.\n");

    return 0;
}

int fs_pop(const char *topic, char *buff) {
    return 0;
}

int fs_subscription_status(const char *topic, struct substat *buff) {
    return 0;
}

int fs_get_topics(struct topic_info* inf) {
    return 0;
}

int fs_get_services(struct service_info* inf) {
    return 0;
}

int fs_call_service(const char *service, char *buff) {
    return 0;
}

int main(int argc, const char **argv) {
    allocator = rcutils_get_default_allocator();

    struct pubsubfs_type fs_type = {
        .init = fs_init,
        .call = fs_call_service,
        .getservices = fs_get_services,
        .gettopics = fs_get_topics,
        .mkpub = fs_make_publisher,
        .mksub = fs_make_subscriber,
        .pop = fs_pop,
        .publish = fs_publish,
        .substat = fs_subscription_status,
    };

    RCL_VERIFY(rclc_support_init(&support, argc, argv, &allocator), "Error while initialising RCLC support.\n");
    RCL_VERIFY(rclc_node_init_default(&node, "pubsubfs", "", &support), "Error while intialising node.\n");

    executor = rclc_executor_get_zero_initialized_executor();
    rclc_executor_init(&executor, &support.context, MAX_TOPICS * 2, &allocator);

    //start_interface(fs_type, argc, argv);

    rclc_executor_spin(&executor);

    return 0;
}
