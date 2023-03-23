#ifndef __INCLUDE_ROSFS_H__
#define __INCLUDE_ROSFS_H__

#define RCL_VERIFY(expr, msg)\
    {rcl_ret_t __ret = expr;\
    if (__ret != RCL_RET_OK) {\
    fprintf(stderr, "[ERROR: %d]\t" msg, __ret);return -1;} else {}}

#define MAX_TOPICS 100 // Max topics to subscribe and publish to. Max topics in total is this * 2

#endif /*__INCLUDE_ROSFS_H__*/
