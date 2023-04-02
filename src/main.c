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
#include "type.h"

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rclc_executor_t executor;

pthread_mutex_t mut_executor = PTHREAD_MUTEX_INITIALIZER;

hashmap typemap;

void subscription_cbk(const void *msg, void *ctx_void) {
    union rosfs_pubsub_context *ctx = ctx_void;

    void *msg_cpy = malloc(ctx->as_sub.type->size);
    memcpy(msg_cpy, msg, ctx->as_sub.type->size);
    circ_buff_put(&ctx->as_sub.msg_queue, msg_cpy);


    printf("Subscription callback\n");
}

int fs_init(void) {
    printf("Initialising ROSFS filesystem.\n");
    return 0;
}

int fs_make_subscriber(const char *topic_and_type, int flags, void **typedata) {
    RCLC_UNUSED(flags);
    
    int result;
    union rosfs_pubsub_context **context;
    struct rosfs_msg_type *type;

    char *type_name;
    char *topic_name;

    // Check that topic name is valid
    // The topic name for rosfs will come in the form:
    //  "name:type"
    // where type is a fully qualified ros type like std_msgs/msg/Int32.

    char *topic_and_type_copy = malloc(strlen(topic_and_type) * sizeof(char));
    strcpy(topic_and_type_copy, topic_and_type);

    type_name = strchr(topic_and_type_copy, ':');
    if (!type_name) {
        printf("No type specified for topic %s\n", topic_and_type);
        return -1;
    }

    // Split topic_and_type_copy into two strings. topic_and_type_copy now points to
    // a whole string of just the topic name, and type_name points to everything after
    // the ':' (which is the type name).
    *(type_name++) = '\0';
    topic_name = topic_and_type_copy;

    RCL_VERIFY(rcl_validate_topic_name(topic_name, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a subscriber to '%s' because the name is invalid. (%d)\n", topic_name, result);
        return -1;
    }

    type = hashmap_get(&typemap, serialise_string(type_name));
    if (!type) {
        printf("Could not find type %s. Perhaps it exists but is not loaded in rosfs.\n", type_name);
        return -1;
    }

    printf("Creating a subscriber to '%s' of type %s.\n", topic_name, type_name);

    context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_sub.subscription = malloc(sizeof(rcl_subscription_t));
    *(*context)->as_sub.subscription = rcl_get_zero_initialized_subscription();
    (*context)->as_sub.sub_msg = malloc(type->size);
    type->msg_init((*context)->as_sub.sub_msg);
    (*context)->as_sub.type = type;

    printf("Initialising circular buffer.\n");
    circ_buff_init(&(*context)->as_sub.msg_queue, 64);
    printf("Circular buffer initialised with length %d\n", (*context)->as_sub.msg_queue.length);

    pthread_mutex_lock(&mut_executor);
    RCL_VERIFY(rclc_subscription_init_default(
        (*context)->as_sub.subscription,
        &node, 
        type->type_support, 
        topic_name),
        "Error while initialising subscriber.\n");

    RCL_VERIFY(rclc_executor_add_subscription_with_context(&executor,
        (*context)->as_sub.subscription, (*context)->as_sub.sub_msg, 
        &subscription_cbk, *context, ON_NEW_DATA),
        "Error while adding subscription to executor.\n");
    pthread_mutex_unlock(&mut_executor);

    printf("Subscriber created successfully.\n");

    return 0;
}

int fs_make_publisher(const char *topic_and_type, int flags, void **typedata) {
    RCLC_UNUSED(flags);

    int result;

    struct rosfs_msg_type *type;

    char *type_name;
    char *topic_name;

    char *topic_and_type_copy = malloc(strlen(topic_and_type) * sizeof(char));
    strcpy(topic_and_type_copy, topic_and_type);

    type_name = strchr(topic_and_type_copy, ':');
    if (!type_name) {
        printf("No type specified for topic %s\n", topic_and_type);
        return -1;
    }

    // Split topic_and_type_copy into two strings. topic_and_type_copy now points to
    // a whole string of just the topic name, and type_name points to everything after
    // the ':' (which is the type name).
    *(type_name++) = '\0';
    topic_name = topic_and_type_copy;

    RCL_VERIFY(rcl_validate_topic_name(topic_name, &result, NULL), "Failed to check a topic name.\n");

    if (result != RCL_TOPIC_NAME_VALID) {
        printf("Cannot make a publisher to '%s' because the name is invalid. (%d)\n", topic_name, result);
        return -1;
    }

    type = hashmap_get(&typemap, serialise_string(type_name));
    if (!type) {
        printf("Could not find type %s. Perhaps it exists but is not loaded in rosfs.\n", type_name);
        return -1;
    }

    printf("Creating a publisher to '%s' with type %s.\n", topic_name, type_name);
    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    *context = malloc(sizeof(union rosfs_pubsub_context));
    (*context)->as_pub.publisher = malloc(sizeof(rcl_publisher_t));
    (*context)->as_pub.type = type;

    pthread_mutex_lock(&mut_executor);
    RCL_VERIFY(rclc_publisher_init_default(
        (*context)->as_pub.publisher,
        &node,
        type->type_support,
        topic_name), 
        "Error while initialising publisher.\n");
    pthread_mutex_unlock(&mut_executor);

    printf("Publisher created successfully.\n");

    return 0;
}

int fs_publish(const char *topic, const char *buff, size_t size, void **typedata) {
    RCL_UNUSED(size);
    RCL_UNUSED(topic);
    
    void *msg;
    rcl_publisher_t *pub;

    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    pub = (*context)->as_pub.publisher;
    struct rosfs_msg_type *type = (*context)->as_pub.type;

    msg = malloc(type->size);
    type->string_to_msg(msg, buff);
    
    RCL_VERIFY(rcl_publish(pub, &msg, NULL), "Error while publishing a message.\n");

    free(msg); // No longer needed after publishing

    return 0;
}

int fs_pop(const char *topic, char *buff, void **typedata) {
    union rosfs_pubsub_context **context = (union rosfs_pubsub_context**)typedata;
    struct rosfs_msg_type *type = (*context)->as_pub.type;
    
    void *msg_void = circ_buff_get(&(*context)->as_sub.msg_queue);

    if (!msg_void) return -1;
    
    //TODO: Allocate space for string in buff
    type->msg_to_string(msg_void, buff);

    printf("Popping message (%s) from %s\n", buff, topic);

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
    union rosfs_pubsub_context *__ctx;
    pthread_t fs_thread;

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


    // Here we pass '1' as the argc value so that RCLC will not use any command line arguments,
    // since we only want to use command line arguments for FUSE.
    RCL_VERIFY(rclc_support_init(&support, 1, argv, &allocator), "Error while initialising RCLC support.\n");
    RCL_VERIFY(rclc_node_init_default(&node, "pubsubfs", "", &support), "Error while intialising node.\n");

    executor = rclc_executor_get_zero_initialized_executor();
    RCL_VERIFY(rclc_executor_init(&executor, &support.context, MAX_TOPICS, &allocator), "Failed to initialise executor\n");

    rosfs_type_system_init(&typemap);
    
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

    // for (size_t i = 0; i < publishers.size; i++) {
    //     struct hm_entry *ent = &publishers.table[i];
    //     struct topic_info *top = ent->v;
    //     if (HM_ENTRY_PRESENT(ent)) {
    //         RCL_VERIFY(rcl_publisher_fini(top->typedata, &node), "Failed to finish publisher.\n");
    //         printf("Successfully finished publisher.\n");
    //     }
    // }

    // for (size_t i = 0; i < subscribers.size; i++) {
    //     struct hm_entry *ent = &subscribers.table[i];
    //     struct topic_info *top = ent->v;
    //     if (HM_ENTRY_PRESENT(ent)) {
    //         RCL_VERIFY(rcl_subscription_fini(top->typedata, &node), "Failed to finish subscription.\n");
    //         printf("Successfully finished subscription.\n");
    //     }
    // }

    return 0;
}
