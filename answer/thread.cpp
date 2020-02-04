#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Thread.h"
#include "thread_lock.h"

const char* readyList = NULL;
const char* sleepingThreadMap = NULL;


Thread* createAndSetThreadToRun(const char* name,
                                void* (*func)(void*),
                                void* arg,
                                int pri) {
    Thread* ret = (Thread*)malloc(sizeof(Thread));
    ret->name = (char*)malloc(strlen(name) + 1);
    strcpy(ret->name, name);
    ret->func = func;
    ret->arg = arg;
    ret->priority = pri;
    ret->originalPriority = pri;

    createThread(ret);
    addToList(readyList, (void*)ret);
    return ret;
}

void destroyThread(Thread* thread) {
    free(thread->name);
    free(thread);
}

Thread* nextThreadToRun(int currentTick) {
    char line[1024];
    if (listSize(readyList) == 0)
        return NULL;
    Thread* ret = NULL;
    do {
        int threadIndex = 0;
        sprintf(line, "[nextThreadToRun] trying thread index %d\n",
                threadIndex);
        verboseLog(line);
        ret = ((Thread*)listGet(readyList, threadIndex));
        if (ret->state == TERMINATED) {
            sprintf(line, "[nextThreadToRun] thread %d was terminated\n",
                    threadIndex);
            verboseLog(line);
            removeFromList(readyList, ret);
            ret = NULL;
        }

    } while (listSize(readyList) > 0 && ret == NULL);
    return ret;
}

void initializeCallback() {
    readyList = createNewList();
    sleepingThreadMap = CREATE_MAP(Thread*);
    const char* lockThreadMap = CREATE_MAP(const char*); // lock -> thread
}

void shutdownCallback() {
    destroyList(readyList);
}

int tickSleep(int numTicks) {
    int start_tick, end_tick, cur_tick;
    start_tick = getCurrentTick(); // start tick is the tick when the function is called
    end_tick = start_tick + numTicks; // end tick is calculated

    // find current thread, remove if from ready list,
    // stop executing it, and append it to the end of ready list
    Thread* curThread = getCurrentThread();
    removeFromList(readyList, (void*) curThread);
    stopExecutingThreadForCycle();
    addToListIndex(readyList, listSize(readyList) + 1, (void*) curThread);

    // add <thread, end tick> to sleepingThreadMap
    int* end_tick_ptr = (int*) malloc(sizeof(int));
    &end_tick_ptr = end_tick;
    PUT_IN_MAP(Thread*, sleepingThreadMap, curThread, (void*) end_tick_ptr);

    cur_tick = getCurrentTick(); // keep track of current tick
    // terminate the while loop if current tick has reached end tick
    while (cur_tick < end_tick) {
        cur_tick = getCurrentTick(); // update current tick
    }
    return start_tick;
}

void setMyPriority(int priority) {}
