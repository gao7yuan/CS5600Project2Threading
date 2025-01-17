#ifndef _THREAD_LOCK_H
#define _THREAD_LOCK_H

extern const char*
    sharedLockThreadMap;  // stores [lock -> lock-holder thread] pairs
extern const char*
    sharedLockAttemptMap;  // stores [thread -> attempt lock] pairs

#endif