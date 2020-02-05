#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Map.h"
#include "Thread.h"
#include "thread_lock.h"


const char *readyList = NULL; // stores threads that are not sleeping and ready for execution
const char *sleepList = NULL; // stores threads that are sleeping
const char *sleepThreadMap = NULL; // stores thread -> wakeTick pairs
const char *sharedLockThreadMap = NULL;


// * Compare two threads for sorting. Return true if t1 is considered smaller and false otherwise.
// * Specifically, if t1 is sleeping and should be waken up, it should be put into the head of ready list.
// * Otherwise, if it should still be sleeping, it should be placed at the end of ready list, so it returns false.
// * If t1 and t2 have the same priority, we should compare their original priority.
// * The one with lower original priority should go first because of the purpose of priority donation.
// * If t1 and t2 have different priorities, then the one with higher priority should go first.
// * @param t1 the first thread.
// * @param t2 the second thread.
// * @return true if t1 should go to the front of the ready list and false otherwise.
// */
//bool comparator(void *t1, void *t2);

void updateReadyAndSleepLists();
Thread* findEarliestWakeUpThread();
Thread* findThreadToRun();

Thread *createAndSetThreadToRun(const char *name,
                                void *(*func)(void *),
                                void *arg,
                                int pri) {
    Thread *ret = (Thread *) malloc(sizeof(Thread));
    ret->name = (char *) malloc(strlen(name) + 1);
    strcpy(ret->name, name);
    ret->func = func;
    ret->arg = arg;
    ret->priority = pri;
    ret->originalPriority = pri;

    createThread(ret);
    addToList(readyList, (void *) ret);
    return ret;
}

void destroyThread(Thread *thread) {
    free(thread->name);
    free(thread);
}

Thread *nextThreadToRun(int currentTick) {
    char line[1024];
    if (listSize(readyList) == 0)
        return NULL;
    Thread *ret = NULL;
    updateReadyAndSleepLists();
    do {
        int threadIndex = 0;
        sprintf(line, "[nextThreadToRun] trying thread index %d\n",
                threadIndex);
        verboseLog(line);
//        ret = ((Thread *) listGet(readyList, threadIndex));
//        updateReadyAndSleepLists(); // move threads that should be waken up from sleep list to ready list
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
    sleepThreadMap = CREATE_MAP(Thread * );
    sharedLockThreadMap = CREATE_MAP(const char *); // lock -> thread
}

void shutdownCallback() {
    destroyList(readyList);
}

int tickSleep(int numTicks) {
    int startTick, wakeTick;

    startTick = getCurrentTick(); // start tick is the tick when the function is called
    wakeTick = startTick + numTicks; // wake tick is calculated

    // find current thread
    Thread *curThread = getCurrentThread();

//    stopExecutingThreadForCycle();

    while (getCurrentTick() < wakeTick) {
        stopExecutingThreadForCycle();
    }

    // remove thread from ready list
    removeFromList(readyList, (void *) curThread);
    // add thread to sleep list
    addToList(sleepList, (void *) curThread);
    // add <curThread, wakeTick> to sleepThreadMap
    PUT_IN_MAP(Thread * , sleepThreadMap, curThread, (void *) &wakeTick);
//    stopExecutingThreadForCycle();
//    // stop executing this thread
//    while (getCurrentTick() < wakeTick) {
//        stopExecutingThreadForCycle();
//    }

    return startTick;
}

void setMyPriority(int priority) {
    getCurrentThread()->priority = priority;
}

/**
 * Update ready and sleep lists.
 * Find all the threads in sleep list that should be waken up. Remove them from sleep list and add them to ready list.
 */

void updateReadyAndSleepLists() {
    int sleepCnt = listSize(sleepList);
    Thread* candidate = NULL;
    int* wakeTick = NULL;
    while (sleepCnt > 0) {
        candidate = findEarliestWakeUpThread();
        wakeTick = (int *) GET_FROM_MAP(Thread*, sleepThreadMap, candidate);
        if (*wakeTick <= getCurrentTick()) {
            // if find a thread with wake tick earlier than current tick
            // should wake up this thread
            // remove it from sleep list and sleep map and add it to ready list
            removeFromList(sleepList, (void *) candidate);
            addToList(readyList, (void *) candidate);
            REMOVE_FROM_MAP(Thread *, sleepThreadMap, candidate);
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
    Thread* ret = NULL; // to record the thread with earliest wake tick
    Thread* cur = NULL; // to traverse sleep list
    int* earliestWakeTick = NULL;
    int* curWakeTick = NULL;
    int sleepCnt = listSize(sleepList); // number of threads sleeping
    if (sleepCnt > 0) {
        ret = (Thread *) listGet(sleepList, 0); // get the first thread in sleep list
        // traverse the rest of the list to find earliest thread to wake up
        for (int i = 1; i < sleepCnt; i++) {
            cur = (Thread *) listGet(sleepList, i);
            // if we find a thread with an earlier wakeTick, update ret
            earliestWakeTick = (int *) GET_FROM_MAP(Thread*, sleepThreadMap, ret);
            curWakeTick = (int *) GET_FROM_MAP(Thread*, sleepThreadMap, cur);
            if (*curWakeTick < *earliestWakeTick) {
                ret = cur;
            }
        }
    }
    return ret;
}

/**
 * Find the thread to run from ready list based on its priority. If two threads have the same priority, we run the one
 * with lower original priority since this one just got priority donation and we should run it first to release the
 * lock.
 * @return the thread to run next or NULL if ready list is empty.
 */
Thread* findThreadToRun() {
    Thread* ret = NULL; // to record thread to run
    Thread* cur = NULL; // to traverse ready list
    int readyCnt = listSize(readyList);
    if (readyCnt > 0) {
        ret = (Thread *) listGet(readyList, 0); // get the first thread in ready list
        // traverse the rest of the list to find the thread to run
        for (int i = 1; i < readyCnt; i++) {
            cur = (Thread *) listGet(readyList, i);
            if (cur->priority > ret->priority
                || cur->priority == ret->priority
                   && cur->originalPriority < ret->originalPriority) {
                // if we find a thread with higher priority, or
                // if we find a thread with same priority, but lower original priority, we know that a higher priority
                // thread just donated priority to this thread in order to have the lock released, so we should run this
                // thread instead
                // update ret
                ret = cur;
            }
        }
    }
    return ret;
}

//bool comparator(void *t1, void *t2) {
//    Thread *thread1 = (Thread *) t1;
//    Thread *thread2 = (Thread *) t2;
//    // check whether thread 1 is sleeping or not
//    bool t1Sleeping = MAP_CONTAINS(Thread*, sleepThreadMap, thread1);
//    if (t1Sleeping) {
//        // if t1 is sleeping, check whether it should be waken up
//        int* endTickPtr = (int*)GET_FROM_MAP(Thread*, sleepThreadMap, thread1);
//        if (*endTickPtr == getCurrentTick()) {
//            // remove t1 from sleeping map and put it in front of list
//            REMOVE_FROM_MAP(Thread*, sleepThreadMap, thread1);
//            return true;
//        } else {
//            // put it at end of list
//            return false;
//        }
//    } else {
//        if (thread1->priority != thread2->priority) {
//            // if t1 and t2 have different priorities, put the one with higher priority closer to head
//            return thread1->priority > thread2->priority;
//        } else {
//            // if t1 and t2 have the same priority, put the one with lower original priority close to head
//            // for the purpose of priority donation
//            return thread1->originalPriority < thread2->originalPriority;
//        }
//    }
//}
