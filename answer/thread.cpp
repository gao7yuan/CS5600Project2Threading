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
const char* sleepList = NULL;             // stores threads that are sleeping
const char* sleepThreadMap = NULL;        // stores thread -> wakeTick pairs
const char* sharedLockThreadMap = NULL;   // stores lock -> thread pairs
const char* sharedLockAttemptMap = NULL;  // stores thread -> attempt lock pairs

/*
 * Function prototypes
 */

// move threads in sleep list that are supposed to wake up to ready list
void updateReadyAndSleepLists();

// from sleep list, find the thread with smallest wakeTick
Thread* findEarliestWakeUpThread();

// from ready list, find the next thread to run considering their priorities
Thread* findThreadToRun();

// given a thread, insert it to ready list, making ready list a priority queue
// make sure it inserts after the last thread with same priority
void insertToReadyList(Thread*);



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
    //    addToList(readyList, (void*)ret);
    insertToReadyList(ret);
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
    sprintf(line, "[nextThreadToRun] current tick is %d", getCurrentTick());
    sprintf(line, "[nextThreadToRun] current ready list size is %d", listSize(readyList));
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
    sharedLockAttemptMap = CREATE_MAP(Thread*);     // thread -> attempt lock
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

    //    // stop executing until reach wake tick
    //    while (getCurrentTick() < wakeTick) {
    //        stopExecutingThreadForCycle();
    //    }

    // remove thread from ready list
    removeFromList(readyList, (void*)curThread);
    // add thread to sleep list
    addToList(sleepList, (void*)curThread);
    // add <curThread, wakeTick> to sleepThreadMap
    PUT_IN_MAP(Thread*, sleepThreadMap, curThread, (void*)&wakeTick);

    // stop executing
    stopExecutingThreadForCycle();

    return startTick;
}

void setMyPriority(int priority) {
    getCurrentThread()->priority = priority;
}

/**
 * Given a thread, insert this thread into ready list in desired order, making
 * ready list a priority queue, while keeping the order of insertion if two
 * threads have the same priority.
 * @param thread
 */

void insertToReadyList(Thread* thread) {
    int threadIndex = 0;
    Thread* curThread = NULL;
    while (threadIndex < listSize(readyList)) {
        curThread = (Thread*)listGet(readyList, threadIndex);
        if (thread->priority > curThread->priority) {
            break;
        }
        threadIndex++;
    }
    addToListAtIndex(readyList, threadIndex, (void*)thread);
}

/**
 * Update ready and sleep lists.
 * Find all the threads in sleep list that should be waken up. Remove them from
 * sleep list and add them to ready list.
 */

void updateReadyAndSleepLists() {
    char buffer[1024];
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
            sprintf(buffer, "[update ready list] waking up thread %s", candidate->name);
            //            addToList(readyList, (void*)candidate);
            insertToReadyList(candidate);
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
    if (listSize(readyList) <= 0) {
        return ret;
    }
    ret = (Thread*)listGet(readyList, 0);  // get first thread in ready list
    // if this thread has attempted a lock
    bool isAttemptingLock = MAP_CONTAINS(Thread*, sharedLockAttemptMap, ret);
    if (isAttemptingLock) {
        const char* attemptLock =
            (const char*)GET_FROM_MAP(Thread*, sharedLockAttemptMap, ret);
        // if the lock of attempting is held by another thread, find the lock
        // holder
        bool isHeld = MAP_CONTAINS(const char*, sharedLockThreadMap, attemptLock);
        if (isHeld) {
            Thread* lockHolder =
                (Thread*)GET_FROM_MAP(const char*, sharedLockThreadMap, attemptLock);
            // if lock holder has a lower priority, do priority donation
            if (lockHolder != NULL && lockHolder != ret &&
                lockHolder->priority < ret->priority) {
                lockHolder->priority = ret->priority;  // priority donation
                ret = lockHolder;  // next tick let's run this lock holder
            }
        }
    }
    // for round-robin
    removeFromList(readyList, (void*)ret);
    insertToReadyList(ret);

    return ret;
}
