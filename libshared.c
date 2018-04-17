/*
 **************************************************************************************
 *       Filename:  libshared.c
 *    Description:   source file
 *
 *        Version:  1.0
 *        Created:  2017-10-09 15:41:02
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */
#define LOG_TAG "shared"
#include "log.h"

#include <pthread.h>
#include <unistd.h>

static void* thread_loop(void* data) {
    logd("hello %s", __func__);
    int* p = (int*)0x100;
    *p = 100;
    return p;
}

void init_shared() {
    logd("hello %s", __func__);
    /*int* p = (int*)0x100;*/
    /**p = 100;            */
    pthread_t tid;
    pthread_create(&tid, NULL, thread_loop, 0);
    sleep(1);
}


/********************************** END **********************************************/

