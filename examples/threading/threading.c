#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    if (thread_func_args == NULL) {
        return NULL;
    }

    // 1. 获取互斥锁前等待（usleep 单位是微秒，需乘以 1000）
    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    // 2. 申请并获取互斥锁
    int rc = pthread_mutex_lock(thread_func_args->mutex);
    if (rc != 0) {
        ERROR_LOG("pthread_mutex_lock failed with code %d", rc);
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    // 3. 持有锁的同时休眠等待
    usleep(thread_func_args->wait_to_release_ms * 1000);

    // 4. 释放互斥锁
    rc = pthread_mutex_unlock(thread_func_args->mutex);
    if (rc != 0) {
        ERROR_LOG("pthread_mutex_unlock failed with code %d", rc);
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    // 标记成功并返回结构体指针
    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    // 1. 在堆上动态分配结构体内存（不能在栈上分配，否则函数退出时内存会被破坏）
    struct thread_data *data = (struct thread_data *)malloc(sizeof(struct thread_data));
    if (data == NULL) {
        ERROR_LOG("Failed to allocate memory for thread_data");
        return false;
    }

    // 2. 初始化结构体成员
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    // 3. 创建线程并传入 data 作为参数
    int rc = pthread_create(thread, NULL, threadfunc, (void *)data);
    if (rc != 0) {
        ERROR_LOG("pthread_create failed with code %d", rc);
        free(data); // 线程创建失败时必须释放内存防止泄漏
        return false;
    }

    // 线程创建成功直接返回 true（注意：不要在这里 free(data)，测试框架 join 之后会去读取并释放它）
    return true;
}

