#include <pubsubfs/hashmap.h>
#include <rcl/macros.h>
#include <rcl/publisher.h>
#include <rcl/subscription.h>
#include <rcl/validate_topic_name.h>
#include <rcl/types.h>

#include <rcutils/allocator.h>
#include <rosidl_runtime_c/message_type_support_struct.h>

#include <std_msgs/msg/detail/int32__struct.h>
#include <std_msgs/msg/detail/string__struct.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/int32.h>

#include <rclc/subscription.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rclc/types.h>

#include <pubsubfs/pubsubfs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define RCL_VERIFY(expr, msg)\
    {rcl_ret_t __ret = expr;\
    if (__ret != RCL_RET_OK) {\
    fprintf(stderr, "[ERROR: %d]\t" msg, __ret);return -1;} else {}}

#define MAX_TOPICS 100 // Max topics to subscribe and publish to. Max topics in total is this * 2

// struct rosfs_pubsub_context {
//     union {
//         struct {
//             rcl_publisher_t *publisher;
//         } as_pub;
//         struct {
//             rcl_subscription_t *subscription;
//             void **message_queue;
//         } as_sub;
//     };
// };

union rosfs_pubsub_context {
    struct {
        rcl_publisher_t *publisher;
    } as_pub;
    struct {
        rcl_subscription_t *subscription;
        void **message_queue;
    } as_sub;
};

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

void subscription_cbk(const void *msg, void *ctx) {
    rcl_subscription_t *sub = (rcl_subscription_t*)ctx;
    RCL_UNUSED(sub);
    const char *msg_s = (const char *)msg;
    

    printf("Subscription callback from %s\n", msg_s);
}

int fs_init(void) {
    printf("Initialising ROSFS filesystem.\n");
    return 0;
}

int fs_make_subscriber(const char *topic, int flags, void **typedata) {
    RCLC_UNUSED(flags);
    
    int result;

    RCL_VERIFY(rcl_validate_topic_name(topic, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a subscriber to '%s' because the name is invalid. (%d)\n", topic, result);
        return -1;
    }

    printf("Creating a subscriber to '%s'.", topic);

    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_sub.subscription = malloc(sizeof(rcl_subscription_t));
    
    RCL_VERIFY(rclc_subscription_init_default(
        (*context)->as_sub.subscription,
        &node, 
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), 
        topic), 
        "Error while initialising subscriber.\n");

    RCL_VERIFY(rclc_executor_add_subscription_with_context(&executor,
        (*context)->as_sub.subscription, malloc(sizeof(std_msgs__msg__Int32)), 
        &subscription_cbk, (*context)->as_sub.subscription, ON_NEW_DATA),
        "Error while adding subscription to executor.\n");

    printf("Subscriber created successfully.\n");

    return 0;
}

int fs_make_publisher(const char *topic, int flags, void **typedata) {
    RCLC_UNUSED(flags);

    int result;

    RCL_VERIFY(rcl_validate_topic_name(topic, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a publisher to '%s' because the name is invalid. (%d)\n", topic, result);
        return -1;
    }

    printf("Creating a publisher to '%s'.", topic);
    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_pub.publisher = malloc(sizeof(rcl_publisher_t));

    RCL_VERIFY(rclc_publisher_init_default(
        (*context)->as_pub.publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        topic), 
        "Error while initialising publisher.\n");

    printf("Publisher created successfully.\n");

    return 0;
}

int fs_publish(const char *topic, const char *buff, size_t size, void **typedata) {
    RCL_UNUSED(size);
    
    std_msgs__msg__Int32 msg;
    rcl_publisher_t *pub;

    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    pub = (*context)->as_pub.publisher;

    msg.data = atoi(buff);
    
    RCL_VERIFY(rcl_publish(pub, &msg, NULL), "Error while publishing a message.\n");

    return 0;
}

int fs_pop(const char *topic, char *buff, void **typedata) {
    printf("Popping message from %s\n", topic);

    return 0;
}

int fs_subscription_status(const char *topic, struct substat *buff, void **typedata) {
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

struct fs_thread_info {
    int argc;
    char **argv;
    struct pubsubfs_type type;
};

void *startfs(void *args_generic) {
    struct fs_thread_info *args = (struct fs_thread_info *)args_generic;
    start_interface(args->type, args->argc, args->argv);
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

    struct fs_thread_info fsargs = {
        .argc = argc,
        .argv = (char**)argv,
        .type = fs_type,
    };

    pthread_t fs_thread;

    // Here we pass '1' as the argc value so that RCLC will not use any command line arguments,
    // since we only want to use command line arguments for FUSE.
    RCL_VERIFY(rclc_support_init(&support, 1, argv, &allocator), "Error while initialising RCLC support.\n");
    RCL_VERIFY(rclc_node_init_default(&node, "pubsubfs", "", &support), "Error while intialising node.\n");

    executor = rclc_executor_get_zero_initialized_executor();
    rclc_executor_init(&executor, &support.context, MAX_TOPICS * 2, &allocator);

    printf("Ready to start FS thread.\n");

    if (pthread_create(&fs_thread, NULL, startfs, &fsargs) != 0) {
        printf("Error creating FS thread.\n");
        return -1;
    }

    rclc_executor_spin(&executor);

    printf("Executor stopped spinning.\n");

    for (size_t i = 0; i < publishers.size; i++) {
        struct hm_entry *ent = &publishers.table[i];
        struct topic_info *top = ent->v;
        if (HM_ENTRY_PRESENT(ent)) {
            RCL_VERIFY(rcl_publisher_fini(top->typedata, &node), "Failed to finish publisher.\n");
            printf("Successfully finished publisher.\n");
        }
    }

    for (size_t i = 0; i < subscribers.size; i++) {
        struct hm_entry *ent = &subscribers.table[i];
        struct topic_info *top = ent->v;
        if (HM_ENTRY_PRESENT(ent)) {
            RCL_VERIFY(rcl_subscription_fini(top->typedata, &node), "Failed to finish subscription.\n");
            printf("Successfully finished subscription.\n");
        }
    }

    return 0;
}
