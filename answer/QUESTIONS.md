# Project 2 Questions

## Scheduling

**Two tests have infinite loops with the scheduler you were provided. Explain
why `Running.PriorityOrder` will never terminate with the basic scheduler.**

- Function `nextThreadToRun()` is called by the simulator every tick to get the next thread to run.
- Currently, `readyList` is simply a FIFO queue, and `nextThreadToRun()` simply looks for the first non-terminated
thread in the queue.
- When function `createAndSetThreadToRun()` is called, a new thread is created and added to `readyList`.
- Currently, without priority scheduling and priority donation implemented, according to `TEST(Running, PriorityOrder)`,
`NAME_LO_PRI` will be added to the readyList first and executed every tick until it is finished. However, when
executing function `recordThreadPriority()`, this thread will be stuck forever in the while loop, since `NAME_HI_PRI` 
will never have a chance to be added to new threads and thus `infoList->cont` will always be `false`.
- As a result, this function never terminates.

## Sleep

**Briefly describe what happens in your `sleep(...)` function.**

1. Record the tick when the function is called as the tick that the sleep is supposed to start - `startTick`.
2. Calculate the tick that the sleep is going to end - `wakeTick`.
3. Update related data structures. Specifically, remove `curThread` from `readyList`, add `curThread -> wakeTick`
pairs into `sleepThreadMap`, and add `curThread` to `sleepList`.
4. Call `stopExecutingThreadForCycle()` so that the currently running thread yields.
5. Return `startTick`.

**How does your `sleep(...)` function guarantee duration?**

- A map `sleepThreadMap` is kept to record `wakeTick` for each thread that is sleeping. The threads are woken up
only when current tick has reached `wakeTick`.

What prevents a thread from waking up too early?
- The threads that are supposed to sleep are removed from `readyList` and kept in `sleepList`. They are never moved
back to `readyList` until they have reached their `wakeTick`, which we can check from `sleepThreadMap`.

What prevents a thread from _never_ waking up?
- Every tick, when we call `nextThreadToRun()`, we check the threads with earliest `wakeTick` in `sleepList`. The 
threads that are supposed to be woken up are removed from `sleepList` and `sleepThreadMap` and added back to 
`readyList`.

**Do you have to check every sleeping thread every tick? Why not?**

- No. `sleepList` is essentially a priority queue. Each tick, we check the threads in `sleepList` with smallest
`wakeTick` and see if they should be waken up. If we see a thread that is not ready to be woken up yet, we know that 
the rest of the threads in `sleepList` are not ready either. Therefore, we don't need to check the rest of `sleepList`.

## Locking

**How do you ensure that the highest priority thread waiting for a lock gets it
first?**

- `readyList` is essentially a priority queue. When looking for the next thread to run, we look for the thread with
highest priority to make sure that it gets run next.
- If it attempts a lock and the lock is held by a lower-priority thread, we do priority donation in `nextThreadToRun()` 
(actually, in the helper function `findThreadToRun()`) and set the priority of the lower-priority thread the same as the 
high-priority thread, and we decide to run this lower-priority thread for the next tick.
- Eventually, after the lock is released by the lower-priority thread, we perform priority inversion on the 
lower-priority thread in `lockReleased()` so that the high-priority thread waiting for the lock will eventually get the 
lock.

## Priority Donation

**Describe the sequence of events before and during a priority donation.**

1. When a thread tries to get the lock, firstly `lockAttempted()` is called.
2. Inside `lockAttempted()`, we put information of `thread -> lock attempted` to `sharedLockAttemptMap` so that the
scheduler can track who needs any lock.
3. When executing `nextThreadToRun()`, we check the first thread in `readyList`. If this high-priority thread is now
attempting a lock which is held by a low-priority thread, the scheduler does priority donation, aka. raise the priority
of the low-priority thread to the same level as the high-priority thread.
4. Then, the scheduler decides to run this low-priority thread first. Eventually, the lock will be released by the
low-priority thread.
5. Now the scheduler does priority inversion and this low-priority thread will eventually return to its original
location in `readyList`.
6. Since the high-priority thread has the highest priority among all threads, it acquires the lock and gets running.

**How is nested donation handled?**

Describe a system of three threads (A, B, C) and three locks (1, 2, 3).

Say the priorities of A, B, C are 1, 5, 9, respectively. A is holding lock 1, B is holding lock 2 waiting for lock 1,
and C is holding lock 3 waiting for lock 2.

1. C runs first and tries to get lock 2.
2. Scheduler finds that B is holding lock 2 and has a lower priority, so scheduler makes C donate priority to B and B's 
priority becomes 9.
3. B runs and tries to get lock 1.
4. Scheduler finds that A is holding lock 1 and has a lower priority. So scheduler makes B donate priority to A and A's 
priority becomes 9 as well.
5. A runs and eventually releases lock 1. Priority inversion is performed on A, so A's priority drops back to 1.
6. B gets lock 1 and runs. B eventually releases lock 1 and lock 2, and its priority drops back to 5.
7. C gets lock 2 and runs. C is eventually terminated and releases lock 2 and lock 3.

**How do you ensure that the highest priority thread waiting is called when the
lock is released?**

- `readyList` is essentially a priority queue. When looking for the next thread to run, we look for the thread with
highest priority to make sure that it gets run next and gets the lock.
- If the lock is held by a lower-priority thread, we do priority donation and set the priority of the low-priority 
thread the same as the high-priority thread.
- We then decide to run this lower-priority thread. We also remove this thread out of `readyList` and re-insert it into
`readyList`, so that if the lock is not released, its position would be the last one of the highest-priority thread.
- The next tick, we run the high-priority thread, and since it needs a lock that is held by another thread, CPU does
nothing for now and this high-priority thread will be placed just behind the low-priority thread that is holding the 
lock.
- This will happen until eventually the lock is released. Since the high-priority thread is just behind the low-priority
thread that is holding the lock, after the lock is released, we perform priority inversion on the low-priority thread 
in `lockReleased()` and the high-priority thread waiting for the lock jumps to the head of `readyList` and will get 
the lock.

## For Bonus Points
- Refer to the comment for function `void stopExecutingThreadForCycle();` would increment score by one :)
- A screen shot of destroying a thread is located in this folder, `destroyThread.png`.
- Mention the lowest priority for an equal value added to your final marks -> the lowest priority value is 1.
- Deleted comments in `thread_lock.h` and unnecessary parts in `QUESTIONS.md`.
