#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Map.h"
#include "Thread.h"
#include "thread_lock.h"

/*
 * Data structures
 */

const char* readyList = NULL; // stores threads that are not sleeping and ready for execution
const char* sleepList = NULL; // stores threads that are sleeping
const char* sleepThreadMap = NULL; // stores [thread -> wakeTick] pairs
const char* sharedLockThreadMap = NULL; // stores [lock -> lock-holder thread] pairs
const char* sharedLockAttemptMap = NULL; // stores [thread -> attempt lock] pairs

/*
 * Function prototypes for helper functions
 */

/**
 * Given a thread, insert this thread into ready list in desired order, making
 * ready list a priority queue, while keeping the order of insertion if two
 * threads have the same priority.
 * @param thread - thread to insert to ready list.
 */
void insertToReadyList(Thread* thread);

/**
 * Given a thread, insert it into sleep list, making thread with earlier wake tick closer to head.
 * If two threads have the same wake tick, the one with higher priority goes first.
 * @param thread - thread to insert into sleep list.
 */
void insertToSleepList(Thread* thread);

/**
 * Update ready and sleep lists.
 * Find all the threads in sleep list that should be woken up. Remove them from
 * sleep list and sleep thread map and add them to ready list.
 */
void updateReadyAndSleepLists();

/**
 * Find the thread to run from ready list based on priority. Re-insert the thread to ready list for
 * realization of Round-Robin.
 * @return the thread to run next or NULL if ready list is empty.
 */
Thread* findThreadToRun();

/*
 * Implementations for functions defined in Thread.student.h
 */

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
    // insert created thread to ready list, making ready list a priority queue
    insertToReadyList(ret);
    return ret;
}

void destroyThread(Thread* thread) {
    char line[1024];
    sprintf(line, "[destroyThread] destroying thread with name %s\n", thread->name);
    verboseLog(line);
    free(thread->name);
    free(thread);
}

Thread* nextThreadToRun(int currentTick) {
    char line[1024];

    sprintf(line, "[nextThreadToRun] current tick is %d\n", getCurrentTick());
    verboseLog(line);
    sprintf(line, "[nextThreadToRun] current ready list size is %d\n", listSize(readyList));
    verboseLog(line);

    // move threads in sleep list that are supposed to be woken up to ready list
    updateReadyAndSleepLists();

    if (listSize(readyList) == 0)
        return NULL;
    Thread* ret = NULL;

    do {
        // find next thread to run from ready list
        ret = findThreadToRun();
        if (ret->state == TERMINATED) {
            sprintf(line, "[nextThreadToRun] thread with name %s was terminated\n",
                    ret->name);
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
    sleepThreadMap = CREATE_MAP(Thread*); // [thread -> wake tick]
    sharedLockThreadMap = CREATE_MAP(const char*); // [lock -> lock-holder thread]
    sharedLockAttemptMap = CREATE_MAP(Thread*); // [thread -> attempt lock]
}

void shutdownCallback() {
    destroyList(readyList);
    destroyList(sleepList);
}

int tickSleep(int numTicks) {
    int startTick, wakeTick;

    startTick = getCurrentTick(); // start tick is the tick when the function is called
    wakeTick = startTick + numTicks; // wake tick is calculated

    // find current thread
    Thread* curThread = getCurrentThread();

    // remove thread from ready list
    removeFromList(readyList, (void*)curThread);
    // add [curThread, wakeTick] to sleepThreadMap
    PUT_IN_MAP(Thread*, sleepThreadMap, curThread, (void*)&wakeTick);
    // add thread to sleep list
    insertToSleepList(curThread);

    // stop executing
    stopExecutingThreadForCycle();

    return startTick;
}

void setMyPriority(int priority) {
    getCurrentThread()->priority = priority;
}

/*
 * Helper functions
 */

void insertToReadyList(Thread* thread) {
    int threadIndex = 0;
    Thread* curThread = NULL; // to iterate ready list
    // insert thread to the first thread in ready list that has a smaller priority than it
    // this ensures threads with equal priorities are run in the order of insertion
    while (threadIndex < listSize(readyList)) {
        curThread = (Thread*)listGet(readyList, threadIndex);
        if (thread->priority > curThread->priority) {
            break;
        }
        threadIndex++;
    }
    addToListAtIndex(readyList, threadIndex, (void*)thread);
}

void insertToSleepList(Thread * thread) {
    int * wakeTick = (int *) GET_FROM_MAP(Thread *, sleepThreadMap, thread);
    int threadIndex = 0;
    Thread *curThread = NULL; // to iterate sleep list
    int *curWakeTick = NULL;
    // insert thread before the first thread in sleep list with later wake tick
    // or with equal wake tick but lower priority
    while (threadIndex < listSize(sleepList)) {
        curThread = (Thread *) listGet(sleepList, threadIndex);
        curWakeTick = (int*) GET_FROM_MAP(Thread *, sleepThreadMap, curThread);
        if (*wakeTick < *curWakeTick || *wakeTick == *curWakeTick && thread->priority > curThread->priority) {
            break;
        }
        threadIndex++;
    }
    addToListAtIndex(sleepList, threadIndex, (void *) thread);
}

void updateReadyAndSleepLists() {
    int sleepCnt = listSize(sleepList);
    Thread* candidate = NULL;
    int* wakeTick = NULL;
    // always check the first thread in sleep list
    // if wake tick smaller than current tick, remove from sleep list and sleep map and add to ready list
    // otherwise break
    while (sleepCnt > 0) {
        candidate = (Thread *) listGet(sleepList, 0);
        wakeTick = (int*)GET_FROM_MAP(Thread*, sleepThreadMap, candidate);
        if (*wakeTick <= getCurrentTick()) {
            // if find a thread with wake tick earlier than current tick
            // should wake up this thread
            // remove it from sleep list and sleep map and add it to ready list
            removeFromList(sleepList, (void*)candidate);
            insertToReadyList(candidate);
            REMOVE_FROM_MAP(Thread*, sleepThreadMap, candidate);
            sleepCnt--;
        } else {
            // if no thread should be woken up
            break;
        }
    }
}

Thread* findThreadToRun() {
    Thread* ret = NULL;  // to record thread to run
    if (listSize(readyList) <= 0) {
        return ret;
    }
    ret = (Thread*)listGet(readyList, 0); // get first thread in ready list
    // if this thread has attempted a lock
    bool isAttemptingLock = MAP_CONTAINS(Thread*, sharedLockAttemptMap, ret);
    if (isAttemptingLock) {
        const char* attemptLock =
                (const char*)GET_FROM_MAP(Thread*, sharedLockAttemptMap, ret);
        // if the lock of attempting is held by another thread, find the lock-holder
        bool isHeld = MAP_CONTAINS(const char*, sharedLockThreadMap, attemptLock);
        if (isHeld) {
            Thread* lockHolder =
                    (Thread*)GET_FROM_MAP(const char*, sharedLockThreadMap, attemptLock);
            // if lock holder has a lower priority, do priority donation
            if (lockHolder != NULL && lockHolder != ret &&
                lockHolder->priority < ret->priority) {
                lockHolder->priority = ret->priority;  // priority donation
                ret = lockHolder; // next tick let's run this lock holder
            }
        }
    }
    // for round-robin, remove the next thread to run from ready list and re-insert
    removeFromList(readyList, (void*)ret);
    insertToReadyList(ret);

    return ret;
}