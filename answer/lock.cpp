#include "Lock.h"
#include <stddef.h>
#include <string.h>
#include "Map.h"
#include "thread_lock.h"

#pragma region Lock Callbacks

void lockCreated(const char* lockId) {
    // initialize, inform this lock exists and no one uses it
    PUT_IN_MAP(const char*, sharedLockThreadMap, lockId, NULL);
}

void lockAttempted(const char* lockId, Thread* thread) {
    // inform scheduler that this thread has attempted this lock
    // store [thread -> attempt lock] pairs into sharedLockAttemptMap for
    // scheduler to track
    PUT_IN_MAP(Thread*, sharedLockAttemptMap, thread, (void*)lockId);
}

void lockAcquired(const char* lockId, Thread* thread) {
    // if this thread has attempted this lock before, remove the record from
    // sharedLockAttemptMap
    bool isAttemptingLock = MAP_CONTAINS(Thread*, sharedLockAttemptMap, thread);
    if (isAttemptingLock) {
        char* attemptedLockId =
            (char*)GET_FROM_MAP(Thread*, sharedLockAttemptMap, thread);
        if (strcmp(lockId, attemptedLockId) == 0) {
            REMOVE_FROM_MAP(Thread*, sharedLockAttemptMap, thread);
        }
    }
    // update sharedLockThreadMap with this thread
    PUT_IN_MAP(const char*, sharedLockThreadMap, lockId, (void*)thread);
}

void lockFailed(const char* lockId, Thread* thread) {
    // inform that this lock is gone
    REMOVE_FROM_MAP(const char*, sharedLockThreadMap, lockId);
    // this thread is not attempting this lock any more
    bool isAttemptingLock = MAP_CONTAINS(Thread*, sharedLockAttemptMap, thread);
    if (isAttemptingLock) {
        char* attemptedLockId =
            (char*)GET_FROM_MAP(Thread*, sharedLockAttemptMap, thread);
        if (strcmp(lockId, attemptedLockId) == 0) {
            REMOVE_FROM_MAP(Thread*, sharedLockAttemptMap, thread);
        }
    }
}

void lockReleased(const char* lockId, Thread* thread) {
    // priority inversion
    thread->priority = thread->originalPriority;
    // restore initial value for lock in sharedLockThreadMap
    PUT_IN_MAP(const char*, sharedLockThreadMap, lockId, NULL);
}

#pragma endregion

#pragma region Lock Functions

Thread* getThreadHoldingLock(const char* lockId) {
    return (Thread*)GET_FROM_MAP(const char*, sharedLockThreadMap, lockId);
}

#pragma endregion