#include <pubsubfs/hashmap.h>
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
#include <string.h>

#include "circ_buff.h"
#include "rosfs.h"

union rosfs_pubsub_context {
    struct {
        rcl_publisher_t *publisher;
    } as_pub;
    struct {
        rcl_subscription_t *subscription;
        void *sub_msg; // Temporary message used to store subscription messages in before they're copied to the queue
        struct circ_buff msg_queue; // A circular buffer queue of messages
    } as_sub;
};

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

pthread_mutex_t mut_executor = PTHREAD_MUTEX_INITIALIZER;

void subscription_cbk(const void *msg, void *ctx_void) {
    union rosfs_pubsub_context *ctx = ctx_void;

    void *msg_cpy = malloc(sizeof(std_msgs__msg__Int32));
    memcpy(msg_cpy, msg, sizeof(std_msgs__msg__Int32));
    circ_buff_put(&ctx->as_sub.msg_queue, msg_cpy);

    const std_msgs__msg__Int32 *msg_i = msg;

    printf("Subscription callback: %d\n", msg_i->data);
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

    printf("Creating a subscriber to '%s'.\n", topic);

    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_sub.subscription = malloc(sizeof(rcl_subscription_t));
    *(*context)->as_sub.subscription = rcl_get_zero_initialized_subscription();

    (*context)->as_sub.sub_msg = malloc(sizeof(std_msgs__msg__Int32));
    std_msgs__msg__Int32__init((*context)->as_sub.sub_msg);

    circ_buff_init(&(*context)->as_sub.msg_queue, 64);
    
    pthread_mutex_lock(&mut_executor);
    RCL_VERIFY(rclc_subscription_init_default(
        (*context)->as_sub.subscription,
        &node, 
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), 
        topic), 
        "Error while initialising subscriber.\n");

    RCL_VERIFY(rclc_executor_add_subscription_with_context(&executor,
        (*context)->as_sub.subscription, (*context)->as_sub.sub_msg, 
        &subscription_cbk, *context, ON_NEW_DATA),
        "Error while adding subscription to executor.\n");
    pthread_mutex_unlock(&mut_executor);

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

    printf("Creating a publisher to '%s'.\n", topic);
    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_pub.publisher = malloc(sizeof(rcl_publisher_t));

    pthread_mutex_lock(&mut_executor);
    RCL_VERIFY(rclc_publisher_init_default(
        (*context)->as_pub.publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        topic), 
        "Error while initialising publisher.\n");
    pthread_mutex_unlock(&mut_executor);

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
    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    void *msg_void = circ_buff_get(&(*context)->as_sub.msg_queue);
    std_msgs__msg__Int32 *msg = msg_void;
    
    sprintf(buff, "data\t%d", msg->data);

    printf("Popping message (%d) from %s\n", msg->data, topic);

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

void _dummy_cbk(const void *msg) {
    printf("Dummy callback called(!?)\n");
}

int main(int argc, const char **argv) {
    union rosfs_pubsub_context *__ctx;

    allocator = rcl_get_default_allocator();

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
    RCL_VERIFY(rclc_executor_init(&executor, &support.context, MAX_TOPICS, &allocator), "Failed to initialise executor\n");

    printf("Running a bit.\n");
    for (int i = 0; i < 1000; i++) {
        rclc_executor_spin_some(&executor, 1000);
    }
    printf("Making initial subs.\n");

    fs_make_subscriber("testsub", 0, (void**)&__ctx);

    // Create dummy subscriber
    rcl_subscription_t _dummy_sub;
    const char *_dummy_topic = "__rosfs_dummy__";
    const rosidl_message_type_support_t *_dummy_support = ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32);
    std_msgs__msg__Int32 _dummy_msg;
    std_msgs__msg__Int32__init(&_dummy_msg);
    RCL_VERIFY(rclc_subscription_init_default(&_dummy_sub, &node, _dummy_support, _dummy_topic), "Failed to initialise dummy subscription.\n");
    RCL_VERIFY(rclc_executor_add_subscription(&executor, &_dummy_sub, &_dummy_msg, _dummy_cbk, ON_NEW_DATA), "Failed to setup dummy callback.\n");

    printf("Ready to start FS thread.\n");

    if (pthread_create(&fs_thread, NULL, startfs, &fsargs) != 0) {
        printf("Error creating FS thread.\n");
        return -1;
    }

    while (1) {
        pthread_mutex_lock(&mut_executor);
        if (rclc_executor_spin_some(&executor, 1000) != RCL_RET_OK) {
            printf("RCLC executor failed.\n");
            pthread_mutex_unlock(&mut_executor);
            break;
        }
        pthread_mutex_unlock(&mut_executor);
    }

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
