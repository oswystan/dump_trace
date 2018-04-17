/*
 **************************************************************************************
 *       Filename:  main.c
 *    Description:   source file
 *
 *        Version:  1.0
 *        Created:  2017-09-14 21:51:55
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */

#define LOG_TAG "main"
#include "log.h"
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "libshared.h"

void fun() {
}

void fun1() {
    fun();
}

void fun2() {
    char* p = (char*)0x000004;
    *p = 0;
    fun1();
}

void fun3() {
    fun2();
}

void fun4() {
    fun3();
}
void do_it() {
    fun4();
}

int main(int argc, const char *argv[]) {
    logfunc();
    logd("argv0=%s", argv[0]);
    logd("pid: %d", getpid());
    init_shared();
    do_it();
    return 0;
}

/********************************** END **********************************************/
