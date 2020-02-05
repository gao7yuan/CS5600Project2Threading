#include "Lock.h"
#include <stddef.h>
#include "thread_lock.h"
#include "Map.h"

// See os_simulator/includes/Lock.student.h for explanations of these functions
// Not all of these functions must be implemented
// Remember to delete these three comments before grading for full marks

#pragma region Lock Callbacks

void lockCreated(const char *lockId) {
    PUT_IN_MAP(const char*, lockThreadMap, lockId, NULL); // initialize, inform this lock exists and no one uses it
}

void lockAttempted(const char *lockId, Thread *thread) {
    Thread* lockHolder = getThreadHoldingLock(lockId);
    if (lockHolder != NULL && lockHolder->priority < thread->priority) {
        lockHolder->priority = thread->priority;
    }
}

void lockAcquired(const char *lockId, Thread *thread) {
    PUT_IN_MAP(const char*, lockThreadMap, lockId, (void*) thread);
}

void lockFailed(const char *lockId, Thread *thread) {}

void lockReleased(const char *lockId, Thread *thread) {
    PUT_IN_MAP(const char*, lockThreadMap, lockId, NULL);
}

#pragma endregion

#pragma region Lock Functions

Thread *getThreadHoldingLock(const char *lockId) {
    Thread *ret = (Thread*)GET_FROM_MAP(const char*, lockThreadMap, lockId);
    return ret;
}

#pragma endregion