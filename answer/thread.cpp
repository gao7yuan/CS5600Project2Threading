#include <Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "List.h"
#include "Map.h"
#include "Thread.h"
#include "thread_lock.h"


const char *readyList = NULL;
const char *sleepingThreadMap = NULL;
const char *lockThreadMap = NULL;

/**
 * Compare two threads for sorting. Return true if t1 is considered smaller and false otherwise.
 * Specifically, if t1 is sleeping and should be waken up, it should be put into the head of ready list.
 * Otherwise, if it should still be sleeping, it should be placed at the end of ready list, so it returns false.
 * If t1 and t2 have the same priority, we should compare their original priority.
 * The one with lower original priority should go first because of the purpose of priority donation.
 * If t1 and t2 have different priorities, then the one with higher priority should go first.
 * @param t1 the first thread.
 * @param t2 the second thread.
 * @return true if t1 should go to the front of the ready list and false otherwise.
 */
bool comparator(void *t1, void *t2);

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
    sortList(readyList, comparator);
    do {
        int threadIndex = 0;
        sprintf(line, "[nextThreadToRun] trying thread index %d\n",
                threadIndex);
        verboseLog(line);
        ret = ((Thread *) listGet(readyList, threadIndex));
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
    sleepingThreadMap = CREATE_MAP(Thread * );
    lockThreadMap = CREATE_MAP(
    const char*); // lock -> thread
}

void shutdownCallback() {
    destroyList(readyList);
}

int tickSleep(int numTicks) {
    int startTick, endTick;
    startTick = getCurrentTick(); // start tick is the tick when the function is called
    endTick = startTick + numTicks; // end tick is calculated

    // find current thread
    Thread *curThread = getCurrentThread();

    // add <thread, end tick> to sleepingThreadMap
    PUT_IN_MAP(Thread * , sleepingThreadMap, curThread, (void *) &endTick);
    // terminate the while loop if current tick has reached end tick
    while (getCurrentTick() < endTick) {
        stopExecutingThreadForCycle();
    }
    return startTick;
}

void setMyPriority(int priority) {
    getCurrentThread()->priority = priority;
}

bool comparator(void *t1, void *t2) {
    Thread *thread1 = (Thread *) t1;
    Thread *thread2 = (Thread *) t2;
    // check whether thread 1 is sleeping or not
    bool t1Sleeping = MAP_CONTAINS(Thread*, sleepingThreadMap, thread1);
    if (t1Sleeping) {
        // if t1 is sleeping, check whether it should be waken up
        int* endTickPtr = (int*)GET_FROM_MAP(Thread*, sleepingThreadMap, thread1);
        if (*endTickPtr == getCurrentTick()) {
            // remove t1 from sleeping map and put it in front of list
            REMOVE_FROM_MAP(Thread*, sleepingThreadMap, thread1);
            return true;
        } else {
            // put it at end of list
            return false;
        }
    } else {
        if (thread1->priority != thread2->priority) {
            // if t1 and t2 have different priorities, put the one with higher priority closer to head
            return thread1->priority > thread2->priority;
        } else {
            // if t1 and t2 have the same priority, put the one with lower original priority close to head
            // for the purpose of priority donation
            return thread1->originalPriority < thread2->originalPriority;
        }
    }
}
