#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Map.h"
#include "Thread.h"
#include "thread_lock.h"

const char* readyList =
        NULL;  // stores threads that are not sleeping and ready for execution
const char* sleepList = NULL;            // stores threads that are sleeping
const char* sleepThreadMap = NULL;       // stores thread -> wakeTick pairs
const char* sharedLockThreadMap = NULL;  // stores lock -> thread pairs

/*
 * Function prototypes
 */

// move threads in sleep list that are supposed to wake up to ready list
void updateReadyAndSleepLists();
// from sleep list, find the thread with smallest wakeTick
Thread* findEarliestWakeUpThread();
// from ready list, find the next thread to run considering their priorities
Thread* findThreadToRun();

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
    char line[1024];
    sprintf(line, "[destroyThread] destroying thread with name %s\n",
            thread->name);
    verboseLog(line);
    free(thread->name);
    free(thread);
}

Thread* nextThreadToRun(int currentTick) {
    char line[1024];
    if (listSize(readyList) == 0)
        return NULL;
    Thread* ret = NULL;
    // move threads in sleep list that are supposed to be waken up to ready list
    updateReadyAndSleepLists();
    do {
        int threadIndex = 0;
        sprintf(line, "[nextThreadToRun] trying thread index %d\n",
                threadIndex);
        verboseLog(line);
        // find next thread to run from ready list according to certain ordering
        // of priority
        ret = findThreadToRun();
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
    sleepList = createNewList();
    sleepThreadMap = CREATE_MAP(Thread*);
    sharedLockThreadMap = CREATE_MAP(const char*);  // lock -> thread
}

void shutdownCallback() {
    destroyList(readyList);
    destroyList(sleepList);
}

int tickSleep(int numTicks) {
    int startTick, wakeTick;

    startTick =
            getCurrentTick();  // start tick is the tick when the function is called
    wakeTick = startTick + numTicks;  // wake tick is calculated

    // find current thread
    Thread* curThread = getCurrentThread();

    // stop executing until reach wake tick
    while (getCurrentTick() < wakeTick) {
        stopExecutingThreadForCycle();
    }

    // remove thread from ready list
    removeFromList(readyList, (void*)curThread);
    // add thread to sleep list
    addToList(sleepList, (void*)curThread);
    // add <curThread, wakeTick> to sleepThreadMap
    PUT_IN_MAP(Thread*, sleepThreadMap, curThread, (void*)&wakeTick);

    return startTick;
}

void setMyPriority(int priority) {
    getCurrentThread()->priority = priority;
}

/**
 * Update ready and sleep lists.
 * Find all the threads in sleep list that should be waken up. Remove them from
 * sleep list and add them to ready list.
 */

void updateReadyAndSleepLists() {
    int sleepCnt = listSize(sleepList);
    Thread* candidate = NULL;
    int* wakeTick = NULL;
    while (sleepCnt > 0) {
        candidate = findEarliestWakeUpThread();
        wakeTick = (int*)GET_FROM_MAP(Thread*, sleepThreadMap, candidate);
        if (*wakeTick <= getCurrentTick()) {
            // if find a thread with wake tick earlier than current tick
            // should wake up this thread
            // remove it from sleep list and sleep map and add it to ready list
            removeFromList(sleepList, (void*)candidate);
            addToList(readyList, (void*)candidate);
            REMOVE_FROM_MAP(Thread*, sleepThreadMap, candidate);
            sleepCnt--;
        } else {
            // if no thread should be waken up
            break;
        }
    }
}

/**
 * From sleepList, find the thread with the earliest wakeTick.
 * @return the thread with the earliest wakeTick or NULL if sleepList is empty.
 */

Thread* findEarliestWakeUpThread() {
    Thread* ret = NULL;  // to record the thread with earliest wake tick
    Thread* cur = NULL;  // to traverse sleep list
    int* earliestWakeTick = NULL;
    int* curWakeTick = NULL;
    int sleepCnt = listSize(sleepList);  // number of threads sleeping
    if (sleepCnt > 0) {
        ret = (Thread*)listGet(sleepList,
                               0);  // get the first thread in sleep list
        earliestWakeTick = (int*)GET_FROM_MAP(Thread*, sleepThreadMap, ret);
        // traverse the rest of the list to find earliest thread to wake up
        for (int i = 1; i < sleepCnt; i++) {
            cur = (Thread*)listGet(sleepList, i);
            curWakeTick = (int*)GET_FROM_MAP(Thread*, sleepThreadMap, cur);
            // if we find a thread with an earlier wakeTick, update ret
            if (*curWakeTick < *earliestWakeTick) {
                ret = cur;
                earliestWakeTick = curWakeTick;
            }
        }
    }
    return ret;
}

/**
 * Find the thread to run from ready list based on its priority. If two threads
 * have the same priority, we run the one with lower original priority since
 * this one just got priority donation and we should run it first to release the
 * lock.
 * @return the thread to run next or NULL if ready list is empty.
 */

Thread* findThreadToRun() {
    Thread* ret = NULL;  // to record thread to run
    Thread* cur = NULL;  // to traverse ready list
    int readyCnt = listSize(readyList);
    if (readyCnt > 0) {
        ret = (Thread*)listGet(readyList,
                               0);  // get the first thread in ready list
        // traverse the rest of the list to find the thread to run
        for (int i = 1; i < readyCnt; i++) {
            cur = (Thread*)listGet(readyList, i);
            if (cur->priority > ret->priority ||
                cur->priority == ret->priority &&
                cur->originalPriority < ret->originalPriority) {
                // if we find a thread with higher priority, or
                // if we find a thread with same priority, but lower original
                // priority, we know that a higher priority thread just donated
                // priority to this thread in order to have the lock released,
                // so we should run this thread instead update ret
                ret = cur;
            }
        }
    }
    return ret;
}
