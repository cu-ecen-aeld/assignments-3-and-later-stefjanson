#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define DEBUG_LOG(msg,...)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *)thread_param;
    // sleep
    sleep((thread_func_args->time_obtain_ms) / 1000);
    // lock mutex
    pthread_mutex_lock(thread_func_args->MUTEX);
    // sleep
    sleep((thread_func_args->time_release_ms) / 1000);
    // unlock mutex
    pthread_mutex_unlock(thread_func_args->MUTEX);
    // check if succesfull
    thread_func_args->thread_complete_success = true;
    // exit
    pthread_exit(thread_func_args);
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *tdata = malloc(sizeof(struct thread_data));
    if (tdata == NULL)
    {
        return false;
    }

    tdata->thread_complete_success = false;
    tdata->time_obtain_ms = wait_to_obtain_ms;
    tdata->time_release_ms = wait_to_release_ms;
    tdata->MUTEX = mutex;

    int result = pthread_create(thread, NULL, threadfunc, tdata);

    if (result != 0)
    {
        ERROR_LOG("Failed to create thread. Error code: %d", result);
        free(tdata);
        return false;
    }

    return true;
}
